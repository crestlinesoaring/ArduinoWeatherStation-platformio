"""PlatformIO custom target: patch Athena EEPROM network settings from build_flags."""

from __future__ import annotations

import os
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


def _hw_version_num() -> int:
    value = _cpp_define("HW_VERSION")
    if value is None:
        raise RuntimeError(
            f"HW_VERSION is not defined for env '{env['PIOENV']}'. "
            "Add e.g. -D HW_VERSION='\"7\"' to build_flags in platformio.ini."
        )
    if isinstance(value, str):
        return int(value.strip().strip('"').strip("'"))
    return int(value)


def _require_define(name: str) -> int:
    value = _cpp_define(name)
    if value is None:
        raise RuntimeError(
            f"{name} is not defined for env '{env['PIOENV']}'. "
            f"Add e.g. -D {name}=227 to build_flags in platformio.ini."
        )
    return int(value)


def _upload_port() -> str:
    port = env.subst("$UPLOAD_PORT")
    if port and port != "$UPLOAD_PORT":
        return port

    option_port = env.GetProjectOption("upload_port", None)
    if option_port:
        return option_port

    raise RuntimeError(
        "Upload port not set. Use upload_port in platformio.ini or "
        "pio run -e <env> -t eeprom-net --upload-port /dev/cu.usbmodemXXXX"
    )


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
    mac5 = _cpp_define("ATHENA_MAC_5")
    mac6 = _cpp_define("ATHENA_MAC_6")
    mac = None
    if mac5 is not None or mac6 is not None:
        if mac5 is None or mac6 is None:
            raise RuntimeError("Define both ATHENA_MAC_5 and ATHENA_MAC_6, or neither.")
        mac = (0xDE, 0xAD, 0xBE, 0xEF, int(mac5), int(mac6))

    settings = network_settings_from_env(
        ip_gw=ip_gw,
        ip_q3=ip_q3,
        ip_wx=ip_wx,
        mac=mac,
        tftp_port=int(tftp_port) if tftp_port is not None else 46969,
    )

    port = _upload_port()

    print(f"[eeprom-net] env={env_name} writer={writer_env} port={port}")
    print(f"[eeprom-net] target {settings.summary()}")

    log_dir = project_dir / ".pio" / "eeprom-net-logs"
    log_dir.mkdir(parents=True, exist_ok=True)
    stamp = datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S")

    upload_cmd = [
        "pio",
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


if "upload-port" in ARGUMENTS:
    env.Replace(UPLOAD_PORT=ARGUMENTS["upload-port"])

env.AddCustomTarget(
    name="eeprom-net",
    dependencies=None,
    actions=[env.Action(patch_athena_eeprom_network, "Patching Athena EEPROM network...")],
    title="EEPROM-Net",
    description=(
        "Upload Athena network writer from IP_GW/IP_Q3/IP_WX and verify via serial. "
        "Use --force-hw-id to re-burn byte 75 from HW_VERSION."
    ),
)
