"""Resolve station IP from PlatformIO env (IP_Q3 / IP_WX), shared by upload/snipe scripts."""

from __future__ import annotations

import re


def cpp_define(env, name: str):
    for item in env.get("CPPDEFINES", []):
        if isinstance(item, str):
            if item == name:
                return True
        elif isinstance(item, (list, tuple)) and item and item[0] == name:
            if len(item) == 1:
                return True
            return item[1]
    return None


def int_define(value, name: str) -> int:
    if isinstance(value, bool):
        raise ValueError(f"{name} is not numeric: {value!r}")
    if isinstance(value, int):
        return value
    match = re.search(r"-?\d+", str(value))
    if not match:
        raise ValueError(f"{name} is not numeric: {value!r}")
    return int(match.group())


def require_define(env, name: str) -> int:
    value = cpp_define(env, name)
    if value is None:
        raise RuntimeError(
            f"{name} is not defined for env '{env['PIOENV']}'. "
            f"Add e.g. -D {name}=227 to build_flags in platformio.ini."
        )
    try:
        return int_define(value, name)
    except ValueError as exc:
        raise RuntimeError(str(exc)) from exc


def resolve_station_host(env, arguments=None) -> str:
    """192.168.<IP_Q3>.<IP_WX>, or --upload-port when it looks like an IP."""
    arguments = arguments or {}
    upload_port = arguments.get("upload-port") or arguments.get("port")
    if upload_port and "." in str(upload_port):
        return str(upload_port).split(":", 1)[0]

    ip_q3 = require_define(env, "IP_Q3")
    ip_wx = require_define(env, "IP_WX")
    return f"192.168.{ip_q3}.{ip_wx}"
