# Changelog v1.9.1

## Button-function audit

- Fixed the live PID autotune `INFO` button, which previously had no handler.
- Replaced the non-interactive `RUNNING` footer item with a working `DETAIL` action.
- Added PID autotune help and live-diagnostics pages.
- Fixed the fault-screen `DETAIL` button and added a fault diagnostics page.
- Made the fault-screen `HOME` button functional while keeping the heater latched off.
- Replaced the inactive OTA `READY` footer with a working `INFO` action.
- Locked all OTA buttons explicitly while a flash write is in progress.
- Hid `+ Add profile` when all profile slots are occupied.
- Changed unavailable heating actions to `LOCKED` instead of displaying an action that cannot run.
- Changed empty/single-entry log navigation from `NEXT` to `NONE`.
- The last position in the name editor now shows `SAVE` and accepts a short press.
- Calibration wording is now sensor-neutral for MAX31865, NTC, and future thermocouple backends.

No NVS layout, profile data, PID values, OTA partition layout, sensor selection, or wiring assignments changed.
