"""Background worker that drives one telemetry link.

Owns the receive loop on its own daemon thread: poll the link → decode into the
store → maintain link-health stats → emit a ~1 Hz GCS heartbeat so the vehicle
sees a station. All cross-thread output goes through the store (telemetry) and
an injected notice sink (messages); the worker never imports Qt.

Mirrors the firmware's ``LinkManager`` + ``LinkTask`` responsibilities.
"""
from __future__ import annotations

import threading
from typing import Callable, Optional

from ..domain.telemetry import Severity, StatusText, TelemetrySnapshot, _now_ms
from ..interfaces.ports import ITelemetryLink
from ..mavlink.decoder import TelemetryDecoder

NoticeSink = Callable[[StatusText], None]

_LINK_TIMEOUT_MS = 3000   # no heartbeat for this long ⇒ link considered down
_HEARTBEAT_MS = 1000      # our own GCS heartbeat cadence
_RECV_TIMEOUT_S = 0.2     # recv poll granularity (keeps stop() responsive)


class LinkManager:
    def __init__(self, store, notice_sink: NoticeSink):
        self._store = store
        self._notice = notice_sink
        self._decoder = TelemetryDecoder(store, on_notice=notice_sink)
        self._link: Optional[ITelemetryLink] = None
        self._thread: Optional[threading.Thread] = None
        self._stop = threading.Event()

    @property
    def link(self) -> Optional[ITelemetryLink]:
        """The active link (also the command sink), or ``None`` if stopped."""
        return self._link

    def start(self, link: ITelemetryLink) -> None:
        self.stop()
        self._link = link
        self._stop.clear()
        self._thread = threading.Thread(target=self._run, name="link-worker", daemon=True)
        self._thread.start()

    def stop(self) -> None:
        self._stop.set()
        thread, self._thread = self._thread, None
        if thread and thread.is_alive() and thread is not threading.current_thread():
            thread.join(timeout=2.0)
        link, self._link = self._link, None
        if link is not None:
            link.close()
        self._store.mutate(lambda s: setattr(s.link, "link_up", False))

    # ── worker thread ────────────────────────────────────────────────────────
    def _run(self) -> None:
        link = self._link
        assert link is not None
        try:
            link.open()
        except Exception as exc:
            self._notice(StatusText(Severity.ERROR, f"Open {link.source_name} failed: {exc}",
                                    _now_ms(), True))
            return
        self._notice(StatusText(Severity.NOTICE, f"Link opened: {link.source_name}",
                                _now_ms(), True))

        frames = good_bytes = errors = 0
        last_hb_sent = 0
        last_frame_ms = 0
        announced = False
        self._set_source(link.source_name)

        while not self._stop.is_set():
            now = _now_ms()
            if now - last_hb_sent >= _HEARTBEAT_MS:
                try:
                    link.send_heartbeat()
                except Exception:
                    pass
                last_hb_sent = now

            try:
                msg = link.recv(_RECV_TIMEOUT_S)
            except Exception as exc:
                self._notice(StatusText(Severity.ERROR, f"Link error: {exc}", now, True))
                break

            now = _now_ms()
            if msg is not None:
                frames += 1
                last_frame_ms = now
                buf = getattr(msg, "get_msgbuf", None)
                if buf is not None:
                    try:
                        good_bytes += len(buf())
                    except Exception:
                        pass
                try:
                    self._decoder.handle(msg)
                except Exception:
                    errors += 1
                if not announced and link.target[0] != 0:
                    announced = True
                    sysid, _ = link.target
                    self._notice(StatusText(Severity.NOTICE,
                                            f"Vehicle detected: system {sysid}", now, True))
            self._write_stats(frames, good_bytes, errors, last_frame_ms, now)

        self._store.mutate(lambda s: setattr(s.link, "link_up", False))

    # ── stats helpers ──────────────────────────────────────────────────────
    def _write_stats(self, frames: int, good_bytes: int, errors: int,
                     last_frame_ms: int, now: int) -> None:
        def mut(s: TelemetrySnapshot) -> None:
            s.link.frames_received = frames
            s.link.bytes_received = good_bytes
            s.link.parse_errors = errors
            s.link.last_frame_ms = last_frame_ms
            # link_up follows heartbeat freshness, not just any frame.
            s.link.link_up = (s.heartbeat_seen and
                              (now - s.last_heartbeat_ms) < _LINK_TIMEOUT_MS)
        self._store.mutate(mut)

    def _set_source(self, name: str) -> None:
        self._store.mutate(lambda s: setattr(s.link, "source_name", name))
