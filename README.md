# Universal Reflow Controller v1.8

Arduino firmware for an ESP32-S3-WROOM-1-N16 reflow oven controller using:

- a 240x240 CS-less ST7789 display on dedicated FSPI;
- a MAX31865/PT100 interface on dedicated HSPI;
- a zero-cross AC SSR driven through an AO3400A;
- three push buttons;
- editable reflow profiles stored in ESP32 NVS flash.

The original dark Ocean UI layout, page order, three-button footer, mode-2 display transport, dirty-tile framebuffer, asynchronous button scanner, PWM backlight, inactivity dimming, and safety interlocks remain intact.

## Changes in v1.8

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

The first installation of v1.8 must be performed over USB so Arduino can write the included dual-slot partition table. An OTA application image cannot change the partition table itself. After that first USB installation, later v1.8-compatible releases can be installed through the browser workflow. Avoid enabling an "erase all flash" upload option if you want to retain NVS profiles and logs.

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

The custom driver uses the proven configuration: SPI mode 2, no CS toggling, `COLMOD=0x05`, inversion on, normal mode on, then display on.

## MAX31865 wiring

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

The firmware is configured for an active-high control command and a two-second time-proportioning window.

## Safety

This firmware is not a certified safety controller. Use an appropriately rated thermal fuse, mains fuse, grounded metal enclosure, strain relief, insulated terminals, and an accessible means of disconnecting heater power. Validate SSR failure behavior and never rely on software as the sole overtemperature protection.

Keep the SSR or mains heater disconnected during initial firmware, display, sensor, OTA, and UI testing.

## Main source files

- `UniversalReflowController_v1_8.ino`: initialization and main control loop
- `CslessST7789.*`: mode-2 no-CS display driver
- `UiManager.*`: UI, themes, centered temperatures, OTA and autotune pages
- `OtaManager.*`: temporary AP, browser upload, flash update, restart
- `PidAutotuner.*`: bounded relay-feedback PID autotune
- `ProfileStore.*`: profiles, settings, logs, CRC, and NVS migration
- `HeaterController.*`: PID and SSR time-proportioning output
- `ReflowEngine.*`: stage execution and run logging
- `ButtonInput.*`: asynchronous button scanner and event queue
- `partitions.csv`: dual-application OTA partition table
