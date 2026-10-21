"""PlatformIO custom target: run scripts/telnet-snipe.ps1 for the active env's station IP."""

from __future__ import annotations

import platform
import shutil
import subprocess
import sys
from pathlib import Path

from SCons.Script import ARGUMENTS

Import("env")

SCRIPT_DIR = Path(env.subst("$PROJECT_DIR")) / "scripts"
sys.path.insert(0, str(SCRIPT_DIR))

from pio_station_net import resolve_station_host  # noqa: E402


def telnet_snipe(target, source, env):
    env_name = env["PIOENV"]
    project_dir = Path(env.subst("$PROJECT_DIR"))
    host = resolve_station_host(env, ARGUMENTS)

    cmd_wrapper = project_dir / "scripts" / "telnet-snipe.cmd"
    ps1 = project_dir / "scripts" / "telnet-snipe.ps1"
    if not ps1.is_file():
        raise RuntimeError(f"Missing {ps1}")

    host_args = ["-StationHost", host]
    print(f"[telnet-snipe] env={env_name} host={host}")

    if platform.system() == "Windows":
        if not cmd_wrapper.is_file():
            raise RuntimeError(f"Missing {cmd_wrapper}")
        run = [str(cmd_wrapper)] + host_args
    else:
        shell = shutil.which("pwsh") or shutil.which("powershell")
        if not shell:
            raise RuntimeError(
                "telnet-snipe needs PowerShell (native on Windows, or pwsh/powershell on PATH)."
            )
        run = [
            shell,
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            str(ps1),
        ] + host_args

    subprocess.run(run, check=True, cwd=project_dir)


env.AddCustomTarget(
    name="telnet-snipe",
    dependencies=[],
    actions=[env.Action(telnet_snipe, "Sniping station telnet...")],
    title="Telnet-Snipe",
    description=(
        "Run scripts/telnet-snipe.ps1 until TCP 23537 connects. "
        "Station IP from IP_Q3/IP_WX for this env, or --upload-port 192.168.x.x."
    ),
)
