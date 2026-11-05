"""PlatformIO custom target: write 0xFF to all Mega 2560 EEPROM bytes over serial."""

from __future__ import annotations

import os
from pathlib import Path

from SCons.Script import ARGUMENTS

Import("env")

MEGA2560_EEPROM_SIZE = 4096
# Athena NETEEPROM_IMG_STAT — bootloader sets IMG_OK (0xEE) on STK_LEAVE_PROGMODE after avrdude.
ATHENA_IMG_STAT_ADDR = 2
ATHENA_IMG_OK_VALUE = 0xEE


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


def _avrdude_bin(pkg_dir: str) -> str:
    for name in ("bin/avrdude", "bin/avrdude.exe", "avrdude", "avrdude.exe"):
        path = os.path.join(pkg_dir, *name.split("/"))
        if os.path.isfile(path):
            return path
    raise RuntimeError(f"avrdude not found under {pkg_dir}")


def _parse_intel_hex(path: Path) -> dict[int, int]:
    """Return address -> byte for data records in an Intel HEX file."""
    mem: dict[int, int] = {}
    extended = 0
    with path.open(encoding="ascii") as fp:
        for line in fp:
            line = line.strip()
            if not line.startswith(":"):
                continue
            count = int(line[1:3], 16)
            addr = int(line[3:7], 16)
            rec_type = int(line[7:9], 16)
            if rec_type == 0:
                base = extended + addr
                for i in range(count):
                    mem[base + i] = int(line[9 + i * 2 : 11 + i * 2], 16)
            elif rec_type == 2:
                extended = int(line[9:13], 16) << 4
            elif rec_type == 4:
                extended = int(line[9:13], 16) << 16
            elif rec_type == 1:
                break
    return mem


ATHENA_IMG_OK_VALUE = 0xEE
# Same bytes eeprom-net writer must program (see WriteEepromNet.ino networkWritable).
NET_WRITER_ADDRS = tuple(list(range(3, 26)) + [68, 69])
# Bootloader stamps these on reset (check_and_update_eeprom_version); ignored on read-back.
BOOTLOADER_STAMP_ADDRS = (0, 1, 65, 66, 67)


def _verify_blank_readback(readback: Path, sample_addrs: tuple[int, ...]) -> None:
    mem = _parse_intel_hex(readback)

    net_bad = [
        addr for addr in NET_WRITER_ADDRS if mem.get(addr, -1) != 0xFF
    ]
    if net_bad:
        preview = ", ".join(f"{a}=0x{mem[a]:02X}" for a in net_bad[:12])
        suffix = "..." if len(net_bad) > 12 else ""
        raise RuntimeError(
            "EEPROM network block not blank for eeprom-net "
            f"({len(net_bad)} bytes in 3..25 / 68-69): {preview}{suffix}"
        )

    sample_bad = [
        addr for addr in sample_addrs if mem.get(addr, -1) != 0xFF
    ]
    if sample_bad:
        samples = ", ".join(f"{a}=0x{mem.get(a, 0):02X}" for a in sample_bad[:8])
        raise RuntimeError(f"EEPROM read-back spot-check failed: {samples}")

    other_non_ff = [
        addr
        for addr in range(70)
        if addr not in NET_WRITER_ADDRS
        and addr not in BOOTLOADER_STAMP_ADDRS
        and addr != ATHENA_IMG_STAT_ADDR
        and mem.get(addr, -1) != 0xFF
    ]
    if other_non_ff:
        preview = ", ".join(f"{a}=0x{mem[a]:02X}" for a in other_non_ff[:8])
        print(
            f"[eeprom-blank] note: non-FF outside net block (e.g. password bytes) — "
            f"{len(other_non_ff)} byte(s): {preview}"
            + ("..." if len(other_non_ff) > 8 else "")
        )

    img = mem.get(ATHENA_IMG_STAT_ADDR, -1)
    img_note = (
        f"byte 2=0xEE (IMG_OK after avrdude exit is normal)"
        if img == ATHENA_IMG_OK_VALUE
        else f"byte 2=0x{img:02X}"
    )
    print(
        "[eeprom-blank] read-back OK (net bytes 3..25 & 68-69 are 0xFF; "
        + img_note
        + "; bootloader may stamp bytes 0,1,65-67 on read)"
    )


def blank_eeprom(target, source, env):
    project_dir = Path(env.subst("$PROJECT_DIR"))
    blank_dir = project_dir / ".pio" / "eeprom-blank"
    blank_dir.mkdir(parents=True, exist_ok=True)
    blank_eep = blank_dir / "blank.eep"
    _write_blank_eep(blank_eep)

    port = _resolve_upload_port(env)
    pkg_dir = env.PioPlatform().get_package_dir("tool-avrdude")
    avrdude = _avrdude_bin(pkg_dir)
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

    readback_eep = blank_dir / "readback.eep"
    read_cmd = (
        f'"{avrdude}" -C"{avrdude_conf}" -p{mcu} -c{protocol} '
        f'-P{port} -b{baud} -U eeprom:r:{readback_eep}:i'
    )
    if env.Execute(env.VerboseAction(read_cmd, "Reading EEPROM for verification")):
        raise RuntimeError("avrdude failed to read EEPROM for verification")

    # Network writer checks sigs 3–4, IPs/MAC 5–25, and CS/reset 68–69 (not byte 2 imgStat).
    _verify_blank_readback(readback_eep, (3, 4, 5, 19, 23, 68, 69))

    print("[eeprom-blank] EEPROM blanked to 0xFF (verified on chip)")
    print(
        "[eeprom-blank] next (same --upload-port): pio run -e",
        env["PIOENV"],
        "-t eeprom-net --upload-port",
        port,
    )


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
