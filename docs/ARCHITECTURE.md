# Architecture v1.8

## Runtime ownership

- The Arduino main loop owns the temperature sensor, reflow engine, heater demand, safety gates, OTA service, UI state, and display flushing.
- A FreeRTOS task pinned to core 0 scans the three buttons every 2 ms and pushes debounced events into a queue.
- The display is composed into a 240x240 RGB565 framebuffer. Only changed 24x24 tiles are transferred.

## SPI separation

The CS-less display is permanently selected and therefore uses FSPI exclusively. The MAX31865 uses HSPI with its own CS. The buses do not share SCK or MOSI.

## Persistent data

`ProfileStore` stores a CRC-protected database in NVS. Database version 4 adds:

- persistent PID Kp, Ki, and Kd;
- selected UI theme.

A valid v1.7/version-3 database is migrated while preserving profiles, logs, calibration, backlight, idle settings, buzzer, and fan preferences.

## OTA state machine

`OFF -> READY -> UPLOADING -> SUCCESS`

Any setup, authorization, image, or flash error enters `ERROR`. Wi-Fi is disabled in `OFF`.

Starting OTA:

1. verifies that an inactive OTA application partition exists;
2. forces the SSR demand and physical output off;
3. generates a random SoftAP SSID, password, and upload token;
4. starts a local HTTP server.

The upload handler requires the random token, checks the `.bin` suffix and ESP32 image magic byte, writes through the ESP32 `Update` API, finalizes the inactive partition, then restarts. OTA activity is an independent hard heater inhibit in the main loop.

## PID autotune state machine

`IDLE -> PREHEAT -> COOLING <-> HEATING -> COMPLETE`

Fault or cancellation produces `FAULT` or `ABORTED` and immediately forces the heater off.

The tuner applies 70% output until crossing target plus hysteresis, then 0% until crossing target minus hysteresis. It measures peak, trough, and high-crossing period. The first oscillation is discarded; later cycles are averaged. It calculates classical Ziegler-Nichols PID values in the same continuous-time units used by `HeaterController`.

The result is not applied or persisted until the user presses `SAVE`.

## UI themes

Theme selection swaps one runtime palette containing background, panel, line, text, muted, and accent colors. Rendering geometry and navigation are shared by all themes.

## Heater arbitration

Demand priority is:

1. OTA active: 0%;
2. PID autotune active: tuner demand;
3. otherwise: reflow engine demand.

The SSR remains inhibited for sensor invalidity, global safety inhibit, reflow fault, OTA activity, or autotune fault.
