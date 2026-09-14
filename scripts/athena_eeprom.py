#!/usr/bin/env python3
"""Helpers for the eeprom-net PlatformIO target (serial verify from writer sketch)."""

from __future__ import annotations

import time
from dataclasses import dataclass

NETEEPROM_SIG_3_VALUE = 0xBB
DEFAULT_TFTP_PORT = 46969


@dataclass(frozen=True)
class AthenaNetworkSettings:
    ip: tuple[int, int, int, int]
    gateway: tuple[int, int, int, int]
    subnet: tuple[int, int, int, int]
    mac: tuple[int, int, int, int, int, int]
    tftp_port: int = DEFAULT_TFTP_PORT

    def summary(self) -> str:
        return (
            f"IP={'.'.join(map(str, self.ip))}, "
            f"GW={'.'.join(map(str, self.gateway))}, "
            f"SN={'.'.join(map(str, self.subnet))}, "
            f"MAC={':'.join(f'{b:02X}' for b in self.mac)}, "
            f"TFTP port={self.tftp_port}"
        )


def network_settings_from_env(
    ip_gw: int,
    ip_q3: int,
    ip_wx: int,
    subnet: tuple[int, int, int, int] = (255, 255, 255, 0),
    mac: tuple[int, int, int, int, int, int] | None = None,
    tftp_port: int = DEFAULT_TFTP_PORT,
) -> AthenaNetworkSettings:
    """Build Athena network settings from PlatformIO IP_GW / IP_Q3 / IP_WX flags."""
    if not (0 <= ip_gw <= 255 and 0 <= ip_q3 <= 255 and 0 <= ip_wx <= 255):
        raise ValueError("IP_GW, IP_Q3, and IP_WX must be byte values (0-255)")

    if mac is None:
        mac = (0xDE, 0xAD, 0xBE, 0xEF, 0x02, ip_wx)

    return AthenaNetworkSettings(
        ip=(192, 168, ip_q3, ip_wx),
        gateway=(192, 168, ip_q3, ip_gw),
        subnet=subnet,
        mac=mac,
        tftp_port=tftp_port,
    )


def read_writer_verify_line(port: str, timeout_s: float = 10.0) -> tuple[str, str]:
    """Reset the board and return VERIFY + RAW lines from a fresh writer boot."""
    try:
        import serial
    except ImportError as exc:
        raise RuntimeError("pyserial is required for eeprom-net verification") from exc

    ser = serial.Serial()
    ser.port = port
    ser.baudrate = 115200
    ser.timeout = 0.2
    ser.dtr = False
    ser.rts = False
    ser.open()
    ser.reset_input_buffer()

    # Force a clean boot so we do not accept stale VERIFY output in the USB buffer.
    ser.dtr = True
    time.sleep(0.05)
    ser.dtr = False
    time.sleep(0.2)
    ser.reset_input_buffer()

    deadline = time.time() + timeout_s
    buffer = ""
    verify_line = ""
    raw_line = ""
    try:
        while time.time() < deadline:
            chunk = ser.read(4096).decode("utf-8", errors="replace")
            if chunk:
                buffer += chunk
                for line in buffer.splitlines():
                    if line.startswith("[eeprom-net] VERIFY "):
                        verify_line = line
                    elif line.startswith("[eeprom-net] RAW "):
                        raw_line = line
            if verify_line and raw_line:
                return verify_line, raw_line
            time.sleep(0.1)
    finally:
        ser.close()

    if verify_line and raw_line:
        return verify_line, raw_line
    raise RuntimeError(
        "timed out waiting for [eeprom-net] VERIFY and RAW lines from writer sketch"
    )


def parse_writer_raw_line(line: str, settings: AthenaNetworkSettings) -> None:
    """Validate [eeprom-net] RAW line (on-device EEPROM read-back)."""
    import re

    pairs = re.findall(r"(\d+)=([0-9A-Fa-f]{1,2})", line)
    if not pairs:
        raise RuntimeError(f"unrecognized RAW line: {line}")

    raw = {int(addr): int(value, 16) for addr, value in pairs}

    expected = {
        5: settings.gateway[0],
        6: settings.gateway[1],
        7: settings.gateway[2],
        8: settings.gateway[3],
        9: settings.subnet[0],
        10: settings.subnet[1],
        11: settings.subnet[2],
        12: settings.subnet[3],
        13: settings.mac[0],
        14: settings.mac[1],
        15: settings.mac[2],
        16: settings.mac[3],
        17: settings.mac[4],
        18: settings.mac[5],
        19: settings.ip[0],
        20: settings.ip[1],
        21: settings.ip[2],
        22: settings.ip[3],
        23: NETEEPROM_SIG_3_VALUE,
        24: settings.tftp_port & 0xFF,
        25: settings.tftp_port >> 8,
    }

    mismatches: list[str] = []
    for addr, want in expected.items():
        got = raw.get(addr)
        if got is None:
            mismatches.append(f"missing byte {addr}")
        elif got != want:
            mismatches.append(f"byte {addr}: 0x{got:02X} != 0x{want:02X}")

    if mismatches:
        raise RuntimeError(
            "on-device EEPROM read-back mismatch: " + "; ".join(mismatches)
        )


def parse_writer_verify_line(line: str, settings: AthenaNetworkSettings) -> None:
    """Raise RuntimeError if the writer VERIFY line does not match settings."""
    import re

    match = re.search(
        r"VERIFY GW=(\d+\.\d+\.\d+\.\d+) IP=(\d+\.\d+\.\d+\.\d+) "
        r"SN=(\d+\.\d+\.\d+\.\d+) MAC=([0-9A-F:]+) PORT=(\d+) CS=(\d+) RESET=(\d+) (OK|FAIL)$",
        line,
    )
    if not match:
        raise RuntimeError(f"unrecognized VERIFY line: {line}")

    gw, ip, sn, mac, port_s, cs_s, reset_s, status = match.groups()
    if status != "OK":
        raise RuntimeError(f"writer reported VERIFY FAIL: {line}")

    expected_gw = ".".join(map(str, settings.gateway))
    expected_ip = ".".join(map(str, settings.ip))
    expected_sn = ".".join(map(str, settings.subnet))
    expected_mac = ":".join(f"{b:02X}" for b in settings.mac)

    mismatches: list[str] = []
    if gw != expected_gw:
        mismatches.append(f"gateway {gw} != {expected_gw}")
    if ip != expected_ip:
        mismatches.append(f"IP {ip} != {expected_ip}")
    if sn != expected_sn:
        mismatches.append(f"subnet {sn} != {expected_sn}")
    if mac.upper() != expected_mac:
        mismatches.append(f"MAC {mac} != {expected_mac}")
    if int(port_s) != settings.tftp_port:
        mismatches.append(f"port {port_s} != {settings.tftp_port}")
    if int(cs_s) != 53:
        mismatches.append("CS pin != 53")
    if int(reset_s) != 6:
        mismatches.append("reset pin != 6")

    if mismatches:
        raise RuntimeError("writer VERIFY mismatch: " + "; ".join(mismatches))
