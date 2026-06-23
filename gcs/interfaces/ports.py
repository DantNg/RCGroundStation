"""Port interfaces split by responsibility (Interface Segregation).

``ITelemetryLink`` is the *read* side — open a link and pull decoded MAVLink
messages off it. ``ICommandSink`` is the *write* side — push commands to the
vehicle. A concrete adapter may implement both, but a reader (the link worker)
need not depend on the command methods, and the command service need not depend
on the receive loop.
"""
from __future__ import annotations

from abc import ABC, abstractmethod
from typing import Any, Optional, Tuple


class ITelemetryLink(ABC):
    """Read side: a connection that yields decoded MAVLink messages."""

    @property
    @abstractmethod
    def is_open(self) -> bool:
        ...

    @abstractmethod
    def open(self) -> None:
        """Open the underlying transport. Raises on failure."""

    @abstractmethod
    def close(self) -> None:
        """Close the transport. Idempotent."""

    @abstractmethod
    def recv(self, timeout: float) -> Optional[Any]:
        """Block up to ``timeout`` seconds for the next message.

        Returns a parsed MAVLink message (duck-typed: it has ``get_type()`` and
        named fields) or ``None`` on timeout. Raises on a hard link error.
        """

    @property
    @abstractmethod
    def source_name(self) -> str:
        """Short human label for the link (e.g. ``"COM5"`` or ``"udp:14550"``)."""

    @property
    @abstractmethod
    def target(self) -> Tuple[int, int]:
        """``(system, component)`` of the vehicle, or ``(0, 0)`` until learned."""

    def request_data_streams(self, rate_hz: int = 12) -> None:
        """Ask the vehicle to stream telemetry (ATTITUDE, position, status…).

        ArduPilot/PX4 only send many messages — notably ``ATTITUDE`` (the HUD
        horizon) — when a GCS requests the stream. SITL and the bundled simulator
        stream unconditionally, so this is a no-op there; real links need it.
        Default is a no-op for transports that can't request streams.
        """


class ICommandSink(ABC):
    """Write side: low-level outgoing MAVLink commands to the active target.

    Deliberately MAVLink-shaped but vehicle-agnostic — it knows *how to send*,
    not *what action* a command represents. High-level policy (arm/disarm, mode
    names) lives in ``gcs.mavlink.command_service.CommandService``.
    """

    @abstractmethod
    def command_long(self, command: int, *params: float, confirmation: int = 0) -> None:
        """Send a COMMAND_LONG with up to 7 float params to the target."""

    @abstractmethod
    def set_mode(self, base_mode: int, custom_mode: int) -> None:
        """Send a SET_MODE to the target."""

    @abstractmethod
    def set_position_target_global(self, lat: float, lon: float, alt_rel: float) -> None:
        """Command a GUIDED position target (the "Fly To Here" primitive).

        ``lat``/``lon`` in degrees, ``alt_rel`` in metres above home.
        """

    @abstractmethod
    def send_heartbeat(self) -> None:
        """Announce this GCS to the vehicle (keeps GCS-failsafe timers happy)."""
