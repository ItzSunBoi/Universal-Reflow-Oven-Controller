# Firmware architecture

## Runtime execution

### Main Arduino task

The main task performs:

1. drain queued button events and update UI state
2. sample the selected NTC or MAX31865 backend when due
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

### Text fitting

`UiManager` measures the classic Adafruit GFX character grid before drawing.
Headers reserve space for status badges, buttons and list rows constrain text
to their interiors, and dynamic details can wrap across two lines with an
ellipsis on the final line. This prevents custom names and runtime fault or OTA
messages from escaping their panels.

## Display driver

The custom `CslessST7789` driver uses:

- a dedicated FSPI controller
- no CS toggling
- SPI mode 2
- the initialization sequence proven on the physical display
- a stride-aware `pushImage()` method for complete frames and dirty tiles

When the MAX31865 backend is selected, it uses the separate HSPI controller because any traffic on a shared bus would also be interpreted by the permanently selected display. In NTC mode, GPIO9 becomes an ADC1 input and HSPI is not started.

## Other modules

- `Safety`: startup SSR inhibit and immediate software hard-off helper
- `TemperatureSensor`: compile-time selectable MAX31865/PT100 or 100 kOhm NTC backend, filtering, calibration, and faults
- `HeaterController`: PID and slow time-proportioned zero-cross SSR output
- `ReflowEngine`: ramp/hold/cool state machine, graph history, and safety monitors
- `ProfileStore`: CRC-protected NVS profiles, settings, and run summaries
- `BacklightController`: LEDC PWM for the BLK input
- `UiManager`: all pages, editors, idle brightness, framebuffer, and tile flushing

## Storage

Profiles and settings are stored in ESP32 NVS through Arduino `Preferences`. A schema mismatch or CRC failure restores the built-in defaults.

## Connector isolation

- Display: connector groups A and D
- Selected temperature sensor backend: group E
- Buttons: group C
- SSR: group F
- Optional buzzer: group B
- Optional fan: group G

No connector group is shared between different modules.


## OTA stability path

OTA is an explicit local SoftAP session. Before the radio starts, the firmware forces the heater off, reduces backlight load, checks total and contiguous internal heap, records a persistent session marker, and stages Wi-Fi startup. RF transmit power defaults to approximately 8.5 dBm.

If the ESP32 resets before OTA is closed normally, the marker survives reboot. `esp_reset_reason()` is then shown on the OTA page and printed over Serial, allowing brownout, watchdog, panic, and power-glitch resets to be distinguished.
