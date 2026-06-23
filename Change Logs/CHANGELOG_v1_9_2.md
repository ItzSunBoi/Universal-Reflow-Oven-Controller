# v1.9.2

- Reduced OTA Wi-Fi transmit power to approximately 8.5 dBm by default.
- Removed the forced `WiFi.setSleep(false)` setting.
- Added staged AP startup, total-heap and contiguous-heap checks, and serial heap diagnostics.
- Added a persistent OTA-session marker and boot reset-reason diagnostics.
- OTA page now reports whether the previous OTA session ended in a reset and shows the last reset reason.
- OTA page refresh reduced to once per second.
- Backlight is reduced before Wi-Fi starts and restored when OTA exits.
- Added free-heap reporting on the active OTA page.
- No changes to profiles, PID, NVS profile schema, partition layout, or sensor backend.
