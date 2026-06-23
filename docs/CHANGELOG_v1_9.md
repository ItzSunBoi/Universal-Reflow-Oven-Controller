# Changelog v1.9.0

- Added a compile-time temperature backend toggle in `Config.h`.
- Added a temporary 100 kOhm B3950 NTC backend using the ESP32-S3 ADC.
- Retained the complete MAX31865/PT100 backend; set `USE_NTC_100K_SENSOR` to `0` to restore it.
- Added configurable NTC divider orientation, fixed resistor, beta value, nominal resistance, supply voltage, filtering, and ADC fault limits.
- Added NTC open-circuit, short-circuit, invalid-resistance, and invalid-temperature lockout.
- Reused GPIO9 as ADC1 input only while NTC mode is selected.
- Updated startup diagnostics, wiring, safety notes, and architecture documentation.
