# Universal Reflow Controller v1.2

Arduino project for an **ESP32-S3-WROOM-1-N16**, a 240x240 ST7789 SPI display, a MAX31865/PT100 temperature interface, three UI buttons, a dedicated normally-closed emergency-stop input, and a zero-cross AC SSR.

The interface preserves the original dark 240x240 design: cyan live values, green success, yellow profile/status information, orange heat output, red faults, fixed header placement, and fixed three-button legends along the bottom.

## Carrier PCB connector allocation

Version 1.2 is mapped specifically for the reused carrier PCB connector groups supplied by the user. The TFT, MAX31865, three-button panel, E-stop, SSR interface, optional buzzer, and optional fan each occupy one connector group and do not share active connector sets. See `WIRING.md` for the exact mapping.


## Implemented features

- Shared hardware SPI bus for the ST7789 and MAX31865, with separate chip-select pins.
- 240x240 UI pages for home, profiles, profile details, running graph, run details, completion, profile editor, stage editor, profile naming, menu, manual heat, calibration, logs, settings, about, and faults.
- Up to 8 profiles in ESP32 NVS flash.
- Add a profile by duplicating the selected profile, then rename and edit it.
- Up to 7 ramp, hold, or cool stages per profile.
- Stage insertion, deletion, reordering, target editing, duration editing, and mode selection.
- Three factory profiles for nominal 138 C, 180 C, and 217 C solder paste.
- Time-proportioned SSR output using a 2-second control window.
- PID control with anti-windup and derivative-on-measurement.
- Temperature calibration offset.
- MAX31865 open/short/fault handling.
- Latched E-stop GPIO interrupt.
- Global and per-profile overtemperature protection.
- Heater-response and conservative SSR-stuck monitoring.
- Last 8 completed run summaries saved to NVS.
- Optional active buzzer and cooling fan output.

## Required Arduino libraries

Install through **Arduino IDE > Library Manager**:

1. Adafruit GFX Library
2. Adafruit ST7735 and ST7789 Library
3. Adafruit MAX31865 library
4. Adafruit BusIO

The ESP32 `Preferences` and `SPI` libraries are included with the Espressif Arduino core.

## Arduino IDE settings

- Board: `ESP32S3 Dev Module`
- Flash Size: `16MB`
- PSRAM: `Disabled` for ESP32-S3-WROOM-1-N16
- CPU Frequency: `240 MHz`
- USB CDC On Boot: choose according to your programming connection
- Partition scheme: any 16 MB scheme retaining the normal NVS partition

Open `UniversalReflowController_v1_0.ino` from a folder with the same name.

## Configuration

All pins and major constants are in `Config.h`.

Important settings to verify before powering a heater:

```cpp
constexpr float RTD_REFERENCE_OHMS = 430.0f;
constexpr uint8_t RTD_WIRE_COUNT = 3;
constexpr bool SSR_ACTIVE_HIGH = true;
constexpr bool TFT_INVERT_COLORS = true;
```

Check the actual reference resistor fitted to the MAX31865 board. PT100 boards commonly use approximately 430 ohms; PT1000 boards require a different value and `RTD_NOMINAL_OHMS` must also be changed.

## Default button behavior

| Screen type | Left | Middle | Right |
|---|---|---|---|
| Normal/list | Back | Open/select | Down/next |
| Numeric edit | Decrease | Save | Increase |
| Name edit | Previous character | Next character | Next character |
| Running | Stop | Pause/resume | Information |
| Manual heat | Lower setpoint | On/off | Raise setpoint |

Hold the middle button on the manual page to stop heating and return to the menu. Hold the middle button in the name editor to save the name. Hold the right button on a fault page to reset after the fault condition has cleared.

## E-stop behavior

The firmware expects a **normally-closed** E-stop circuit:

- GPIO15 to the normally-closed contact
- other side of the contact to GND
- external 10 kOhm pull-up from GPIO15 to 3.3 V
- healthy circuit reads LOW
- pressed button or broken wire reads HIGH

The rising edge invokes an ISR which immediately commands the SSR output inactive and latches the fault. Firmware refuses reset while the E-stop input remains open.

A software interrupt is not a complete emergency-stop system. The E-stop should additionally remove the SSR input enable or heater contactor power in hardware.

## Control tuning

Initial PID values are in `HeaterController.h`:

```cpp
float kp_ = 5.0f;
float ki_ = 0.10f;
float kd_ = 18.0f;
```

These are safe starting values, not universal tuning. Oven power, thermal mass, fan placement, probe placement, and SSR cycle time all affect tuning. First test with the heater electrically isolated or with a low-risk test load.

## Profile model

Each profile contains:

- name
- liquidus temperature
- maximum allowed temperature
- maximum upward target ramp rate
- desired time above liquidus
- one to seven stages

Each stage contains:

- stage name
- mode: ramp, hold, or cool
- target temperature
- planned duration

The profile editor constrains values to avoid obviously invalid combinations, and the profile is validated again before a run starts.

## First-power checklist

1. Keep mains disconnected.
2. Confirm the SSR output is inactive during reset and boot.
3. Confirm opening the E-stop circuit immediately extinguishes the SSR input indicator.
4. Confirm the display and MAX31865 have different CS lines.
5. Confirm the PT100 reading at room temperature against a trusted thermometer.
6. Test the SSR with a low-voltage load or isolated indicator first.
7. Install an independent thermal fuse in series with the heater.
8. Keep all exposed mains conductors enclosed and earthed appropriately.
9. Tune and validate profiles using a thermocouple attached to a sacrificial PCB before processing valuable boards.

## Files

- `UniversalReflowController_v1_0.ino`: setup and cooperative main loop
- `Config.h`: pin mapping and hardware constants
- `Types.h`: profile, stage, fault, settings, and log structures
- `Safety.*`: E-stop ISR and heater inhibit latch
- `TemperatureSensor.*`: MAX31865/PT100 driver wrapper
- `HeaterController.*`: PID and SSR time proportioning
- `ReflowEngine.*`: profile state machine and safety monitors
- `ProfileStore.*`: NVS profile/settings/log persistence with CRC
- `ButtonInput.*`: debounce, short press, long press, and repeat events
- `UiManager.*`: complete 240x240 interface and editors
- `WIRING.md`: detailed default pin map and wiring notes
- `SAFETY.md`: hardware safety requirements
- `ARCHITECTURE.md`: module, state-machine, storage, and UI design
- `VALIDATION.md`: checks completed and real-hardware commissioning work remaining
- `docs/ui_reference_v1.png`: original approved 240x240 visual reference
