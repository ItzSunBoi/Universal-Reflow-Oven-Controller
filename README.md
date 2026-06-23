# Universal Reflow Controller v1.9.3

Arduino firmware for an ESP32-S3-WROOM-1-N16 reflow oven controller using:

- a 240x240 CS-less ST7789 display on dedicated FSPI;
- a compile-time selectable MAX31865/PT100 or 100 kOhm NTC temperature backend;
- a zero-cross AC SSR driven through an AO3400A;
- three push buttons;
- editable reflow profiles stored in ESP32 NVS flash.

The original dark Ocean UI layout, page order, three-button footer, mode-2 display transport, dirty-tile framebuffer, asynchronous button scanner, PWM backlight, inactivity dimming, and safety interlocks remain intact.

## Changes in v1.9.3

### UI text-fit audit

All page titles, status badges, footer labels, list rows, fixed instructions,
and dynamic profile, stage, fault, autotune, and OTA strings were checked
against their actual 240x240 drawing regions.

The UI now has width-aware fitted text, word-wrapped detail messages, reserved
header space for status badges, constrained footer labels, and panel-aware
profile-name rendering. Long fault details no longer collide with later rows,
and the OTA upload percentage no longer overlaps heap diagnostics. See
`UI_TEXT_AUDIT.md` for the full review.

### Requested hardware configuration

```cpp
constexpr uint32_t TFT_INIT_SPI_HZ = 4000000UL;
constexpr uint32_t TFT_SPI_HZ = 40000000UL;
constexpr int8_t PIN_COOLING_FAN = -1;
constexpr int8_t PIN_BUZZER = -1;
constexpr float RTD_NOMINAL_OHMS = 100.0f;
constexpr float RTD_REFERENCE_OHMS = 4300.0f;
constexpr uint8_t RTD_WIRE_COUNT = 2;
```

The NTC/MAX31865 compile-time selector and both sensor implementations remain
unchanged.

## Changes retained from v1.9.2

### Button-function audit and fixes

All page footers and handlers were checked together. The live PID autotune `INFO` button now opens an explanation page, while `DETAIL` opens live diagnostics including target, heater demand, cycles, and the latest peak/trough. The previously inactive fault `DETAIL` and OTA-session center controls are also implemented.

Unavailable actions now display `LOCKED` rather than appearing usable, the profile list no longer offers an add action when all slots are occupied, and the full mapping is documented in `BUTTON_AUDIT.md`.

### Selectable temperature sensor backend

`Config.h` now contains a single compile-time flag:

```cpp
#define USE_NTC_100K_SENSOR 1
```

- `1`: use a temporary 100 kOhm NTC thermistor on GPIO9.
- `0`: use the original MAX31865/PT100 implementation on HSPI.

The MAX31865 source code and configuration remain in the project. Switching back does not require restoring deleted files. The NTC backend supports configurable nominal resistance, nominal temperature, beta coefficient, fixed divider resistor, divider orientation, ADC averaging, filter strength, and open/short limits.

The default NTC configuration is for a 100 kOhm B3950 sensor and this divider:

```text
3.3 V --- NTC --- GPIO9 --- 2.2 kOhm (0.1%) --- GND
```

Add approximately 100 nF from GPIO9 to GND near the ESP32. The divider must use 3.3 V, never the carrier's 5 V rail. Use only a thermistor probe rated for the actual oven temperature.

## Features retained from v1.8.1

### Explicit local-browser OTA updates

`Settings -> OTA update -> START` creates a temporary password-protected Wi-Fi access point. The display shows its SSID, password, local address, progress, and session timeout.

The heater is forced off for the entire OTA session. Wi-Fi is otherwise disabled. The browser form accepts an ESP32 application `.bin`, writes it to the inactive OTA partition, verifies the image, then restarts into the new firmware.

A custom `partitions.csv` is included with two 3 MB application slots. Keep this file in the sketch folder when compiling.

### PID autotune

`Settings -> PID autotune` runs bounded relay-feedback tuning around a selectable target temperature:

- target range: 100.0 to 230.0 C;
- default target: 200.0 C;
- heater high output: 70%;
- hysteresis: +/-2.0 C;
- six measured cycles;
- independent overtemperature and timeout protection.

Calculated Kp, Ki, and Kd values are shown for review and are saved only after pressing `SAVE`. Existing values remain unchanged after cancel, failure, or power loss before saving.

Autotune should be performed with the oven in the physical configuration in which it will be used, with its normal sensor, insulation, airflow, and door position. Keep combustible materials and PCBs out of the oven during tuning.

### UI themes

`Settings -> Theme` cycles between:

- Ocean, the original approved color scheme;
- Ember;
- Forest;
- Mono.

Only colors change. Page geometry, control placement, ordering, and button behavior are preserved.

### Temperature formatting

Primary temperature readouts are horizontally centered and always show one decimal place. Temperature values elsewhere in profile and stage pages also use one decimal place.

## OTA workflow

1. Compile the project and export the application binary from Arduino IDE.
2. On the controller, open `Menu -> Settings -> OTA update`.
3. Press `START`.
4. Connect a phone or computer to the displayed `Reflow-XXXX` Wi-Fi network using the displayed password.
5. Open the displayed local address, normally `http://192.168.4.1`.
6. Select the exported application `.bin` and press `Install update`.
7. Keep power connected until the controller reports success and restarts.

The temporary session closes automatically after ten minutes when no upload is active. Uploading cannot be cancelled from the front-panel buttons once flash writing has begun.

## Required software

- Arduino IDE 2.x
- ESP32 Arduino core 3.2.1 or later recommended
- Adafruit GFX Library
- Adafruit MAX31865 Library
- Adafruit BusIO

`WiFi`, `WebServer`, `Update`, `Preferences`, and FreeRTOS are supplied by the ESP32 Arduino core.

## Recommended Arduino board settings

- Board: `ESP32S3 Dev Module`
- Flash size: `16MB (128Mb)`
- PSRAM: `Disabled`
- CPU frequency: `240MHz`
- USB CDC on boot: as preferred

The included custom `partitions.csv` should be selected automatically when it remains beside the `.ino` file. Confirm the build log reports the custom partition file and that the resulting application is below the 3 MB slot size.

## Display wiring

The display has no exposed chip-select and is permanently selected, so it uses its own SPI controller.

| Display | ESP32-S3 |
|---|---:|
| SCL | GPIO12 |
| SDA/MOSI | GPIO11 |
| DC | GPIO41 |
| RES | GPIO10 |
| BLK PWM | GPIO13 |
| GND | GND |
| VCC | Module-rated supply |

The custom driver uses SPI mode 2, no CS toggling, `COLMOD=0x05`, inversion on, normal mode on, then display on. v1.9.3 initializes at 4 MHz and uses 40 MHz for normal framebuffer transfers.

## Temperature sensor wiring

### Temporary 100 kOhm NTC mode

With `USE_NTC_100K_SENSOR` set to `1`:

| NTC divider node | ESP32-S3 |
|---|---:|
| Divider midpoint | GPIO9 |
| Divider supply | Regulated 3.3 V |
| Divider ground | GND |

Default arrangement:

```text
3.3 V --- 100 kOhm NTC --- GPIO9 --- 2.2 kOhm fixed resistor --- GND
                                      |
                                    100 nF
                                      |
                                     GND
```

The NTC and fixed-resistor values are configured in `Config.h`. If your divider is reversed, set `NTC_IS_HIGH_SIDE` to `false`. Do not feed GPIO9 from a divider powered by 5 V.

### MAX31865/PT100 mode

Set `USE_NTC_100K_SENSOR` to `0`; the original MAX31865 backend is retained unchanged.

| MAX31865 | ESP32-S3 |
|---|---:|
| CLK | GPIO8 |
| SDO | GPIO9 |
| SDI | GPIO18 |
| CS | GPIO40 |
| RDY | Not connected |
| GND | GND |

Confirm `RTD_REFERENCE_OHMS`, `RTD_NOMINAL_OHMS`, and `RTD_WIRE_COUNT` in `Config.h` match the breakout and probe.

## SSR interface

Use the AO3400A as a low-side driver:

- GPIO16 through approximately 100 ohms to MOSFET gate;
- 10 kohm gate-to-ground pull-down;
- MOSFET source to low-voltage ground;
- MOSFET drain to SSR input negative;
- SSR input positive to 5 V.

The firmware is configured for an active-high control command and a two-second time-proportioning window. The optional buzzer and cooling-fan outputs are disabled by default with pin value `-1`.

## Safety

This firmware is not a certified safety controller. Use an appropriately rated thermal fuse, mains fuse, grounded metal enclosure, strain relief, insulated terminals, and an accessible means of disconnecting heater power. Validate SSR failure behavior and never rely on software as the sole overtemperature protection.

Keep the SSR or mains heater disconnected during initial firmware, display, sensor, OTA, and UI testing.

## Main source files

- `UniversalReflowController_v1_9_3.ino`: initialization and main control loop
- `CslessST7789.*`: mode-2 no-CS display driver
- `UiManager.*`: UI, themes, centered temperatures, OTA and autotune pages
- `OtaManager.*`: temporary AP, browser upload, flash update, restart
- `PidAutotuner.*`: bounded relay-feedback PID autotune
- `ProfileStore.*`: profiles, settings, logs, CRC, and NVS migration
- `TemperatureSensor.*`: selectable NTC or MAX31865 backend, filtering, calibration, and faults
- `HeaterController.*`: PID and SSR time-proportioning output
- `ReflowEngine.*`: stage execution and run logging
- `ButtonInput.*`: asynchronous button scanner and event queue
- `partitions.csv`: dual-application OTA partition table
- `BUTTON_AUDIT.md`: page-by-page three-button behavior matrix
- `tools/verify_button_contracts.py`: static regression audit for button mappings
- `tools/verify_ui_text_layout.py`: static text-width and layout contract audit
- `UI_TEXT_AUDIT.md`: page text review and corrected overflow cases


## OTA stability diagnostics (v1.9.2)

The OTA access point now starts at reduced Wi-Fi transmit power, no longer
forces Wi-Fi sleep off, stages radio initialization, checks free heap, and
reduces backlight brightness while active. If the controller resets during an
OTA session, the next OTA page displays the recorded reset reason such as
`BROWNOUT`, `TASK WDT`, or `PANIC`. Serial output also reports free heap before
and after Wi-Fi startup.

If `BROWNOUT` or `POWER GLITCH` is reported, improve the 3.3 V supply and local
decoupling rather than disabling the brownout detector.
