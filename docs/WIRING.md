# Connector-optimized wiring for the reused ESP32-S3 carrier PCB

This map treats each slash-separated pin group as one physical connector set. It prioritizes one connector per module and avoids sharing connector groups between active modules.

## Connector groups

| Group | GPIO pins | Power available | Assigned module |
|---|---|---|---|
| A | 13, 14, 35 | 3.3 V + GND | Dedicated E-stop |
| B | 36, 3, 21, 47, 48, 46, 45 | Header group | Dedicated optional buzzer on GPIO21 |
| C | 4, 5, 6, 42 | 5 V + GND | Three-button control panel |
| D | 10, 11, 12, 41 | 5 V + GND | CS-less ST7789 display |
| E | 8, 9, 18, 40 | 5 V + GND | MAX31865 |
| F | 39, 17, 16, 15 | 5 V + GND | Dedicated SSR interface on GPIO16 |
| G | 1, 2, 7, 38 | 5 V + GND | Dedicated optional cooling fan on GPIO38 |

GPIO3, GPIO45, and GPIO46 are intentionally unused because they are ESP32-S3 strapping pins. The design also leaves GPIO42, GPIO14, GPIO35, GPIO36, GPIO47, GPIO48, GPIO39, GPIO17, GPIO15, GPIO1, GPIO2, and GPIO7 available for later expansion.

## ST7789 display — connector group D

| Display pin | ESP32-S3 connection |
|---|---|
| SCL | GPIO12 |
| SDA | GPIO11 |
| DC | GPIO41 |
| BLK | GPIO10 PWM |
| RES | Hardware reset, described below |
| GND | Group D GND |
| VCC | Group D supply only if the module is rated for it |

The module already contains the backlight power MOSFET, so GPIO10 drives BLK
directly as a logic-level PWM signal. Do not add another power transistor unless
the module documentation requires one.

### Display reset without consuming another GPIO

The preferred arrangement is to connect display `RES` to the ESP32 board's
`EN`/reset signal, so both devices reset together. If EN is not available, use
a local power-on reset: connect RES to 3.3 V through 10 kOhm and add 100 nF from
RES to GND. Do not pull RES up to 5 V unless the exact display module explicitly
states that the pin is 5 V tolerant. The firmware passes `-1` as the ST7789 reset
pin and therefore does not drive RES.

This leaves SCL, SDA, DC, and BLK as the four signals in group D; the display
still consumes only one JST signal group.

## MAX31865: connector group E

The MAX31865 uses its own HSPI controller because the display is permanently selected.

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

Each momentary button connects its GPIO to GND when pressed. GPIO42 remains spare. The 5 V pin is not required by a passive button panel.

## Dedicated E-stop: connector group A

| Signal | Connection |
|---|---|
| E-stop sense | GPIO13 |
| Pull-up supply | 3.3 V |
| Return | GND |

Use a normally-closed E-stop contact from GPIO13 to GND and an external 10 kΩ pull-up from GPIO13 to 3.3 V. Healthy wiring reads LOW. Pressing the E-stop or breaking the cable reads HIGH and triggers the interrupt.

For a real emergency stop, use a dual-contact E-stop. The second normally-closed contact must interrupt the hardware SSR-enable, contactor coil, or heater-power safety chain independently of firmware.

## Dedicated SSR interface: connector group F

| Signal | Connection |
|---|---:|
| SSR command | GPIO16 |
| Driver supply | 5 V if required by the interface |
| Ground | GND |

Use an appropriate transistor or optocoupler interface. Add a hardware pull-down so the SSR command remains off during reset or loss of ESP32 power. GPIO39, GPIO17, and GPIO15 remain spare in this connector.

## Optional buzzer: group B

The firmware assigns the optional buzzer to GPIO21. This avoids the strapping pins in the same header group. Set `PIN_BUZZER = -1` in `Config.h` if no buzzer is fitted.

## Optional cooling fan: group G

The firmware assigns the cooling-fan driver to GPIO38. Use a MOSFET or relay driver and a flyback diode for an inductive fan or relay coil. Set `PIN_COOLING_FAN = -1` if unused.
