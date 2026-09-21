# PlatformIO guide

This project builds the Arduino Weather Station firmware for an **Arduino Mega 2560**
using [PlatformIO](https://platformio.org) on Windows. Configuration lives in
`platformio.ini`; sketch sources are under `src/`.

Run commands from **PowerShell**, **cmd**, or the PlatformIO IDE terminal in the
project root.

## Prerequisites

- [PlatformIO Core](https://platformio.org/install/cli) or PlatformIO IDE extension
  for VS Code
- **`src/Marshall.h`** — copy from `src/Marshall.h.example` and edit for your site.
  This file is not committed to git.
- **Serial upload / monitor** — USB cable to the Mega; note the COM port in Device
  Manager (e.g. **COM5**)
- **TFTP upload** — `curl` on your PATH (included in Windows 10/11)
- **EEPROM network writer** — Python package `pyserial` (used by the `eeprom-net` target)

Fresh clone: libraries are vendored under `libraries/`, so no extra library setup is needed.

## Build environments

| Environment | Purpose |
|---|---|
| `mega` | Default. Uses network/site settings from `Marshall.h`. |
| `tavis` | Tavis lab board — IP `192.168.2.227`, HW version 7, betaTwo |
| `gb5_lab` | Brain box 5 — IP `192.168.222.222`, HW version 5, betaOne |
| `gb6_lab` | Brain box 6 — IP `192.168.222.226`, HW version 6, betaThree |
| `*-eeprom-net` | Internal one-shot writer sketches (used by `eeprom-net`, not run directly) |

Lab envs (`tavis`, `gb5_lab`, `gb6_lab`) override station settings via `build_flags`
in `platformio.ini` instead of `Marshall.h`.

Pick an environment with `-e`:

```powershell
pio run -e gb5_lab
```

If you omit `-e`, the default env `mega` is built.

## Everyday commands

```powershell
# Build
pio run -e gb5_lab

# Upload over USB serial (avrdude)
pio run -e gb5_lab -t upload

# Serial monitor (115200 baud, logs to logs\)
pio device monitor -e gb5_lab

# Upload, then monitor
pio run -e gb5_lab -t upload -t monitor

# List all targets for an env (upload, eeprom-net, tftp-upload, etc.)
pio run -e gb5_lab --list-targets
```

### Serial port (COM5)

Find the port in **Device Manager → Ports (COM & LPT)**. Set it in `platformio.ini`
(`upload_port` / `monitor_port`) or pass it on the command line:

```powershell
pio run -e gb5_lab -t upload --upload-port COM5
pio device monitor -e gb5_lab --port COM5
```

Example in `platformio.ini` (uncomment or add under `[env:gb5_lab]`):

```ini
upload_port = COM5
monitor_port = COM5
```

## Build outputs

After a successful build:

| File | Description |
|---|---|
| `.pio\build\<env>\firmware.hex` | Intel HEX for serial upload |
| `.pio\build\<env>\firmware.hex.bin` | Raw binary (for TFTP / Athena bootloader) |
| `src\version.h` | Auto-generated at build time from git (`git_rev.py`) |

The `.hex.bin` file is produced automatically by `scripts/pio_hex_to_bin.py` on every
compile.

## Custom targets

These are project-specific PlatformIO tasks beyond the usual `upload` and `monitor`.

### `eeprom-net` — write Athena network settings

Available on lab envs: `tavis`, `gb5_lab`, `gb6_lab`.

Burns IP, gateway, subnet, MAC, and TFTP port into the Mega's **internal EEPROM**
(Athena bootloader block, bytes 0–69). Reads `IP_GW`, `IP_Q3`, and `IP_WX` from
the env's `build_flags` to form addresses like `192.168.<IP_Q3>.<IP_WX>`.

```powershell
pio run -e gb5_lab -t eeprom-net --upload-port COM5
```

Options:

- `--force-hw-id` — re-write hardware ID byte 75 from `HW_VERSION`
- `--upload-port COM5` — serial port (required unless set in `platformio.ini`)

On success, re-flash the main firmware:

```powershell
pio run -e gb5_lab -t upload --upload-port COM5
```

Verify logs are saved under `.pio\eeprom-net-logs\`.

### `eeprom-blank` — recover corrupted EEPROM

Available on all envs (via `env:mega`).

Writes `0xFF` to all 4096 EEPROM bytes over serial. Use this when `eeprom-net`
reports *cells cannot be updated in place* (EEPROM was cleared to zeros and AVR
cells can no longer be programmed without a full erase).

```powershell
pio run -e gb5_lab -t eeprom-blank --upload-port COM5
pio run -e gb5_lab -t eeprom-net --upload-port COM5
pio run -e gb5_lab -t upload --upload-port COM5
```

### `tftp-upload` — flash over Ethernet (Athena bootloader)

Available on lab envs: `tavis`, `gb5_lab`, `gb6_lab`.

Uploads `firmware.hex.bin` via TFTP using curl. The board must **already be in
Athena TFTP/update mode** before you run this (the station is normally sleeping;
wake it and enter TFTP mode manually — telnet `M`, etc.).

Target IP is derived from `IP_Q3` and `IP_WX` in the env's `build_flags`, or
override with `--upload-port`:

```powershell
# Board already in TFTP mode at 192.168.222.222
pio run -e gb5_lab -t tftp-upload

# Override IP
pio run -e gb5_lab -t tftp-upload --upload-port 192.168.222.222
```

Equivalent manual command:

```powershell
curl -T .pio\build\gb5_lab\firmware.hex.bin tftp://192.168.222.222/firmware.hex.bin
```

The script retries the TFTP put up to 3 times. Add PlatformIO verbose for curl `-v`:

```powershell
pio run -e gb5_lab -t tftp-upload -v
```

## Feature flags

Common toggles in `platformio.ini` under `[env:mega] build_flags`:

| Flag | Effect |
|---|---|
| `ANEMO_WS85` | WS85 ultrasonic anemometer on Serial1 (on by default) |
| `DONT_SLEEP` | Skip night-time power save (on by default) |
| `BENCH_MODE` | Stay awake; upload over Ethernet every 5 minutes |
| `ENABLE_HARDWARE_SIMULATION` | Enable `SIMULATE_*` sensor fakes |
| `SERIAL_TIMESTAMPS` | Prefix serial output with timestamps |

Individual `SIMULATE_*` values are still set in the HARDWARE SIMULATION SETTINGS
block of `ArduinoWeatherStation.ino`.

Lab envs also set per-board flags such as `HW_VERSION`, `WX_BETA_TEXT`, `IP_GW`,
`IP_Q3`, `IP_WX`, and optionally `MAC_6`.

## Serial monitor logging

All envs use `monitor_filters = default, log2file_env`. Each monitor session writes a
timestamped log to:

```
logs\<env>-device-monitor-YYMMDD-HHMMSS.log
```

## Project layout

```
src\                  Sketch sources (.ino tabs, pins.h, Marshall.h)
libraries\            Vendored third-party libraries
scripts\              PlatformIO helper scripts (hex→bin, EEPROM, TFTP)
monitor\              Custom serial monitor filters
platformio.ini        Build environments and flags
docs\platformio.md    This file
```

PlatformIO merges `.ino` files in `src/` like the Arduino IDE: main file first
(`ArduinoWeatherStation.ino`), then remaining files alphabetically.

## Typical workflows

### Local dev (USB)

```powershell
pio run -e gb5_lab -t upload -t monitor --upload-port COM5
```

### New lab board setup

```powershell
pio run -e gb5_lab -t eeprom-net --upload-port COM5
pio run -e gb5_lab -t upload --upload-port COM5
```

### Field firmware update (TFTP, board already in update mode)

```powershell
pio run -e gb5_lab -t tftp-upload
```

### EEPROM recovery

```powershell
pio run -e gb5_lab -t eeprom-blank --upload-port COM5
pio run -e gb5_lab -t eeprom-net --upload-port COM5
pio run -e gb5_lab -t upload --upload-port COM5
```
