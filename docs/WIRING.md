# Wiring v1.8

No wiring changes are required from v1.7.

## Connector allocation

| Group | GPIOs | Exclusive module |
|---|---|---|
| A | 13, 14, 35 plus 3.3 V/GND | Display extension: BLK PWM on GPIO13 |
| B | 36, 3, 21, 47, 48, 46, 45 | Optional buzzer on GPIO21 |
| C | 4, 5, 6, 42 plus 5 V/GND | Three-button panel |
| D | 10, 11, 12, 41 plus 5 V/GND | Display control and SPI |
| E | 8, 9, 18, 40 plus 5 V/GND | MAX31865 |
| F | 39, 17, 16, 15 plus 5 V/GND | SSR interface |
| G | 1, 2, 7, 38 plus 5 V/GND | Optional fan |

A module may use more than one connector group, but a connector group is not shared between different modules.

## Display

| Module pin | Connection |
|---|---|
| GND | GND |
| VCC | Module-rated supply |
| SCL | GPIO12 |
| SDA | GPIO11 |
| RES | GPIO10 |
| DC | GPIO41 |
| BLK | GPIO13 |

BLK is a 20 kHz 3.3 V PWM control signal for the module's onboard backlight MOSFET.

## MAX31865

| Module pin | Connection |
|---|---|
| VIN | Approved module supply |
| GND | GND |
| CLK | GPIO8 |
| SDO | GPIO9 |
| SDI | GPIO18 |
| CS | GPIO40 |
| RDY | Not connected |
| 3V3 | Leave unconnected when it is the breakout regulator output |

Do not connect VIN and 3V3 together. Confirm the exact breakout power arrangement.

## Buttons

Each button connects its input to GND when pressed:

- Left: GPIO4
- Middle: GPIO5
- Right: GPIO6

## SSR AO3400A driver

- GPIO16 -> 100 ohm -> AO3400A gate
- 10 kohm from gate to GND
- source -> low-voltage GND
- drain -> SSR input negative
- SSR input positive -> 5 V

The ESP32 supply and SSR input supply must share low-voltage ground. Do not place a flyback diode across the optically isolated SSR input.

## OTA

OTA adds no external hardware. The ESP32-S3 creates a temporary local access point using its internal Wi-Fi radio.
