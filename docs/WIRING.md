# Connector-isolated wiring for the reused ESP32-S3 carrier PCB

This map follows two rules:

1. A module may use more than one physical connector group.
2. A connector group is never shared between different modules.

## Connector groups

| Group | GPIO pins | Power available | Assigned module |
|---|---|---|---|
| A | 13, 14, 35 | 3.3 V + GND | ST7789 backlight and optional display power |
| B | 36, 3, 21, 47, 48, 46, 45 | Header group | Optional buzzer on GPIO21 |
| C | 4, 5, 6, 42 | 5 V + GND | Three-button control panel |
| D | 10, 11, 12, 41 | 5 V + GND | ST7789 SPI/control signals |
| E | 8, 9, 18, 40 | 5 V + GND | MAX31865 |
| F | 39, 17, 16, 15 | 5 V + GND | SSR interface on GPIO16 |
| G | 1, 2, 7, 38 | 5 V + GND | Optional cooling fan on GPIO38 |

GPIO3, GPIO45, and GPIO46 are intentionally unused because they are ESP32-S3 strapping pins.

Groups A and D are both reserved exclusively for the display. Their unused GPIOs are not assigned to another module.

## ST7789 display: groups A and D

The display has no chip-select pin and therefore uses its own FSPI controller.

### Group D: display bus and control

| Display pin | Carrier connection |
|---|---:|
| SCL | GPIO12 |
| SDA | GPIO11 |
| RES | GPIO10 |
| DC | GPIO41 |

### Group A: backlight and power

| Display pin | Carrier connection |
|---|---:|
| BLK | GPIO13, 20 kHz PWM |
| VCC | 3.3 V if the display module is 3.3 V compatible |
| GND | GND |

GPIO14 and GPIO35 remain physically unused but group A is reserved to the display.

The module already contains the backlight power MOSFET, so GPIO13 only supplies the PWM logic signal. Confirm whether `BLK` is active-high. The firmware defaults to active-high and can be inverted using `TFT_BACKLIGHT_ACTIVE_HIGH` in `Config.h`.

If the display module requires 5 V on VCC, power it from a suitable 5 V rail instead while retaining GPIO13 as the 3.3 V PWM control signal. Do not apply 5 V to ESP32 GPIOs.

## MAX31865: connector group E

The MAX31865 uses its own HSPI controller because the CS-less display is permanently selected.

| MAX31865 pin | Carrier connection |
|---|---:|
| CLK | GPIO8 |
| SDO | GPIO9 |
| SDI | GPIO18 |
| CS | GPIO40 |
| RDY | Not connected |
| VIN | Connector 5 V only if the breakout VIN is rated for 5 V |
| GND | GND |
| 3V3 | Leave disconnected when it is a regulator output |

Confirm whether the breakout's `3V3` pin is an output or input before powering it. Do not connect VIN and 3V3 together.

## Three-button panel: connector group C

| Button | GPIO |
|---|---:|
| Left | GPIO4 |
| Middle | GPIO5 |
| Right | GPIO6 |

Each momentary button connects its GPIO to GND when pressed. GPIO42 remains spare but group C is reserved for the button panel.

## SSR interface: connector group F

| Signal | Connection |
|---|---:|
| SSR command | GPIO16 |
| Driver supply | 5 V if required by the interface |
| Ground | GND |

Use an appropriate transistor or optocoupler interface. Add a hardware pull-down so the SSR command remains off during reset or loss of ESP32 power. GPIO39, GPIO17, and GPIO15 remain unused but group F is reserved for the SSR interface.

## Optional buzzer: group B

The optional buzzer uses GPIO21. Set `PIN_BUZZER = -1` in `Config.h` if no buzzer is fitted. Group B is then unused and may be reassigned manually.

## Optional cooling fan: group G

The optional fan driver uses GPIO38. Use a MOSFET or relay driver and a flyback diode for an inductive fan or relay coil. Set `PIN_COOLING_FAN = -1` if unused.

## Emergency isolation

No GPIO E-stop is used in v1.7. For an emergency, disconnect the oven from mains power. Arrange the wiring so the accessible plug or switched socket removes power from the heater circuit itself.
