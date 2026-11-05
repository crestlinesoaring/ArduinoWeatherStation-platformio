"""Point *-eeprom-net environments at src/eeprom-net-writer only."""

import os
import re
import sys
from pathlib import Path

Import("env")

_SCRIPT_DIR = Path(env.subst("$PROJECT_DIR")) / "scripts"
if str(_SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(_SCRIPT_DIR))

from athena_eeprom import int_from_cpp_define, resolve_mac5_mac6  # noqa: E402

_D_DEFINE_RE = re.compile(r"^-D\s*(\w+)(?:=(.*))?$")


def _cpp_define_value(action_env, name: str):
    for item in action_env.get("CPPDEFINES", []):
        if isinstance(item, str):
            if item == name:
                return True
        elif isinstance(item, (list, tuple)) and item and item[0] == name:
            if len(item) == 1:
                return True
            return item[1]
    return None


def _build_flag_entries(action_env):
    flags = action_env.get("BUILD_FLAGS") or []
    if not isinstance(flags, list):
        return [str(flags)]
    return [str(item) for item in flags]


def _build_flag_define_value(action_env, name: str):
    for raw in _build_flag_entries(action_env):
        match = _D_DEFINE_RE.match(raw.strip())
        if not match or match.group(1) != name:
            continue
        if match.group(2) is None:
            return True
        return match.group(2).strip()
    return None


def _resolve_define_value(action_env, name: str):
    raw = _cpp_define_value(action_env, name)
    if raw is not None and raw is not True:
        return raw
    raw = _build_flag_define_value(action_env, name)
    if raw is not None and raw is not True:
        return raw
    return None


def _int_from_define(value, name: str) -> int:
    if isinstance(value, bool):
        raise ValueError(f"{name} is not numeric: {value!r}")
    if isinstance(value, int):
        return value
    match = re.search(r"-?\d+", str(value))
    if not match:
        raise ValueError(f"{name} is not numeric: {value!r}")
    return int(match.group())


def _strip_cpp_define(action_env, name: str) -> None:
    filtered = []
    for item in action_env.get("CPPDEFINES", []):
        if isinstance(item, str):
            if item == name:
                continue
        elif isinstance(item, (list, tuple)) and item and item[0] == name:
            continue
        filtered.append(item)
    action_env.Replace(CPPDEFINES=filtered)


def _strip_build_flag_define(action_env, name: str) -> None:
    kept = []
    for raw in _build_flag_entries(action_env):
        match = _D_DEFINE_RE.match(raw.strip())
        if match and match.group(1) == name:
            continue
        kept.append(raw)
    action_env.Replace(BUILD_FLAGS=kept)


def _replace_numeric_define(action_env, name: str, value: int) -> None:
    _strip_cpp_define(action_env, name)
    _strip_build_flag_define(action_env, name)
    action_env.Append(CPPDEFINES=[(name, value)])


if env["PIOENV"].endswith("-eeprom-net"):
    writer_src = env.subst("$PROJECT_DIR/src/eeprom-net-writer")
    env.Replace(SRC_DIR=writer_src)
    env.Replace(PROJECT_SRC_DIR=writer_src)

    hw_version = os.environ.get("EEPROM_NET_HW_VERSION")
    if not hw_version:
        raw = _resolve_define_value(env, "HW_VERSION")
        if raw is not None:
            hw_version = str(_int_from_define(raw, "HW_VERSION"))
    if hw_version:
        # Numeric ID for the writer only. Lab build_flags use string HW_VERSION='"6"'
        # for Marshall.h; using that macro as uint8_t in the sketch truncates a pointer
        # (same bogus byte, e.g. 33, on every board flashed with the same .hex).
        _replace_numeric_define(env, "EEPROM_HW_VERSION_NUM", int(hw_version))

    if os.environ.get("EEPROM_NET_FORCE_HW_ID"):
        env.Append(CPPDEFINES=[("EEPROM_NET_FORCE_HW_ID",)])

    mac6 = os.environ.get("EEPROM_NET_MAC_6")
    mac5 = os.environ.get("EEPROM_NET_MAC_5")
    if mac6 is None or mac5 is None:
        mac5_def = _resolve_define_value(env, "MAC_5")
        mac6_def = _resolve_define_value(env, "MAC_6")
        ip_wx_raw = _resolve_define_value(env, "IP_WX")
        if ip_wx_raw is None:
            raise RuntimeError("IP_WX must be defined in lab env build_flags for eeprom-net writer")
        ip_wx = int_from_cpp_define(ip_wx_raw, "IP_WX")
        resolved_mac5, resolved_mac6 = resolve_mac5_mac6(mac5_def, mac6_def, ip_wx)
        if mac5 is None:
            mac5 = str(resolved_mac5)
        if mac6 is None:
            mac6 = str(resolved_mac6)
    if mac6:
        _replace_numeric_define(env, "MAC_6", int(mac6))
    if mac5:
        _replace_numeric_define(env, "MAC_5", int(mac5))
