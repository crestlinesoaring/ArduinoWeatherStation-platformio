"""PlatformIO custom target: upload firmware.hex.bin via Athena TFTP (curl)."""

from __future__ import annotations

import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

from SCons.Script import ARGUMENTS

Import("env")

TFTP_RETRY_COUNT = 3
TFTP_RETRY_DELAY_S = 2.0


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


def _tftp_url(host: str, filename: str) -> str:
    return f"tftp://{host}/{filename}"


def _resolve_tftp_host() -> str:
    """Return target host. Honor --upload-port when it looks like an IP address."""
    upload_port = ARGUMENTS.get("upload-port") or ARGUMENTS.get("port")
    if upload_port and "." in upload_port:
        return upload_port.split(":", 1)[0]

    ip_q3 = _require_define("IP_Q3")
    ip_wx = _require_define("IP_WX")
    return f"192.168.{ip_q3}.{ip_wx}"


def _curl_tftp_upload(curl: str, bin_path: Path, url: str) -> None:
    cmd = [curl, "-T", str(bin_path), url]
    if int(ARGUMENTS.get("PIOVERBOSE", 0)):
        cmd.insert(1, "-v")
    subprocess.run(cmd, check=True)


def _tftp_upload_with_retries(curl: str, bin_path: Path, url: str) -> None:
    last_error: Exception | None = None
    for attempt in range(1, TFTP_RETRY_COUNT + 1):
        try:
            print(f"[tftp-upload] TFTP put attempt {attempt}/{TFTP_RETRY_COUNT}")
            _curl_tftp_upload(curl, bin_path, url)
            return
        except subprocess.CalledProcessError as exc:
            last_error = exc
            if attempt < TFTP_RETRY_COUNT:
                print(f"[tftp-upload] TFTP failed; retrying in {TFTP_RETRY_DELAY_S:.0f}s...")
                time.sleep(TFTP_RETRY_DELAY_S)
    raise RuntimeError("TFTP upload failed after retries") from last_error


def tftp_upload(target, source, env):
    env_name = env["PIOENV"]
    build_dir = Path(env.subst("$BUILD_DIR"))
    bin_path = build_dir / f"{env.subst('${PROGNAME}')}.hex.bin"

    if not bin_path.is_file():
        raise RuntimeError(
            f"{bin_path} not found. Run 'pio run -e {env_name}' to build firmware.hex.bin."
        )

    curl = shutil.which("curl")
    if not curl:
        raise RuntimeError("curl not found on PATH (required for TFTP upload).")

    host = _resolve_tftp_host()
    url = _tftp_url(host, bin_path.name)

    print(f"[tftp-upload] env={env_name} file={bin_path}")
    print(f"[tftp-upload] curl -T {bin_path} {url}")
    print("[tftp-upload] board must already be in Athena TFTP/update mode")
    _tftp_upload_with_retries(curl, bin_path, url)
    print("[tftp-upload] upload complete")


env.AddCustomTarget(
    name="tftp-upload",
    dependencies="$BUILD_DIR/${PROGNAME}.hex",
    actions=[env.Action(tftp_upload, "Uploading firmware via TFTP...")],
    title="TFTP-Upload",
    description=(
        "Upload firmware.hex.bin via Athena TFTP using curl. "
        "Target IP comes from IP_Q3/IP_WX build_flags. "
        "Override with --upload-port 192.168.x.x. "
        "Put the board in TFTP mode first."
    ),
)
