# Safety requirements

This firmware controls a heating appliance and cannot make an unsafe mains design safe.

## Required independent protections

- Correctly rated fuse at the mains input.
- Non-resettable thermal fuse physically coupled to the oven/heater region and wired in series with the heater.
- Properly rated zero-cross SSR from a reputable source, mounted to a heatsink where required.
- Protective earth bonded to all exposed conductive enclosure parts.
- Strain relief, mains-rated wire, insulated terminals, creepage/clearance, and a closed flame-resistant enclosure.
- An accessible plug or switched outlet that disconnects the heater mains circuit.
- SSR failure should be assumed to be potentially short-circuit/on.

## Firmware protections included

- SSR commanded off before peripheral initialization.
- Startup software inhibit remains active until peripheral setup completes.
- Selected-sensor fault detection and invalid-sample lockout.
- NTC open/short, invalid-resistance, invalid-temperature, and ADC-range detection.
- MAX31865 fault detection remains available when that backend is selected.
- Global 285 C ceiling.
- Per-profile temperature ceiling.
- Heater-response monitor.
- Conservative temperature-rise-while-off monitor.
- No explicit delay-based waits in the control loop; display transfers are minimized through dirty-tile updates.
- CRC-checked NVS profile database.

## Emergency behavior

Version 1.7 intentionally has no GPIO emergency-stop input. The normal UI STOP action commands the SSR off, but emergency isolation is performed by unplugging the oven or switching off the outlet supplying the heater.

This only works as intended when the accessible disconnect removes power from the heater circuit. A separate controller supply must not leave the heater energized through another path.

## Commissioning sequence

1. Test all logic from an isolated low-voltage supply with mains absent.
2. Measure the SSR GPIO during boot, reset, and firmware upload.
3. Verify that unplugging or switching off the supply physically removes heater power.
4. Use an isolated low-voltage load before connecting the heater.
5. Confirm the temperature sensor remains accurate throughout the oven's operating range.
6. For NTC mode, independently verify several temperatures and confirm the probe, insulation, and cable are rated above the selected profile peak.
7. Run a low-temperature empty-oven test.
8. Run a sacrificial-board test with an independent contact probe.
9. Do not leave the oven unattended.


## OTA safety

- OTA can start only while the reflow engine is idle and PID autotune is inactive.
- Heater demand is forced to zero for the complete OTA session.
- Button cancellation is disabled while flash writing is in progress.
- A firmware update does not replace the required thermal fuse, mains fuse, grounding, enclosure, or accessible mains disconnect.
- Repeated Wi-Fi-start resets should be treated as a power-integrity fault until the reset reason and 3.3 V rail are verified. Do not disable brownout protection to conceal the symptom.
