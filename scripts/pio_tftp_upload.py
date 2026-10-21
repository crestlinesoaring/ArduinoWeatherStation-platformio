"""PlatformIO custom target: upload firmware.hex.bin via Athena TFTP (curl)."""

from __future__ import annotations

import re
import shutil
import socket
import subprocess
import time
from pathlib import Path

from SCons.Script import ARGUMENTS

Import("env")

TFTP_RETRY_COUNT = 3
TFTP_RETRY_DELAY_S = 2.0

DEFAULT_TFTP_PORT = 69
TELNET_PORT = 23537
DEPLOY_RESET_PRE_DELAY_S = 0.5
DEPLOY_POST_RESET_DELAY_S = 2.0
DEPLOY_RETRY_DELAY_S = 1.0


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


def _resolve_tftp_port() -> int:
    port = _cpp_define("ATHENA_TFTP_PORT")
    if port is None:
        return DEFAULT_TFTP_PORT
    return _int_define(port, "ATHENA_TFTP_PORT")


def _tftp_url(host: str, filename: str, port: int | None = None) -> str:
    if port is None:
        port = _resolve_tftp_port()
    if port == 69:
        return f"tftp://{host}/{filename}"
    return f"tftp://{host}:{port}/{filename}"


def _resolve_tftp_host() -> str:
    """Return target host. Honor --upload-port when it looks like an IP address."""
    upload_port = ARGUMENTS.get("upload-port") or ARGUMENTS.get("port")
    if upload_port and "." in upload_port:
        return upload_port.split(":", 1)[0]

    ip_q3 = _require_define("IP_Q3")
    ip_wx = _require_define("IP_WX")
    return f"192.168.{ip_q3}.{ip_wx}"


def _curl_tftp_put(curl: str, bin_path: Path, url: str) -> None:
    cmd = [curl, "-T", str(bin_path), url, "--tftp-no-options"]
    subprocess.run(cmd, check=True)


def _send_telnet_tftp_mode(host: str) -> None:
    time.sleep(DEPLOY_RESET_PRE_DELAY_S)
    with socket.create_connection((host, TELNET_PORT), timeout=5) as sock:
        sock.settimeout(2.0)
        # Let the welcome line arrive before we send M (matches interactive telnet).
        try:
            while True:
                chunk = sock.recv(256)
                if not chunk or b"help" in chunk.lower():
                    break
        except OSError:
            pass
        sock.sendall(b"M\n")
        try:
            sock.recv(512)
        except OSError:
            pass
        time.sleep(0.25)


def _until_telnet_tftp_mode(host: str) -> None:
    while True:
        try:
            _send_telnet_tftp_mode(host)
            return
        except OSError:
            print(".")
            time.sleep(DEPLOY_RETRY_DELAY_S)


def _until_tftp_upload(curl: str, bin_path: Path, url: str) -> None:
    while True:
        try:
            _curl_tftp_put(curl, bin_path, url)
            return
        except subprocess.CalledProcessError:
            print(".")
            time.sleep(DEPLOY_RETRY_DELAY_S)


def _tftp_upload_with_retries(curl: str, bin_path: Path, url: str) -> None:
    last_error: Exception | None = None
    for attempt in range(1, TFTP_RETRY_COUNT + 1):
        try:
            print(f"[tftp-upload] TFTP put attempt {attempt}/{TFTP_RETRY_COUNT}")
            _curl_tftp_put(curl, bin_path, url)
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
    tftp_port = _resolve_tftp_port()
    url = _tftp_url(host, bin_path.name, tftp_port)

    print(f"[tftp-upload] env={env_name} file={bin_path}")
    print(f"[tftp-upload] curl -T {bin_path} {url} --tftp-no-options")
    print("[tftp-upload] station must be in Athena TFTP mode (e.g. right after a hard reboot)")
    _tftp_upload_with_retries(curl, bin_path, url)
    print("[tftp-upload] upload complete")


def tftp_deploy(target, source, env):
    env_name = env["PIOENV"]
    build_dir = Path(env.subst("$BUILD_DIR"))
    bin_path = build_dir / f"{env.subst('${PROGNAME}')}.hex.bin"

    if not bin_path.is_file():
        raise RuntimeError(
            f"{bin_path} not found. Run 'pio run -e {env_name}' to build firmware.hex.bin."
        )

    curl = shutil.which("curl")
    if not curl:
        raise RuntimeError("curl not found on PATH (required for TFTP deploy).")

    host = _resolve_tftp_host()
    tftp_port = _resolve_tftp_port()
    url = _tftp_url(host, bin_path.name, tftp_port)

    print(f"[tftp-deploy] env={env_name} host={host} file={bin_path}")
    if tftp_port != DEFAULT_TFTP_PORT:
        print(f"[tftp-deploy] TFTP port={tftp_port} (from ATHENA_TFTP_PORT)")
    print(
        f"[tftp-deploy] telnet M+enter on port {TELNET_PORT} "
        "(TFTP flag + reboot) until connect succeeds"
    )
    _until_telnet_tftp_mode(host)
    time.sleep(DEPLOY_POST_RESET_DELAY_S)
    print(f"[tftp-deploy] curl -T {bin_path} {url} --tftp-no-options")
    _until_tftp_upload(curl, bin_path, url)
    print("done")


env.AddCustomTarget(
    name="tftp-deploy",
    dependencies="$BUILD_DIR/${PROGNAME}.hex",
    actions=[env.Action(tftp_deploy, "Deploying firmware (telnet TFTP mode + upload)...")],
    title="TFTP-Deploy",
    description=(
        "Retry telnet M+enter on port 23537 until the station answers, wait 2s, "
        "then retry TFTP upload (curl --tftp-no-options) until success. "
        "Target IP from IP_Q3/IP_WX or --upload-port."
    ),
)

env.AddCustomTarget(
    name="tftp-upload",
    dependencies="$BUILD_DIR/${PROGNAME}.hex",
    actions=[env.Action(tftp_upload, "Uploading firmware via TFTP...")],
    title="TFTP-Upload",
    description=(
        "Upload firmware.hex.bin via Athena TFTP using curl. "
        "Target IP comes from IP_Q3/IP_WX build_flags. "
        "Override with --upload-port 192.168.x.x. "
        "Station must be in TFTP mode (e.g. immediately after a hard reboot)."
    ),
)
