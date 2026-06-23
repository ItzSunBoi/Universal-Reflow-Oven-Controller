# Changelog v1.8.1

- Fixed an Arduino core macro collision in `OtaManager::makeHex()`.
- Renamed the local hexadecimal lookup table from `HEX` to `HEX_DIGITS`.
- Added an explicit `uint8_t` nibble index.
- No changes to NVS data, profiles, OTA partitioning, UI behavior, PID autotune, or themes.
