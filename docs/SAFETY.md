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
- MAX31865 fault detection and invalid-sample lockout.
- Global 285 C ceiling.
- Per-profile temperature ceiling.
- Heater-response monitor.
- Conservative temperature-rise-while-off monitor.
- No blocking delays in the control loop.
- CRC-checked NVS profile database.

## Emergency behavior

Version 1.4 intentionally has no GPIO emergency-stop input. The normal UI STOP action commands the SSR off, but emergency isolation is performed by unplugging the oven or switching off the outlet supplying the heater.

This only works as intended when the accessible disconnect removes power from the heater circuit. A separate controller supply must not leave the heater energized through another path.

## Commissioning sequence

1. Test all logic from an isolated low-voltage supply with mains absent.
2. Measure the SSR GPIO during boot, reset, and firmware upload.
3. Verify that unplugging or switching off the supply physically removes heater power.
4. Use an isolated low-voltage load before connecting the heater.
5. Confirm the temperature sensor remains accurate throughout the oven's operating range.
6. Run a low-temperature empty-oven test.
7. Run a sacrificial-board test with an independent contact probe.
8. Do not leave the oven unattended.
