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


if env["PIOENV"].endswith("-eeprom-net"):
    writer_src = env.subst("$PROJECT_DIR/src/eeprom-net-writer")
    env.Replace(SRC_DIR=writer_src)
    env.Replace(PROJECT_SRC_DIR=writer_src)

    hw_version = os.environ.get("EEPROM_NET_HW_VERSION")
    if not hw_version:
        raw = _cpp_define_value(env, "HW_VERSION")
        if raw is not None and raw is not True:
            hw_version = str(_int_from_define(raw, "HW_VERSION"))
    if hw_version:
        _strip_cpp_define(env, "HW_VERSION")
        env.Append(CPPDEFINES=[("HW_VERSION", int(hw_version))])

    if os.environ.get("EEPROM_NET_FORCE_HW_ID"):
        env.Append(CPPDEFINES=[("EEPROM_NET_FORCE_HW_ID",)])

    mac6 = os.environ.get("EEPROM_NET_MAC_6")
    mac5 = os.environ.get("EEPROM_NET_MAC_5")
    if mac6 is None or mac5 is None:
        mac5_def = _cpp_define_value(env, "MAC_5")
        mac6_def = _cpp_define_value(env, "MAC_6")
        if mac5_def is True:
            mac5_def = None
        if mac6_def is True:
            mac6_def = None
        ip_wx_raw = _cpp_define_value(env, "IP_WX")
        if ip_wx_raw is None or ip_wx_raw is True:
            raise RuntimeError("IP_WX must be defined in lab env build_flags for eeprom-net writer")
        ip_wx = int_from_cpp_define(ip_wx_raw, "IP_WX")
        resolved_mac5, resolved_mac6 = resolve_mac5_mac6(mac5_def, mac6_def, ip_wx)
        if mac5 is None:
            mac5 = str(resolved_mac5)
        if mac6 is None:
            mac6 = str(resolved_mac6)
    if mac6:
        _strip_cpp_define(env, "MAC_6")
        env.Append(CPPDEFINES=[("MAC_6", int(mac6))])
    if mac5:
        _strip_cpp_define(env, "MAC_5")
        env.Append(CPPDEFINES=[("MAC_5", int(mac5))])
