#!/usr/bin/env python3
"""Fetch and analyze flymarshall.com weather .dat files for upload health."""

from __future__ import annotations

import argparse
import re
import sys
import urllib.error
import urllib.request
from collections import Counter
from datetime import date, datetime, timedelta
from pathlib import Path

BASE_URL = "https://www.flymarshall.com/wx/{subdir}/wx{yyyymmdd}.dat"
UPLOAD_RETRIES_RE = re.compile(r"(?:^|,)UploadRetries=(\d+)(?:,|$)")
REBOOT_MARKER_RE = re.compile(r"(?:^|,)R(?:,|$)")


def fetch_dat(url: str, timeout_s: float = 30.0) -> list[str]:
    request = urllib.request.Request(url, headers={"User-Agent": "awx-pio wx_dat_stats"})
    with urllib.request.urlopen(request, timeout=timeout_s) as response:
        text = response.read().decode("utf-8", errors="replace")
    return [line.strip() for line in text.splitlines() if line.strip()]


def parse_record(line: str) -> dict | None:
    parts = line.split(",")
    if len(parts) < 11:
        return None
    time_s = parts[0].strip()
    date_s = parts[1].strip()
    if ":" not in time_s or "/" not in date_s:
        return None
    try:
        hour_s, minute_s = time_s.split(":", 1)
        month_s, day_s, year_s = date_s.split("/", 2)
        when = datetime(
            int(year_s),
            int(month_s),
            int(day_s),
            int(hour_s),
            int(minute_s),
        )
    except ValueError:
        return None

    retry_match = UPLOAD_RETRIES_RE.search(line)
    return {
        "when": when,
        "time": time_s,
        "date": date_s,
        "version": parts[10].strip(),
        "has_reboot_marker": bool(REBOOT_MARKER_RE.search(line)),
        "upload_retries": int(retry_match.group(1)) if retry_match else None,
        "line": line,
    }


def analyze(records: list[dict], target: date, now: datetime | None = None) -> dict:
    now = now or datetime.now()
    day_records = [r for r in records if r["when"].date() == target]
    day_records.sort(key=lambda r: r["when"])

    by_minute: dict[datetime, list[dict]] = {}
    for record in day_records:
        key = record["when"].replace(second=0, microsecond=0)
        by_minute.setdefault(key, []).append(record)

    retry_lines = [r for r in day_records if r["upload_retries"] is not None]
    retry_counts = Counter(r["upload_retries"] for r in retry_lines)

    duplicate_minutes = sorted(
        (minute, len(entries))
        for minute, entries in by_minute.items()
        if len(entries) > 1
    )

    reboots: list[tuple[datetime, str, str]] = []
    previous_version: str | None = None
    for record in day_records:
        version = record["version"]
        if previous_version is not None and version != previous_version:
            reboots.append((record["when"], previous_version, version))
        previous_version = version

    if day_records:
        span_start = day_records[0]["when"]
        span_end = day_records[-1]["when"]
    else:
        span_start = datetime.combine(target, datetime.min.time())
        span_end = span_start

    if target == now.date():
        expected_end = now.replace(second=0, microsecond=0)
    else:
        expected_end = datetime.combine(target, datetime.max.time()).replace(
            hour=23, minute=59, second=0, microsecond=0
        )

    expected_start = datetime.combine(target, datetime.min.time())
    if day_records:
        expected_start = min(expected_start, span_start)

    missing: list[datetime] = []
    cursor = expected_start
    while cursor <= expected_end:
        if cursor not in by_minute:
            missing.append(cursor)
        cursor += timedelta(minutes=1)

    reboot_marker_minutes = sum(1 for r in day_records if r["has_reboot_marker"])

    return {
        "target": target,
        "url_suffix": target.strftime("%Y%m%d"),
        "total_lines": len(day_records),
        "unique_minutes": len(by_minute),
        "retry_lines": len(retry_lines),
        "retry_counts": retry_counts,
        "duplicate_minutes": duplicate_minutes,
        "reboots": reboots,
        "reboot_marker_lines": reboot_marker_minutes,
        "missing": missing,
        "span_start": span_start if day_records else None,
        "span_end": span_end if day_records else None,
        "expected_end": expected_end,
    }


def format_minute(when: datetime) -> str:
    return when.strftime("%H:%M")


def print_report(stats: dict, url: str, subdir: str, show_missing_limit: int) -> None:
    target = stats["target"]
    print(f"Subfolder: {subdir}")
    print(f"URL: {url}")
    print(f"Date: {target.isoformat()} ({target.strftime('%A')})")
    print()

    if stats["total_lines"] == 0:
        print("No records found for this date.")
        return

    print("Coverage")
    print(f"  lines:          {stats['total_lines']}")
    print(f"  unique minutes: {stats['unique_minutes']}")
    print(
        f"  span:           {format_minute(stats['span_start'])}"
        f" – {format_minute(stats['span_end'])}"
    )
    print(
        f"  expected through {format_minute(stats['expected_end'])}"
    )
    print(f"  missing minutes:{len(stats['missing'])}")
    if stats["missing"]:
        shown = stats["missing"][:show_missing_limit]
        missing_text = ", ".join(format_minute(m) for m in shown)
        if len(stats["missing"]) > show_missing_limit:
            missing_text += f", ... (+{len(stats['missing']) - show_missing_limit} more)"
        print(f"    {missing_text}")
    print()

    print("Upload retries")
    print(f"  lines with UploadRetries: {stats['retry_lines']}")
    if stats["retry_counts"]:
        for value, count in sorted(stats["retry_counts"].items()):
            print(f"    UploadRetries={value}: {count}")
    else:
        print("    none")
    print(f"  duplicate timestamps:     {len(stats['duplicate_minutes'])}")
    for minute, count in stats["duplicate_minutes"]:
        print(f"    {format_minute(minute)} x{count}")
    print()

    print("Reboots (firmware VERSION_ID change)")
    if stats["reboots"]:
        for when, old_version, new_version in stats["reboots"]:
            print(
                f"  {when.strftime('%H:%M')}  {old_version} -> {new_version}"
            )
    else:
        print("  none")
    print(
        f"  lines with ,R marker: {stats['reboot_marker_lines']}"
        "  (includes post-reboot uploads, not only hardware resets)"
    )


VALID_SUBDIRS = ("betaOne", "betaTwo", "betaThree")


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Analyze flymarshall wxYYYYMMDD.dat upload health stats."
    )
    parser.add_argument(
        "subdir",
        choices=VALID_SUBDIRS,
        help="data subfolder: betaOne, betaTwo, or betaThree",
    )
    parser.add_argument(
        "--date",
        help="Target date YYYY-MM-DD (default: today, local time)",
    )
    parser.add_argument(
        "--file",
        type=Path,
        help="Read a local .dat file instead of fetching from the web",
    )
    parser.add_argument(
        "--show-missing",
        type=int,
        default=24,
        help="Max missing minute timestamps to print (default: 24)",
    )
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv or sys.argv[1:])

    if args.date:
        target = datetime.strptime(args.date, "%Y-%m-%d").date()
    else:
        target = date.today()

    yyyymmdd = target.strftime("%Y%m%d")
    url = BASE_URL.format(subdir=args.subdir, yyyymmdd=yyyymmdd)

    try:
        if args.file:
            lines = [
                line.strip()
                for line in args.file.read_text(encoding="utf-8", errors="replace").splitlines()
                if line.strip()
            ]
            url = str(args.file.resolve())
        else:
            lines = fetch_dat(url)
    except urllib.error.HTTPError as exc:
        print(f"Failed to fetch {url}: HTTP {exc.code}", file=sys.stderr)
        return 1
    except urllib.error.URLError as exc:
        print(f"Failed to fetch {url}: {exc.reason}", file=sys.stderr)
        return 1
    except OSError as exc:
        print(f"Failed to read {args.file}: {exc}", file=sys.stderr)
        return 1

    records = [parsed for line in lines if (parsed := parse_record(line)) is not None]
    stats = analyze(records, target)
    print_report(stats, url, args.subdir, args.show_missing)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
