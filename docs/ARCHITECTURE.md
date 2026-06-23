# Firmware architecture

## Runtime execution

### Main Arduino task

The main task performs:

1. drain queued button events and update UI state
2. sample the MAX31865 when due
3. update the reflow state machine and safety monitors
4. persist completed run summaries
5. update SSR time-proportioning output
6. update the optional cooling fan
7. render the UI when required

### Independent button task

`ButtonInput` starts a FreeRTOS task pinned to core 0. It scans the three GPIOs every 2 ms, performs debounce and hold timing, and places events in a thread-safe FreeRTOS queue.

The display may occupy the main task during an SPI transfer, but button state continues to be sampled. Events are consumed as soon as the main task returns.

## UI rendering pipeline

The physical LCD is never intentionally cleared before a normal redraw.

1. `UiManager` clears a 240x240 `GFXcanvas16` framebuffer in SRAM.
2. The complete page is drawn into that invisible buffer.
3. The screen is divided into 24x24 tiles.
4. FNV-1a hashes are calculated for all 100 tiles.
5. On a stable page, only tiles whose hashes changed are transmitted.
6. On a page transition, the complete finished framebuffer is transmitted once.

This requires one 115,200-byte RGB565 framebuffer. Tile hashes require only 400 additional bytes, avoiding the cost of a second framebuffer.

The technique addresses clear/redraw flicker while retaining the original Adafruit GFX drawing code and page geometry.

## Display driver

The custom `CslessST7789` driver uses:

- a dedicated FSPI controller
- no CS toggling
- SPI mode 2
- the initialization sequence proven on the physical display
- a stride-aware `pushImage()` method for complete frames and dirty tiles

The MAX31865 uses the separate HSPI controller because any traffic on a shared bus would also be interpreted by the permanently selected display.

## Other modules

- `Safety`: startup SSR inhibit and immediate software hard-off helper
- `TemperatureSensor`: MAX31865 communication, filtering, calibration, and faults
- `HeaterController`: PID and slow time-proportioned zero-cross SSR output
- `ReflowEngine`: ramp/hold/cool state machine, graph history, and safety monitors
- `ProfileStore`: CRC-protected NVS profiles, settings, and run summaries
- `BacklightController`: LEDC PWM for the BLK input
- `UiManager`: all pages, editors, idle brightness, framebuffer, and tile flushing

## Storage

Profiles and settings are stored in ESP32 NVS through Arduino `Preferences`. A schema mismatch or CRC failure restores the built-in defaults.

## Connector isolation

- Display: connector groups A and D
- MAX31865: group E
- Buttons: group C
- SSR: group F
- Optional buzzer: group B
- Optional fan: group G

No connector group is shared between different modules.
