# Firmware architecture

## Cooperative loop order

The firmware deliberately keeps the main control path non-blocking:

1. debounce and queue UI button events
2. sample the MAX31865 when its interval expires
3. update the reflow state machine and thermal safety monitors
4. persist any completed run summary
5. update SSR time-proportioning output
6. update the optional cooling fan
7. update the idle dim/off backlight state
8. redraw the current UI page when required

Before this loop begins, `Safety` forces the SSR command inactive and holds a startup inhibit until all peripherals have initialized.

## Core modules

- `ButtonInput`: three-button debounce, short press, long press, and auto-repeat.
- `Safety`: startup SSR inhibit and immediate software hard-off helper.
- `TemperatureSensor`: MAX31865 communication, filtering, calibration, and fault handling.
- `HeaterController`: PID and slow time-proportioned zero-cross SSR output.
- `ReflowEngine`: ramp/hold/cool state machine, live graph history, run metrics, and thermal safety checks.
- `ProfileStore`: CRC-protected NVS database for profiles, settings, and run summaries.
- `BacklightController`: LEDC PWM output for the display module's `BLK` MOSFET input.
- `UiManager`: all 240x240 pages, three-button editors, and the inactivity state machine.

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
- normal brightness, idle-dim delay, screen-off delay, and dim brightness
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

## Carrier connector isolation

The physical pin map is organized around the reused carrier PCB rather than conventional dev-board header order.

- The CS-less TFT occupies connector groups A and D.
- The MAX31865 occupies group E.
- The button panel occupies group C.
- The SSR interface occupies group F.
- Optional buzzer and fan outputs occupy groups B and G.

No connector group is shared by different modules. The display is permitted to span two groups so it can retain both software reset and software PWM backlight control.

## Backlight inactivity state machine

The UI owns a three-state backlight policy:

- **ACTIVE:** configured normal brightness
- **DIMMED:** configured idle brightness after the dim timeout
- **OFF:** zero PWM duty after the screen-off timeout

Any button resets the inactivity timer. A press while dimmed restores normal brightness and continues with the requested UI action. A press while fully off restores the display but consumes that first event, avoiding accidental profile starts or setting changes while the screen was invisible.

Running, paused, manual heating, completion, and fault states force the backlight to remain at normal brightness. When the backlight is fully off on an idle page, UI redraws are suspended to avoid wasting SPI bandwidth.

The three inactivity settings reuse bytes reserved in the v1.5 NVS structure, so the database size and schema version remain unchanged. Existing profiles and logs are migrated in place by initializing zero-valued reserved bytes to the new defaults.
