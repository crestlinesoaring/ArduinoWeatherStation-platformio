# ArduinoWeatherStation

Please check Wiki   
https://github.com/crestlinesoaring/ArduinoWeatherStation/wiki   

## Building with PlatformIO

This project is built with [PlatformIO](https://platformio.org) targeting the
Arduino Mega 2560 (`megaatmega2560`). Third-party libraries are vendored under
`libraries/` (no submodules), so a fresh clone builds without extra setup.

Quick start (Windows, COM5):

    pio run -e gb5_lab -t upload --upload-port COM5
    pio device monitor -e gb5_lab --port COM5

See **[docs/platformio.md](docs/platformio.md)** for build environments, custom
targets (`eeprom-net`, `eeprom-blank`, `tftp-upload`), feature flags, and typical
workflows.

## Project layout

    src/            sketch sources (ArduinoWeatherStation.ino + tabs, pins.h, Marshall.h)
    libraries/      vendored third-party libraries, committed directly in the repo
    platformio.ini  build configuration (board, framework, library paths)