"""PlatformIO custom target: patch Athena EEPROM network settings from build_flags."""

from __future__ import annotations

import os
import re
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

from SCons.Script import ARGUMENTS

Import("env")

SCRIPT_DIR = Path(env.subst("$PROJECT_DIR")) / "scripts"
sys.path.insert(0, str(SCRIPT_DIR))

from athena_eeprom import (  # noqa: E402
    network_settings_from_env,
    parse_writer_raw_line,
    parse_writer_verify_line,
    read_writer_verify_line,
)


def _cpp_define(name: str):
    for item in env.get("CPPDEFINES", []):
        if isinstance(item, str):
            if item == name:
                return True
        elif isinstance(item, (list, tuple)) and item and item[0] == name:
            if len(item) == 1:
                return True
            return item[1]
    return None


def _int_define(value, name: str) -> int:
    if isinstance(value, bool):
        raise ValueError(f"{name} is not numeric: {value!r}")
    if isinstance(value, int):
        return value
    match = re.search(r"-?\d+", str(value))
    if not match:
        raise ValueError(f"{name} is not numeric: {value!r}")
    return int(match.group())


def _hw_version_num() -> int:
    value = _cpp_define("HW_VERSION")
    if value is None:
        raise RuntimeError(
            f"HW_VERSION is not defined for env '{env['PIOENV']}'. "
            "Add e.g. -D HW_VERSION='\"7\"' to build_flags in platformio.ini."
        )
    try:
        return _int_define(value, "HW_VERSION")
    except ValueError as exc:
        raise RuntimeError(str(exc)) from exc


def _require_define(name: str) -> int:
    value = _cpp_define(name)
    if value is None:
        raise RuntimeError(
            f"{name} is not defined for env '{env['PIOENV']}'. "
            f"Add e.g. -D {name}=227 to build_flags in platformio.ini."
        )
    try:
        return _int_define(value, name)
    except ValueError as exc:
        raise RuntimeError(str(exc)) from exc


def _detect_serial_port() -> str | None:
    """Return the first likely Mega 2560 serial port, or any serial port."""
    try:
        from serial.tools import list_ports
    except ImportError:
        return None

    mega_pids = {0x0042, 0x0010, 0x0036}
    arduino_vids = {0x2341, 0x2A03}

    for port in list_ports.comports():
        if port.vid in arduino_vids and port.pid in mega_pids:
            return port.device

    ports = list(list_ports.comports())
    if len(ports) == 1:
        return ports[0].device
    return None


def _upload_port(action_env) -> str:
    if "upload-port" in ARGUMENTS:
        return str(ARGUMENTS["upload-port"])

    port = action_env.subst("$UPLOAD_PORT")
    if port and port != "$UPLOAD_PORT":
        return port

    option_port = action_env.GetProjectOption("upload_port", None)
    if option_port:
        return option_port

    detected = _detect_serial_port()
    if detected:
        print(f"[eeprom-net] auto-detected upload port: {detected}")
        return detected

    raise RuntimeError(
        "Upload port not set. Add upload_port to platformio.ini or pass "
        "--upload-port, e.g. "
        "pio run -e tavis -t eeprom-net --upload-port COM3"
    )


def _pio_cmd() -> list[str]:
    return [sys.executable, "-m", "platformio"]


def _writer_env_name(base_env: str) -> str:
    return f"{base_env}-eeprom-net"


def patch_athena_eeprom_network(target, source, env):
    env_name = env["PIOENV"]
    writer_env = _writer_env_name(env_name)
    project_dir = Path(env.subst("$PROJECT_DIR"))

    ip_gw = _require_define("IP_GW")
    ip_q3 = _require_define("IP_Q3")
    ip_wx = _require_define("IP_WX")

    tftp_port = _cpp_define("ATHENA_TFTP_PORT")
    mac5_define = _cpp_define("MAC_5")
    mac6_define = _cpp_define("MAC_6")
    mac5 = _int_define(mac5_define, "MAC_5") if mac5_define is not None else 0x02
    mac6 = _int_define(mac6_define, "MAC_6") if mac6_define is not None else ip_wx
    mac = (0xDE, 0xAD, 0xBE, 0xEF, mac5, mac6)

    settings = network_settings_from_env(
        ip_gw=ip_gw,
        ip_q3=ip_q3,
        ip_wx=ip_wx,
        mac=mac,
        tftp_port=int(tftp_port) if tftp_port is not None else 46969,
    )

    port = _upload_port(env)

    print(f"[eeprom-net] env={env_name} writer={writer_env} port={port}")
    print(f"[eeprom-net] target {settings.summary()}")

    log_dir = project_dir / ".pio" / "eeprom-net-logs"
    log_dir.mkdir(parents=True, exist_ok=True)
    stamp = datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S")

    upload_cmd = _pio_cmd() + [
        "run",
        "-e",
        writer_env,
        "-t",
        "upload",
        "--upload-port",
        port,
    ]
    upload_env = os.environ.copy()
    upload_env["EEPROM_NET_HW_VERSION"] = str(_hw_version_num())
    upload_env["EEPROM_NET_MAC_6"] = str(mac6)
    if mac5_define is not None:
        upload_env["EEPROM_NET_MAC_5"] = str(mac5)
    if "force-hw-id" in ARGUMENTS:
        upload_env["EEPROM_NET_FORCE_HW_ID"] = "1"
        print(
            f"[eeprom-net] will force re-write hardware ID byte 75 "
            f"(HW_VERSION={upload_env['EEPROM_NET_HW_VERSION']})"
        )
    print(f"[eeprom-net] uploading writer sketch ({writer_env})...")
    subprocess.run(upload_cmd, cwd=str(project_dir), check=True, env=upload_env)

    time.sleep(1.5)
    print("[eeprom-net] verifying via writer serial output...")
    verify_line, raw_line = read_writer_verify_line(port)
    print(verify_line)
    print(raw_line)
    parse_writer_verify_line(verify_line, settings)
    parse_writer_raw_line(raw_line, settings)

    if verify_line.endswith(" FAIL"):
        raise RuntimeError(
            "EEPROM network settings could not be written. "
            "The network block is likely corrupted; recover with an ISP chip erase, "
            "then run eeprom-net again."
        )

    verify_path = log_dir / f"{env_name}-{stamp}-verify.txt"
    verify_path.write_text(verify_line + "\n" + raw_line + "\n", encoding="utf-8")
    print(f"[eeprom-net] verified OK; log saved to {verify_path}")
    print("[eeprom-net] re-flash the main firmware: pio run -e", env_name, "-t upload")


env.AddCustomTarget(
    name="eeprom-net",
    dependencies=None,
    actions=[env.Action(patch_athena_eeprom_network, "Patching Athena EEPROM network...")],
    title="EEPROM-Net",
    description=(
        "Upload Athena network writer from IP_GW/IP_Q3/IP_WX and verify via serial. "
        "Optional MAC_6 overrides the MAC last octet (default IP_WX). "
        "Use --force-hw-id to re-burn byte 75 from HW_VERSION."
    ),
)
