# Firmware architecture

## Cooperative loop order

The firmware deliberately keeps the main control path non-blocking:

1. debounce and queue UI button events
2. sample the MAX31865 when its interval expires
3. process the latched E-stop state
4. update the reflow state machine and safety monitors
5. persist any completed run summary
6. update SSR time-proportioning output
7. update the optional cooling fan
8. redraw the current UI page when required

The dedicated E-stop ISR does not wait for this loop. It immediately writes the SSR command inactive and latches the heater inhibit.

## Core modules

- `ButtonInput`: three-button debounce, short press, long press, and auto-repeat.
- `Safety`: normally-closed E-stop interrupt and global heater-inhibit latch.
- `TemperatureSensor`: MAX31865 communication, filtering, calibration, and fault handling.
- `HeaterController`: PID and slow time-proportioned zero-cross SSR output.
- `ReflowEngine`: ramp/hold/cool state machine, live graph history, run metrics, and thermal safety checks.
- `ProfileStore`: CRC-protected NVS database for profiles, settings, and run summaries.
- `UiManager`: all 240x240 pages and three-button editors.

## Profile execution

A profile contains one to seven stages:

- **Ramp:** linearly moves the target from the previous stage target to the new target over the configured duration.
- **Hold:** immediately requests the configured target and holds it for the configured duration.
- **Cool:** ramps the displayed target downward, commands the heater off, and optionally enables the cooling fan.

An additional maximum upward target-ramp limit is applied to every heating stage. This prevents an edited profile from creating an instantaneous unsafe target jump.

## Storage

The ESP32-S3-WROOM-1-N16 does not require an external EEPROM for these settings. A roughly 2 KB CRC-protected database is stored as one NVS blob through Arduino `Preferences`.

The database contains:

- up to 8 profiles
- selected-profile index
- calibration and UI settings
- the newest 8 run summaries

A schema version mismatch or CRC failure restores the compiled factory profiles.

## UI inheritance

`docs/ui_reference_v1.png` is the original approved visual reference. The implementation preserves:

- dark near-black background
- cyan actual temperature
- green ready/completed state
- yellow targets and profile details
- orange heater information
- red faults
- rounded header and panel geometry
- fixed bottom row of three button legends
- original home, profile, running, complete, menu, manual, and fault page ordering
