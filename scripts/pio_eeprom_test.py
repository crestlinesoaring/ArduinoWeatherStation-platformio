"""PlatformIO custom target: upload destructive EEPROM cell test sketch."""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

from SCons.Script import ARGUMENTS

Import("env")

SCRIPT_DIR = Path(env.subst("$PROJECT_DIR")) / "scripts"
sys.path.insert(0, str(SCRIPT_DIR))


def _resolve_upload_port(action_env) -> str:
    if "upload-port" in ARGUMENTS:
        action_env.Replace(UPLOAD_PORT=ARGUMENTS["upload-port"])
    elif ARGUMENTS.get("port"):
        action_env.Replace(UPLOAD_PORT=ARGUMENTS["port"])
    else:
        action_env.AutodetectUploadPort()

    port = action_env.subst("$UPLOAD_PORT")
    if not port or port == "$UPLOAD_PORT":
        raise RuntimeError(
            f"upload_port not set for env '{action_env['PIOENV']}'. "
            "Select a port in PlatformIO or pass -p."
        )
    return port


def _pio_cmd() -> list[str]:
    return [sys.executable, "-m", "platformio"]


def run_eeprom_cell_test(target, source, env):
    base_env = env["PIOENV"]
    test_env = "mega-eeprom-test"
    project_dir = Path(env.subst("$PROJECT_DIR"))
    port = _resolve_upload_port(env)

    print(f"[eeprom-test] lab env={base_env} sketch={test_env} port={port}")
    print("[eeprom-test] DESTRUCTIVE: overwrites EEPROM under test; re-run eeprom-blank + eeprom-net after.")

    upload_cmd = _pio_cmd() + [
        "run",
        "-e",
        test_env,
        "-t",
        "upload",
        "--upload-port",
        port,
    ]
    subprocess.run(upload_cmd, cwd=str(project_dir), check=True)
    print("[eeprom-test] open serial @ 115200 for [eeprom-test] PASS/FAIL lines")


env.AddCustomTarget(
    name="eeprom-test",
    dependencies=None,
    actions=[env.Action(run_eeprom_cell_test, "Running EEPROM cell test...")],
    title="EEPROM-Test",
    description=(
        "Upload cell test sketch (default bytes 0-69). Full 4096: use *-eeprom-test env "
        "with -D EEPROM_TEST_LAST_ADDR=4095. Destructive — blank and eeprom-net afterward."
    ),
)
