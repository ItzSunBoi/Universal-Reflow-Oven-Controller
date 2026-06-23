# Default wiring

Change these assignments in `Config.h` if they do not match your PCB.

## Shared SPI bus

| Signal | ESP32-S3 GPIO | ST7789 | MAX31865 |
|---|---:|---|---|
| SCK | 12 | SCL/SCK | CLK/SCK |
| MOSI | 11 | SDA/MOSI | SDI/MOSI |
| MISO | 13 | normally unused | SDO/MISO |
| Display CS | 10 | CS | no connection |
| Display DC | 9 | DC | no connection |
| Display reset | 8 | RST | no connection |
| Display backlight | 7 | BL/LED through suitable drive | no connection |
| MAX31865 CS | 14 | no connection | CS |
| 3.3 V | 3V3 | VCC, subject to module design | VIN/VCC, subject to board design |
| Ground | GND | GND | GND |

The display **must have a controllable CS input** when sharing the bus. A display module with CS permanently tied active may interpret MAX31865 traffic as display commands. Use a display with exposed CS or place the display on a separate SPI controller.

Both CS pins should have pull-ups so the devices remain deselected during boot.

## User buttons

| Function | GPIO | Wiring |
|---|---:|---|
| Left | 4 | momentary switch to GND |
| Middle | 5 | momentary switch to GND |
| Right | 6 | momentary switch to GND |

The firmware enables internal pull-ups. External 10 kOhm pull-ups are optional but useful in an electrically noisy oven enclosure.

## E-stop

| Function | GPIO | Wiring |
|---|---:|---|
| Normally-closed E-stop input | 15 | NC contact to GND, 10 kOhm pull-up to 3.3 V |

Healthy wiring produces LOW. Pressing the E-stop, disconnecting the switch, or breaking the wire produces HIGH and latches a fault.

For real emergency-stop behavior, use a dual-contact E-stop:

- one NC contact for GPIO15 fault detection
- one NC contact in the hardware SSR-enable, contactor-coil, or heater-power safety chain

## SSR

| Function | GPIO | Wiring |
|---|---:|---|
| SSR command | 16 | through a suitable transistor/opto interface to SSR input |

Do not assume every SSR input is directly compatible with 3.3 V. Verify required input voltage and current. Use a transistor or optocoupler stage where necessary. Add a defined hardware pull-down so the SSR remains off while the ESP32 is unpowered or resetting.

The firmware is intended for a **zero-cross AC SSR** and uses slow time-proportioned control, not high-frequency PWM.

## Optional outputs

| Function | GPIO | Notes |
|---|---:|---|
| Active buzzer | 17 | drive through a transistor if required |
| Cooling fan | 18 | use a MOSFET/relay driver and flyback diode for inductive loads |

## ESP32-S3 pin cautions

- GPIO19 and GPIO20 are commonly used for native USB.
- GPIO0, GPIO3, GPIO45, and GPIO46 have boot/strapping considerations.
- GPIO35, GPIO36, and GPIO37 may be unavailable on ESP32-S3 module variants using Octal flash/PSRAM.
- The default map deliberately avoids those pins.
