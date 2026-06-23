# Validation status

## Completed off-target checks

- All `.cpp`, `.h`, and `.ino` sources were compiled and linked with a local C++17 Arduino/Adafruit API-shape stub.
- Compiler flags included `-Wall -Wextra -Wpedantic`.
- No warnings were produced by that host-side check.
- The Arduino sketch folder and main `.ino` filename match.
- The shared-SPI code uses one `SPIClass` object passed to both Adafruit drivers.
- NVS profile data is versioned and CRC checked.

## Still required on real hardware

This package has not been compiled against the user's exact installed Arduino-ESP32 core and library versions, and it has not been exercised on the physical PCB. Before connecting mains power:

1. compile for `ESP32S3 Dev Module` with 16 MB flash and PSRAM disabled
2. verify the chosen board definition exposes all configured pins
3. test display and MAX31865 communication with the heater disconnected
4. scope the SSR command during boot, reset, upload, E-stop, and sensor-fault conditions
5. confirm the E-stop's independent hardware contact removes heater drive
6. calibrate the RTD chain and tune PID values using a sacrificial load
7. validate actual PCB temperature with an independent attached probe

The default reflow profiles are safe starting templates, not a substitute for the solder-paste manufacturer's profile or component thermal limits.
