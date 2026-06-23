# v1.9.3

## UI text-fit audit

- Added width-aware text rendering for headers, status badges, footer buttons,
  centered labels, list rows, profile names, and dynamic status/detail text.
- Added two-line word wrapping with final-line ellipsis for fault descriptions.
- Prevented title/status overlap on run, autotune, OTA, delete, and fault pages.
- Shortened several built-in headings so they retain their intended font size.
- Reworked the fault diagnostics page so long details do not collide with
  sensor, temperature, heater, or footer elements.
- Removed an OTA upload-page collision between the progress percentage and
  heap diagnostics.
- Added `tools/verify_ui_text_layout.py` and `UI_TEXT_AUDIT.md`.

## Requested configuration changes

- TFT initialization SPI: 4 MHz.
- TFT runtime SPI: 40 MHz.
- Cooling fan output disabled with pin `-1`.
- Buzzer output disabled with pin `-1`.
- RTD nominal resistance: 100 ohms.
- RTD reference resistance: 4300 ohms.
- RTD wire mode: 2-wire.

No profile schema, NVS layout, control algorithm, sensor-selection flag, OTA
partition map, or button behavior was changed.
