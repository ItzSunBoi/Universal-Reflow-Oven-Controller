# Firmware architecture

## Cooperative loop order

The firmware deliberately keeps the main control path non-blocking:

1. debounce and queue UI button events
2. sample the MAX31865 when its interval expires
3. update the reflow state machine and thermal safety monitors
4. persist any completed run summary
5. update SSR time-proportioning output
6. update the optional cooling fan
7. redraw the current UI page when required

Before this loop begins, `Safety` forces the SSR command inactive and holds a startup inhibit until all peripherals have initialized.

## Core modules

- `ButtonInput`: three-button debounce, short press, long press, and auto-repeat.
- `Safety`: startup SSR inhibit and immediate software hard-off helper.
- `TemperatureSensor`: MAX31865 communication, filtering, calibration, and fault handling.
- `HeaterController`: PID and slow time-proportioned zero-cross SSR output.
- `ReflowEngine`: ramp/hold/cool state machine, live graph history, run metrics, and thermal safety checks.
- `ProfileStore`: CRC-protected NVS database for profiles, settings, and run summaries.
- `BacklightController`: LEDC PWM output for the display module's `BLK` MOSFET input.
- `CslessST7789`: dedicated no-CS ST7789 transport using SPI mode 2 and the proven command sequence, exposed as an Adafruit GFX-compatible display.
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

## Carrier connector isolation in v1.5

The physical pin map is organized around the reused carrier PCB rather than conventional dev-board header order.

- The CS-less TFT occupies connector groups A and D.
- The MAX31865 occupies group E.
- The button panel occupies group C.
- The SSR interface occupies group F.
- Optional buzzer and fan outputs occupy groups B and G.

No connector group is shared by different modules. The display is permitted to span two groups so it can retain both software reset and software PWM backlight control.


## Display protocol

The display controller is permanently selected because the module exposes no
CS pin. The firmware therefore gives it a dedicated FSPI controller and never
toggles a synthetic CS. The physical module was confirmed to require CPOL=1,
CPHA=0 (`SPI_MODE2`) on the ESP32-S3. Initialization runs at the proven 1 MHz;
after `DISPON`, normal UI drawing switches to 10 MHz so screen refreshes do not
dominate the control loop. Initialization deliberately follows the
working MicroPython implementation:

1. active-low hardware reset
2. `SWRESET` and 150 ms delay
3. `SLPOUT`
4. `COLMOD = 0x05`
5. rotation-specific `MADCTL` and 240x240 RAM offsets
6. `INVON`
7. `NORON`
8. `DISPON`

The custom class inherits `Adafruit_GFX`; therefore `UiManager` retains the
approved drawing and layout implementation without depending on Adafruit's
ST7789 transport layer.
