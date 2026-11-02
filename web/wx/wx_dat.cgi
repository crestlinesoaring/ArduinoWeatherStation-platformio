#!/usr/bin/env python3
"""
CGI: fetch wxYYYYMMDD.dat from flymarshall.com (cached in ./wx-data/).

Upload the whole web/wx/ directory to wildc.net, e.g. public_html/wx/
  wx_dat.cgi   — this script (chmod 755)
  index.html   — graphs
  wx-data/     — writable cache (betaOne|Two|Three/wxYYYYMMDD.dat)
  .htaccess    — enables CGI in this directory

Query: site=betaThree&date=2026-11-01  optional refresh=1
Env override: WX_DAT_CACHE=/absolute/path
"""

from __future__ import annotations

import os
import sys
import traceback
import urllib.error
import urllib.request
from datetime import date, datetime
from pathlib import Path

BASE_URL = "https://www.flymarshall.com/wx/{subdir}/wx{yyyymmdd}.dat"
VALID_SUBDIRS = frozenset({"betaOne", "betaTwo", "betaThree"})
USER_AGENT = "wildc.net wx_dat.cgi"
HERE = Path(__file__).resolve().parent


def dat_url(subdir: str, day: date) -> str:
    return BASE_URL.format(subdir=subdir, yyyymmdd=day.strftime("%Y%m%d"))


def fetch_to_file(url: str, dest: Path, timeout_s: float = 60.0) -> None:
    request = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    with urllib.request.urlopen(request, timeout=timeout_s) as response:
        data = response.read()
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_bytes(data)


def local_dat_path(subdir: str, day: date, output_dir: Path) -> Path:
    return output_dir / subdir / f"wx{day.strftime('%Y%m%d')}.dat"


def ensure_dat(
    subdir: str,
    day: date,
    output_dir: Path,
    *,
    refresh: bool = False,
) -> Path:
    if subdir not in VALID_SUBDIRS:
        raise ValueError("site must be betaOne, betaTwo, or betaThree")
    dest = local_dat_path(subdir, day, output_dir)
    if refresh or not dest.is_file():
        fetch_to_file(dat_url(subdir, day), dest)
    return dest


def cache_dir() -> Path:
    raw = os.environ.get("WX_DAT_CACHE")
    if raw:
        return Path(raw)
    return HERE / "wx-data"


def parse_date(raw: str) -> date:
    raw = raw.strip()
    if not raw:
        raise ValueError("missing date")
    if len(raw) == 8 and raw.isdigit():
        return datetime.strptime(raw, "%Y%m%d").date()
    if len(raw) == 10 and raw[4] == "-" and raw[7] == "-":
        return datetime.strptime(raw, "%Y-%m-%d").date()
    raise ValueError("date must be YYYY-MM-DD or YYYYMMDD")


def param(name: str) -> str:
    from urllib.parse import parse_qs

    qs = os.environ.get("QUERY_STRING", "")
    values = parse_qs(qs, keep_blank_values=True).get(name, [])
    return values[0] if values else ""


def respond(status: int, body: str) -> None:
    status_text = {200: "OK", 400: "Bad Request", 502: "Bad Gateway"}.get(
        status, "Error"
    )
    sys.stdout.write(f"Status: {status} {status_text}\r\n")
    sys.stdout.write("Content-Type: text/plain; charset=utf-8\r\n")
    sys.stdout.write("\r\n")
    sys.stdout.write(body)


def main() -> None:
    try:
        site = param("site").strip()
        date_raw = param("date").strip()
        refresh = param("refresh").strip().lower() in ("1", "true", "yes")

        if not site:
            respond(400, "missing site (betaOne, betaTwo, or betaThree)\n")
            return

        day = parse_date(date_raw)
        out = cache_dir()
        out.mkdir(parents=True, exist_ok=True)
        path = ensure_dat(site, day, out, refresh=refresh)
        respond(200, path.read_text(encoding="utf-8", errors="replace"))
    except ValueError as exc:
        respond(400, f"{exc}\n")
    except urllib.error.HTTPError as exc:
        respond(502, f"flymarshall HTTP {exc.code}\n")
    except urllib.error.URLError as exc:
        respond(502, f"flymarshall: {exc.reason}\n")
    except OSError as exc:
        respond(502, f"{exc}\n")
    except Exception:
        respond(502, "internal error\n")
        traceback.print_exc(file=sys.stderr)


if __name__ == "__main__":
    main()
