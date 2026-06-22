#!/usr/bin/env python3
"""Interactive MAVLink simulator over UDP — test the GCS without hardware.

Streams a believable ArduCopter telemetry feed (HEARTBEAT, ATTITUDE, VFR_HUD,
SYS_STATUS, GPS_RAW_INT, GLOBAL_POSITION_INT, STATUSTEXT) to the desktop GCS and
*responds* to the commands the GCS sends:

  * ARM/DISARM (MAV_CMD_COMPONENT_ARM_DISARM) → flips the armed bit, replies
    COMMAND_ACK. Arming is refused while still "initialising" for the first few
    seconds so you can see a Mission-Planner-style pre-arm error in the log.
  * SET_MODE → switches the reported flight mode (so the mode buttons visibly
    change the HUD's mode readout).

Run the sim, then connect the GCS via UDP on the same port (default 14550):

    python tools/sim_udp.py                 # sends to 127.0.0.1:14550
    python tools/sim_udp.py --port 14550 --host 127.0.0.1
"""
from __future__ import annotations

import argparse
import math
import time

from pymavlink import mavutil

ARM_MAGIC_FORCE = 21196
SAFETY_ARMED = 0x80
CUSTOM_MODE_ENABLED = 0x01

# ArduCopter custom_mode → name, just for log messages.
MODE_NAMES = {0: "STABILIZE", 2: "ALT_HOLD", 5: "LOITER", 9: "LAND", 6: "RTL",
              4: "GUIDED", 3: "AUTO", 16: "POSHOLD"}


def bearing_deg(lat1, lon1, lat2, lon2):
    dlon = math.radians(lon2 - lon1)
    y = math.sin(dlon) * math.cos(math.radians(lat2))
    x = (math.cos(math.radians(lat1)) * math.sin(math.radians(lat2)) -
         math.sin(math.radians(lat1)) * math.cos(math.radians(lat2)) * math.cos(dlon))
    return (math.degrees(math.atan2(y, x)) + 360.0) % 360.0


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--host", default="127.0.0.1", help="GCS host to stream to")
    ap.add_argument("--port", type=int, default=14550, help="GCS UDP port")
    ap.add_argument("--lat", type=float, default=21.0285, help="start latitude")
    ap.add_argument("--lon", type=float, default=105.8048, help="start longitude")
    ap.add_argument("--arm-delay", type=float, default=6.0,
                    help="seconds before arming is permitted (pre-arm demo)")
    args = ap.parse_args()

    conn = mavutil.mavlink_connection(
        f"udpout:{args.host}:{args.port}", source_system=1, source_component=1)
    mav = conn.mav
    print(f"[sim] streaming ArduCopter telemetry -> {args.host}:{args.port}")

    t0 = time.time()
    lat, lon = args.lat, args.lon
    alt = 0.0                 # current altitude (m, relative)
    target_alt = None         # set by NAV_TAKEOFF / goto
    target_pos = None         # (lat, lon) set by "Fly To Here"
    heading_deg = 0.0
    last_alt = 0.0
    climb = 0.0
    ground_speed = 0.0
    armed = False
    custom_mode = 0          # STABILIZE
    next_send = {"hb": 0.0, "att": 0.0, "vfr": 0.0, "sys": 0.0, "gps": 0.0, "pos": 0.0}
    rates = {"hb": 1.0, "att": 0.05, "vfr": 0.1, "sys": 0.5, "gps": 0.5, "pos": 0.2}
    statustext_sent = False

    def base_mode() -> int:
        m = CUSTOM_MODE_ENABLED
        if armed:
            m |= SAFETY_ARMED
        return m

    def send_text(severity: int, text: str) -> None:
        mav.statustext_send(severity, text.encode("ascii", "replace")[:50])

    def poll():
        # On Windows a udpout socket raises WinError 10022 on recvfrom until it
        # has sent at least once; tolerate that and other transient recv errors.
        try:
            return conn.recv_match(blocking=False)
        except OSError:
            return None

    # Prime the socket so the OS assigns a local port before we ever recv.
    mav.heartbeat_send(mavutil.mavlink.MAV_TYPE_QUADROTOR,
                       mavutil.mavlink.MAV_AUTOPILOT_ARDUPILOTMEGA, 0, 0, 0)

    while True:
        now = time.time()
        t = now - t0

        # ── react to GCS commands ────────────────────────────────────────────
        msg = poll()
        while msg is not None:
            mtype = msg.get_type()
            if mtype == "COMMAND_LONG":
                cmd = msg.command
                if cmd == mavutil.mavlink.MAV_CMD_COMPONENT_ARM_DISARM:
                    want_arm = msg.param1 >= 0.5
                    forced = abs(msg.param2 - ARM_MAGIC_FORCE) < 1
                    if want_arm and t < args.arm_delay and not forced:
                        send_text(mavutil.mavlink.MAV_SEVERITY_ERROR,
                                  "PreArm: initialising sensors")
                        mav.command_ack_send(
                            cmd, mavutil.mavlink.MAV_RESULT_TEMPORARILY_REJECTED)
                    else:
                        armed = want_arm
                        if not armed:
                            target_alt = target_pos = None
                        mav.command_ack_send(cmd, mavutil.mavlink.MAV_RESULT_ACCEPTED)
                        send_text(mavutil.mavlink.MAV_SEVERITY_INFO,
                                  "Armed" if armed else "Disarmed")
                        print(f"[sim] {'ARMED' if armed else 'DISARMED'}")
                elif cmd == mavutil.mavlink.MAV_CMD_NAV_TAKEOFF:
                    if armed:
                        target_alt = float(msg.param7)
                        mav.command_ack_send(cmd, mavutil.mavlink.MAV_RESULT_ACCEPTED)
                        send_text(mavutil.mavlink.MAV_SEVERITY_INFO,
                                  f"Takeoff to {target_alt:.0f}m")
                        print(f"[sim] TAKEOFF -> {target_alt:.0f} m")
                    else:
                        mav.command_ack_send(cmd, mavutil.mavlink.MAV_RESULT_FAILED)
                        send_text(mavutil.mavlink.MAV_SEVERITY_ERROR,
                                  "Takeoff: vehicle disarmed")
                else:
                    mav.command_ack_send(cmd, mavutil.mavlink.MAV_RESULT_UNSUPPORTED)
            elif mtype == "SET_MODE":
                custom_mode = msg.custom_mode
                name = MODE_NAMES.get(custom_mode, f"MODE {custom_mode}")
                send_text(mavutil.mavlink.MAV_SEVERITY_INFO, name)
                print(f"[sim] mode -> {name}")
            elif mtype == "SET_POSITION_TARGET_GLOBAL_INT":
                target_pos = (msg.lat_int / 1e7, msg.lon_int / 1e7)
                target_alt = float(msg.alt)
                send_text(mavutil.mavlink.MAV_SEVERITY_INFO, "Guided target set")
                print(f"[sim] FLY TO {target_pos[0]:.6f},{target_pos[1]:.6f} "
                      f"@ {target_alt:.0f}m")
            msg = poll()

        # ── periodic telemetry ───────────────────────────────────────────────
        if now >= next_send["hb"]:
            next_send["hb"] = now + rates["hb"]
            mav.heartbeat_send(mavutil.mavlink.MAV_TYPE_QUADROTOR,
                               mavutil.mavlink.MAV_AUTOPILOT_ARDUPILOTMEGA,
                               base_mode(), custom_mode,
                               mavutil.mavlink.MAV_STATE_ACTIVE if armed
                               else mavutil.mavlink.MAV_STATE_STANDBY)
            if not statustext_sent:
                send_text(mavutil.mavlink.MAV_SEVERITY_INFO, "Sim telemetry online")
                statustext_sent = True

        if now >= next_send["att"]:
            next_send["att"] = now + rates["att"]
            roll = math.radians(20.0 * math.sin(t * 0.8))
            pitch = math.radians(12.0 * math.sin(t * 0.5))
            yaw = (t * 0.15) % (2 * math.pi)
            mav.attitude_send(int(t * 1000) & 0xFFFFFFFF, roll, pitch, yaw,
                              0.0, 0.0, 0.0)

        if now >= next_send["vfr"]:
            next_send["vfr"] = now + rates["vfr"]
            throttle = 55 if (armed and alt > 0.5) else (12 if armed else 0)
            mav.vfr_hud_send(ground_speed, ground_speed, int(heading_deg),
                             throttle, alt, climb)

        if now >= next_send["sys"]:
            next_send["sys"] = now + rates["sys"]
            voltage = int(12300 - (t * 2))            # mV, slowly draining
            current = int(1500 + 500 * math.sin(t))   # cA
            remaining = max(0, 100 - int(t / 5))      # %
            mav.sys_status_send(0, 0, 0, 500, voltage, current, remaining,
                                0, 0, 0, 0, 0, 0)

        if now >= next_send["gps"]:
            next_send["gps"] = now + rates["gps"]
            mav.gps_raw_int_send(int(t * 1e6), 3, int(lat * 1e7), int(lon * 1e7),
                                 30000, 80, 65535, 0, 0, 14)

        if now >= next_send["pos"]:
            next_send["pos"] = now + rates["pos"]
            # climb toward the takeoff/goto target altitude
            if target_alt is not None:
                if abs(alt - target_alt) <= 0.6:
                    alt = target_alt
                else:
                    alt += 2.0 if target_alt > alt else -2.0
            # fly horizontally toward a "Fly To Here" target (exp. approach)
            ground_speed = 0.0
            if target_pos is not None and armed:
                tlat, tlon = target_pos
                dlat, dlon = tlat - lat, tlon - lon
                if abs(dlat) + abs(dlon) > 1e-6:
                    heading_deg = bearing_deg(lat, lon, tlat, tlon)
                    lat += dlat * 0.10
                    lon += dlon * 0.10
                    ground_speed = 6.0
            climb = (alt - last_alt) / rates["pos"]
            last_alt = alt
            alt_mm = int(alt * 1000)
            mav.global_position_int_send(int(t * 1000) & 0xFFFFFFFF,
                                         int(lat * 1e7), int(lon * 1e7),
                                         int((100 + alt) * 1000), alt_mm,
                                         0, 0, 0, int(heading_deg * 100))

        time.sleep(0.01)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\n[sim] stopped")
