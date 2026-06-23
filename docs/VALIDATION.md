# Validation status v1.8

## Completed static/build checks

- Every project `.cpp` and the `.ino` translation unit were compiled as C++17 with warnings enabled against Arduino/ESP32/Adafruit API-shape stubs.
- All resulting objects linked into one validation executable.
- The validation executable ran successfully.
- NVS version-3 migration and version-4 default paths were source-reviewed.
- OTA heater arbitration, timeout, upload token, image-header check, progress, and restart paths were source-reviewed.
- PID autotune cancellation, sensor-fault, overtemperature, phase-timeout, total-timeout, review, and save paths were source-reviewed.
- The original Ocean palette remains the default; three additional palettes use the same geometry.
- Primary live temperature rendering uses the centered one-decimal helper.

## Not yet physically validated

- Compilation against the user's exact installed Arduino-ESP32 and library versions.
- Flashing with the included custom partition table.
- Browser uploads from each phone/computer platform.
- Power-loss behavior at every point during an OTA upload.
- PID autotune performance on the user's actual oven.
- Stability of the calculated gains over different PCB loads.
- Mains wiring, SSR thermal performance, thermal-fuse behavior, and enclosure safety.

Use ESP32 Arduino core 3.2.1 or later and perform first tests with the heater disconnected.
