# Universal Reflow Controller v1.7

Arduino project for an **ESP32-S3-WROOM-1-N16**, a 240x240 CS-less ST7789 display, MAX31865/PT100 interface, three-button control panel, PWM backlight, and zero-cross AC SSR.

The interface preserves the approved dark 240x240 design, color scheme, page placement, and fixed bottom button legends.

## What changed in v1.7

### Flicker-free framebuffer rendering

The UI no longer clears the physical LCD before redrawing it.

- A 240x240 RGB565 framebuffer is composed in ESP32 SRAM.
- Clearing and redrawing happen only in RAM, where they are invisible.
- The completed frame is divided into 24x24 tiles.
- Each tile is hashed and only changed tiles are sent to the display.
- Page changes are sent as one continuous complete frame, without first showing a black screen.
- Live pages normally update only the tiles containing changing temperatures, timers, progress, and graph data.

The framebuffer uses **115,200 bytes** of internal RAM. The ESP32-S3-WROOM-1-N16 has no PSRAM requirement for this design, but avoid adding other very large static buffers without checking free heap.

### Independent button scanner

Button input no longer depends on the Arduino `loop()` reaching a polling call.

- A FreeRTOS task scans all three buttons every 2 ms.
- It runs on core 0, independently of display transfers on the main Arduino task.
- Debounced short, long, and repeat events are stored in a 32-event FreeRTOS queue.
- Display transfers can no longer cause brief presses to be missed.
- A cooperative fallback is retained if task creation fails.

## Display transport

The display uses the configuration proven on the physical module:

- no CS signal
- dedicated FSPI bus
- SPI mode 2, CPOL 1 and CPHA 0
- `COLMOD = 0x05`
- inversion on
- `NORON`, then `DISPON`
- initialization at 1 MHz
- normal drawing at 10 MHz

The custom `CslessST7789` driver now includes an optimized RGB565 rectangle-transfer method used by the framebuffer renderer.

## Connector allocation

A module may occupy multiple connector groups, but no group is shared between different modules.

| Group | Module |
|---|---|
| A + D | ST7789 display, reset and PWM backlight |
| B | Optional buzzer |
| C | Three-button panel |
| E | MAX31865 |
| F | SSR interface |
| G | Optional cooling fan |

See `WIRING.md` for exact pins.

## Implemented features

- Universal editable profiles stored in ESP32 NVS
- Up to 8 profiles and 7 stages per profile
- Ramp, hold, and cool stages
- Factory templates for nominal 138 C, 180 C, and 217 C solder pastes
- Profile duplication, renaming, editing, stage insertion, deletion, and reordering
- MAX31865/PT100 measurement and calibration offset
- PID control and 2-second SSR time-proportioning window
- Manual heating mode
- Run graphs and last 8 run summaries
- Sensor, overtemperature, heater-response, and SSR-stuck fault detection
- PWM brightness setting stored in NVS
- Configurable inactivity dimming and screen-off timer
- Optional buzzer and cooling fan

## Required Arduino libraries

Install through Arduino Library Manager:

1. Adafruit GFX Library
2. Adafruit MAX31865 Library
3. Adafruit BusIO

The ST7789 library is **not required** because this project contains its own driver for the CS-less mode-2 display.

## Arduino IDE settings

- Board: `ESP32S3 Dev Module`
- Flash Size: `16MB`
- PSRAM: `Disabled`
- CPU Frequency: `240 MHz`
- Arduino-ESP32 core: 3.x

Open `UniversalReflowController_v1_7.ino` from a folder named `UniversalReflowController_v1_7`.

## Rendering configuration

Important constants in `Config.h`:

```cpp
constexpr uint32_t TFT_INIT_SPI_HZ = 1000000UL;
constexpr uint32_t TFT_SPI_HZ = 10000000UL;
constexpr uint32_t UI_REFRESH_INTERVAL_MS = 200;
constexpr uint8_t UI_DIRTY_TILE_SIZE = 24;
```

If dynamic regions still show visible tearing, first reduce `TFT_SPI_HZ` to 4 MHz or increase `UI_REFRESH_INTERVAL_MS` to 250-500 ms. Do not reintroduce physical `fillScreen()` calls for routine updates.

## Button-task configuration

```cpp
constexpr uint32_t BUTTON_SCAN_INTERVAL_MS = 2;
constexpr uint16_t BUTTON_EVENT_QUEUE_LENGTH = 32;
constexpr uint16_t BUTTON_TASK_STACK_BYTES = 3072;
constexpr uint8_t BUTTON_TASK_PRIORITY = 2;
constexpr int8_t BUTTON_TASK_CORE = 0;
```

## Backlight behavior

- GPIO13 drives the module's BLK MOSFET input.
- Normal brightness is adjustable from 10% to 100%.
- Default inactivity dim: 20% after 60 seconds.
- Default screen off: 10 minutes.
- Any button wakes the display.
- A press from fully off is consumed as wake-only to avoid blind actions.
- Dimming is disabled during heating, pause, manual mode, completion, and faults.

## First-power checklist

1. Keep mains disconnected.
2. Confirm the display works and remains stable through live updates.
3. Confirm all button presses register during page transitions and full-screen drawing.
4. Confirm the MAX31865 reading at room temperature.
5. Confirm the SSR output remains inactive during boot, reset, and upload.
6. Test the SSR with a low-voltage load before connecting the oven heater.
7. Install an independent thermal fuse in series with the heater.
8. Validate temperature with a second attached probe before processing valuable boards.

## Main files

- `UniversalReflowController_v1_7.ino`: startup and main control loop
- `CslessST7789.*`: mode-2, no-CS display transport and framebuffer transfer
- `UiManager.*`: framebuffer composition, dirty-tile flushing, pages, and editors
- `ButtonInput.*`: independent FreeRTOS scanner and event queue
- `Config.h`: pins, timing, rendering, and hardware settings
- `TemperatureSensor.*`: MAX31865/PT100 wrapper
- `HeaterController.*`: PID and SSR output
- `ReflowEngine.*`: profile execution and thermal monitoring
- `ProfileStore.*`: CRC-protected NVS database
- `BacklightController.*`: PWM brightness control
