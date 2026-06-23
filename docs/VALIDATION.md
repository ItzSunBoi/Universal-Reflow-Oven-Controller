# Validation status

## Completed checks

- All project `.cpp` and `.ino` sources compile and link in the local C++17 Arduino/Adafruit API-shape validation environment.
- Validation flags: `-Wall -Wextra -Wpedantic`.
- No compiler warnings were produced.
- `UiManager` renders to a 240x240 RGB565 framebuffer rather than clearing the physical LCD.
- Page changes use one complete frame transfer.
- Stable pages transmit only changed 24x24 tiles.
- Tile hashing uses one framebuffer rather than two, limiting additional UI RAM use.
- `CslessST7789::pushImage()` supports cropped, stride-based RGB565 transfers.
- Button scanning runs in a FreeRTOS task on core 0 with a thread-safe event queue.
- The Arduino loop retains a cooperative button fallback if task creation fails.
- The display and MAX31865 remain on separate SPI controllers and physical buses.
- NVS profile format remains unchanged, preserving existing profiles and settings.
- Connector-group isolation remains unchanged.

## Still required on the physical ESP32-S3

1. Compile with the installed Arduino-ESP32 3.x core and current Adafruit libraries.
2. Confirm startup reports `Button scanner: asynchronous core task` over serial.
3. Press and release each button repeatedly while changing pages and while the running graph updates.
4. Verify there is no visible black clear frame during updates.
5. Observe whether partial tile updates produce any residual tearing at 10 MHz.
6. If required, test 4 MHz and a 250-500 ms UI refresh interval.
7. Check free heap after startup because the framebuffer consumes 115,200 bytes.
8. Complete heater, SSR, sensor, and thermal-safety commissioning with mains isolated first.

The default thermal profiles and PID values remain starting templates and must be validated for the actual oven and solder paste.
