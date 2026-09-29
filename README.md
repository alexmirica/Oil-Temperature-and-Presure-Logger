# Oil / Temperature / Battery Monitor

Arduino-based monitoring and data-logging firmware for an **Arduino Uno**.

The project provides real-time monitoring of oil pressure, oil temperature and battery voltage on a 16×2 LCD, with optional SD-card CSV logging and a serial command interface.

## Features

* Oil pressure monitoring
* Oil temperature monitoring using an NTC sensor
* Battery voltage monitoring
* Analog keypad input
* 16×2 LCD runtime display
* SD card data logging
* Automatic sequential CSV file creation
* Serial monitor output
* Serial SD-card file commands
* Operation without an SD card
* 1-second measurement/logging interval

## Hardware

### Arduino Uno

| Function        | Pin |
| --------------- | --: |
| Analog keypad   |  A0 |
| Oil pressure    |  A1 |
| Oil temperature |  A2 |
| Battery voltage |  A3 |
| SD card CS      | D10 |
| LCD RS          |  D8 |
| LCD Enable      |  D9 |
| LCD D4          |  D4 |
| LCD D5          |  D5 |
| LCD D6          |  D6 |
| LCD D7          |  D7 |

The project uses the Arduino SPI interface for the SD card.

## LCD Display

The 16×2 LCD displays the current operating information.

### Line 1

Runtime and battery voltage.

Example:

```text
12s  13.8V
```

### Line 2

Oil pressure, oil temperature and logging status.

Example:

```text
2.35bar 74.2C
```

## Oil Pressure

The pressure sensor uses a proportional **0.5–4.5 V** output.

| Sensor voltage | Display |
| -------------: | ------- |
|        < 0.5 V | `UV`    |
|          0.5 V | 0 bar   |
|          4.5 V | 6 bar   |
|        > 4.5 V | `OV`    |

The pressure range is therefore:

```text
0.5 V  → 0 bar
4.5 V  → 6 bar
```

The pressure value is calculated linearly between these limits.

## Oil Temperature

Oil temperature is measured using an NTC thermistor connected to **A2**.

The firmware converts the measured resistance/ADC value to temperature using the configured Steinhart-Hart calculation.

## Battery Voltage

Battery voltage is measured on **A3** through the project's resistor-divider circuit.

The displayed value is converted back to the corresponding battery voltage.

## Keypad

The analog keypad is connected to **A0**.

Supported functions:

* `UP`
* `DOWN`
* `LEFT / BACK`
* `RIGHT`
* `SELECT`

`SELECT` is used to start and stop SD-card logging.

`BACK` is also used on the SD-card error screen to continue operation without an SD card.

## SD Card

The SD card is initialized during startup.

If the card cannot be initialized, the monitor displays:

```text
SD Card Error
B=Ignore S=Retry
```

### Retry

Press `SELECT` to retry SD-card initialization.

### Ignore

Press `BACK` to continue without the SD card.

When the SD card is ignored:

* Data logging is disabled.
* SD-card serial commands are unavailable.
* Normal monitoring continues.
* The LCD and sensor functions remain operational.

If the SD card is successfully initialized, logging and SD-card commands become available.

## Data Logging

Press `SELECT` to start or stop logging when an SD card is available.

Log files are created automatically using sequential filenames:

```text
LOG001.CSV
LOG002.CSV
LOG003.CSV
...
```

Existing log files are not overwritten.

### CSV Format

```text
Time,OilP,OilT,Vbatt
```

A new measurement is written approximately every second while logging is active.

The CSV format is intended for easy import into spreadsheet and data-analysis software.

## Serial Monitor

Serial communication uses:

```text
115200 baud
```

During normal operation the monitor periodically outputs the current measurements.

Example:

```text
12s 13.8V 2.35bar 74.2C
```

The output contains:

* Runtime
* Battery voltage
* Oil pressure
* Oil temperature

## Serial Commands

The serial interface also provides SD-card file commands.

### `dir`

Lists files on the SD card.

```text
dir
```

### `list`

Displays the complete contents of a file.

```text
list LOG001.CSV
```

### `del`

Deletes a specific file.

```text
del LOG001.CSV
```

### `del *.csv`

Deletes CSV log files.

```text
del *.csv
```

### `help`

Displays the available serial commands.

```text
help
```

### `logon`

Starts serial logging.

```text
logon
```

### `logoff`

Stops serial logging.

```text
logoff
```

If the SD card was not initialized or was deliberately ignored, SD-card commands report:

```text
SD Card Error
```

## Project Structure

```text
.
├── .gitignore
├── LICENSE
├── README.md
├── platformio.ini
├── lib/
│   └── LiquidCrystal/
└── src/
    └── main.cpp
```

The project uses [PlatformIO](https://platformio.org/) with the Arduino framework for the Arduino Uno.

The SD library is provided through the PlatformIO dependency:

```text
arduino-libraries/SD
```

The project also contains the required LiquidCrystal library source under `lib/LiquidCrystal/`.

## Building

Open the project in PlatformIO and build the `uno` environment.

The configured environment is:

```ini
[env:uno]
platform = atmelavr
board = uno
framework = arduino
```

Build optimization and compiler warnings are enabled through:

```text
-Wall
-Wextra
-Os
```

## License

The project source is released under the **Tech Art Electronics non-commercial license** contained in the repository's `LICENSE` file.

Commercial use, including use in commercial products or services, requires separate permission from the copyright holder.

### Third-Party Software

This project includes the **New LiquidCrystal** library by Francisco Malpartida.

The LiquidCrystal library is licensed under the **GNU General Public License v3.0 (GPL-3.0)**. Its original license notice and attribution are retained in the library's `README.md`.

The third-party library remains subject to its own license terms.

## Author

**Alexandru-Bogdan Mirica**
**Tech Art Electronics**

Copyright © 2026 Tech Art Electronics