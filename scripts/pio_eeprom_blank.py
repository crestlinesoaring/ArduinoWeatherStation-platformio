"""PlatformIO custom target: write 0xFF to all Mega 2560 EEPROM bytes over serial."""

from __future__ import annotations

import os
from pathlib import Path

from SCons.Script import ARGUMENTS

Import("env")

MEGA2560_EEPROM_SIZE = 4096


def _write_blank_eep(path: Path, size: int = MEGA2560_EEPROM_SIZE) -> None:
    with path.open("w", encoding="ascii") as fp:
        for addr in range(0, size, 16):
            chunk = "FF" * 16
            record = f"10{addr:04X}00{chunk}"
            checksum = (
                -(sum(int(record[i : i + 2], 16) for i in range(0, len(record), 2)) & 0xFF)
            ) & 0xFF
            fp.write(f":{record}{checksum:02X}\n")
        fp.write(":00000001FF\n")


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


def _avrdude_part_id(action_env) -> str:
    mcu = action_env.subst("$BOARD_MCU")
    if mcu in ("atmega2560", "ATmega2560"):
        return "m2560"
    return mcu


def blank_eeprom(target, source, env):
    project_dir = Path(env.subst("$PROJECT_DIR"))
    blank_dir = project_dir / ".pio" / "eeprom-blank"
    blank_dir.mkdir(parents=True, exist_ok=True)
    blank_eep = blank_dir / "blank.eep"
    _write_blank_eep(blank_eep)

    port = _resolve_upload_port(env)
    pkg_dir = env.PioPlatform().get_package_dir("tool-avrdude")
    avrdude = os.path.join(pkg_dir, "bin", "avrdude")
    if not os.path.isfile(avrdude):
        avrdude = os.path.join(pkg_dir, "avrdude")
    avrdude_conf = os.path.join(pkg_dir, "avrdude.conf")

    mcu = _avrdude_part_id(env)
    protocol = env.GetProjectOption("upload_protocol", "wiring")
    baud = env.GetProjectOption("upload_speed", 115200)

    print(f"[eeprom-blank] port={port} mcu={mcu} size={MEGA2560_EEPROM_SIZE}")
    print(f"[eeprom-blank] writing {blank_eep}")

    cmd = (
        f'"{avrdude}" -C"{avrdude_conf}" -p{mcu} -c{protocol} '
        f'-P{port} -b{baud} -U eeprom:w:{blank_eep}:i'
    )
    if env.Execute(env.VerboseAction(cmd, "Uploading blank EEPROM")):
        raise RuntimeError("avrdude failed to write blank EEPROM")

    print("[eeprom-blank] EEPROM blanked to 0xFF")
    print("[eeprom-blank] next: pio run -e", env["PIOENV"], "-t eeprom-net")


env.AddCustomTarget(
    name="eeprom-blank",
    dependencies=None,
    actions=[env.Action(blank_eeprom, "Blanking EEPROM to 0xFF...")],
    title="EEPROM-Blank",
    description=(
        "Write 0xFF to all EEPROM bytes over serial. Recovery after a clear-to-zero "
        "sketch or when eeprom-net reports cells cannot be updated in place."
    ),
)
