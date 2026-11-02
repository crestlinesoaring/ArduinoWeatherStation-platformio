#!/usr/bin/env python3
"""Download wxYYYYMMDD.dat files from flymarshall.com for local use (e.g. wx-graph.html)."""

from __future__ import annotations

import argparse
import sys
import urllib.error
import urllib.request
from datetime import date, datetime, timedelta
from pathlib import Path

BASE_URL = "https://www.flymarshall.com/wx/{subdir}/wx{yyyymmdd}.dat"
VALID_SUBDIRS = ("betaOne", "betaTwo", "betaThree")
DEFAULT_OUT = Path(__file__).resolve().parent.parent / "web" / "wx" / "wx-data"
USER_AGENT = "awx-pio fetch_wx_dat"


def dat_url(subdir: str, day: date) -> str:
    return BASE_URL.format(subdir=subdir, yyyymmdd=day.strftime("%Y%m%d"))


def local_dat_path(subdir: str, day: date, output_dir: Path = DEFAULT_OUT) -> Path:
    return output_dir / subdir / f"wx{day.strftime('%Y%m%d')}.dat"


def fetch_to_file(url: str, dest: Path, timeout_s: float = 60.0) -> int:
    request = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    with urllib.request.urlopen(request, timeout=timeout_s) as response:
        data = response.read()
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_bytes(data)
    text = data.decode("utf-8", errors="replace")
    line_count = sum(1 for line in text.splitlines() if line.strip())
    return line_count


def ensure_dat(
    subdir: str,
    day: date,
    *,
    output_dir: Path = DEFAULT_OUT,
    refresh: bool = False,
    timeout_s: float = 60.0,
) -> Path:
    """Return path to cached .dat, downloading from flymarshall if missing or refresh."""
    if subdir not in VALID_SUBDIRS:
        raise ValueError(f"invalid subdir: {subdir!r}")
    dest = local_dat_path(subdir, day, output_dir)
    if refresh or not dest.is_file():
        fetch_to_file(dat_url(subdir, day), dest, timeout_s=timeout_s)
    return dest


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Download flymarshall wxYYYYMMDD.dat files (no browser CORS)."
    )
    parser.add_argument(
        "subdir",
        choices=VALID_SUBDIRS,
        help="data subfolder: betaOne, betaTwo, or betaThree",
    )
    parser.add_argument(
        "--date",
        help="Single day YYYY-MM-DD (default: today, local time)",
    )
    parser.add_argument(
        "--from",
        dest="from_date",
        metavar="YYYY-MM-DD",
        help="Start date for a range (inclusive)",
    )
    parser.add_argument(
        "--to",
        dest="to_date",
        metavar="YYYY-MM-DD",
        help="End date for a range (inclusive); default: same as --from",
    )
    parser.add_argument(
        "-o",
        "--output-dir",
        type=Path,
        default=DEFAULT_OUT,
        help=f"Directory for .dat files (default: {DEFAULT_OUT})",
    )
    parser.add_argument(
        "--flat",
        action="store_true",
        help="Write wxYYYYMMDD.dat directly in output-dir (default: output-dir/subdir/)",
    )
    return parser.parse_args(argv)


def iter_days(args: argparse.Namespace) -> list[date]:
    if args.from_date:
        start = datetime.strptime(args.from_date, "%Y-%m-%d").date()
        end_s = args.to_date or args.from_date
        end = datetime.strptime(end_s, "%Y-%m-%d").date()
        if end < start:
            raise SystemExit("--to must be on or after --from")
        days: list[date] = []
        cursor = start
        while cursor <= end:
            days.append(cursor)
            cursor += timedelta(days=1)
        return days

    if args.date:
        return [datetime.strptime(args.date, "%Y-%m-%d").date()]
    return [date.today()]


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv or sys.argv[1:])
    days = iter_days(args)

    failed = 0
    for day in days:
        url = dat_url(args.subdir, day)
        name = f"wx{day.strftime('%Y%m%d')}.dat"
        if args.flat:
            dest = args.output_dir / name
        else:
            dest = local_dat_path(args.subdir, day, args.output_dir)
        try:
            lines = fetch_to_file(url, dest)
        except urllib.error.HTTPError as exc:
            print(f"FAIL {name}: HTTP {exc.code} ({url})", file=sys.stderr)
            failed += 1
            continue
        except urllib.error.URLError as exc:
            print(f"FAIL {name}: {exc.reason} ({url})", file=sys.stderr)
            failed += 1
            continue
        print(f"OK   {dest}  ({lines} lines)  <- {url}")

    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
