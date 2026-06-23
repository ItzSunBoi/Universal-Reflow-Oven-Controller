# Validation status

## Completed off-target checks

- All `.cpp`, `.h`, and `.ino` sources were compiled and linked with a local C++17 Arduino/Adafruit API-shape stub.
- Compiler flags included `-Wall -Wextra -Wpedantic`.
- The host-side check completed without warnings after removing one unused display-size constant.
- The project folder and main `.ino` filename match.
- The CS-less ST7789 and MAX31865 use separate `SPIClass` objects and separate physical SPI pins.
- The display has both a software reset GPIO and a software PWM backlight GPIO.
- All GPIO E-stop references and interrupt code have been removed from the firmware.
- No active connector group is assigned to more than one module.
- NVS profile data remains versioned and CRC checked.
- A source hash manifest is included in the package.

## Still required on real hardware

This package has not been compiled against the user's exact installed Arduino-ESP32 core and library versions, and it has not been exercised on the physical PCB. Before connecting mains power:

1. compile for `ESP32S3 Dev Module` with 16 MB flash and PSRAM disabled
2. verify the chosen board definition exposes all configured pins
3. test display reset, display PWM brightness, and MAX31865 communication with the heater disconnected
4. scope the SSR command during boot, reset, upload, and sensor-fault conditions
5. verify that unplugging or switching off the supply physically removes heater power
6. calibrate the RTD chain and tune PID values using a sacrificial load
7. validate actual PCB temperature with an independent attached probe

The default reflow profiles are starting templates, not a substitute for the solder-paste manufacturer's profile or component thermal limits.
