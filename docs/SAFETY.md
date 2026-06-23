# Safety requirements

This firmware controls a heating appliance and cannot make an unsafe mains design safe.

## Required independent protections

- Correctly rated fuse at the mains input.
- Non-resettable thermal fuse physically coupled to the oven/heater region and wired in series with the heater.
- Properly rated zero-cross SSR from a reputable source, mounted to a heatsink where required.
- Hardware E-stop contact that interrupts the SSR-enable or contactor circuit independently of the ESP32.
- Protective earth bonded to all exposed conductive enclosure parts.
- Strain relief, mains-rated wire, insulated terminals, creepage/clearance, and a closed flame-resistant enclosure.
- SSR failure should be assumed to be potentially short-circuit/on.

## Firmware protections included

- SSR commanded off before peripheral initialization.
- Normally-closed E-stop monitored by GPIO interrupt and latched until manually reset.
- MAX31865 fault detection and invalid-sample lockout.
- Global 285 C ceiling.
- Per-profile temperature ceiling.
- Heater-response monitor.
- Conservative temperature-rise-while-off monitor.
- No blocking delays in the control loop.
- CRC-checked NVS profile database.

## Commissioning sequence

1. Test all logic from an isolated low-voltage supply with mains absent.
2. Measure the SSR GPIO during boot, reset, firmware upload, and E-stop operation.
3. Test the hardware safety chain without relying on firmware.
4. Use an isolated low-voltage load before connecting the heater.
5. Confirm the temperature sensor remains accurate throughout the oven's operating range.
6. Run a low-temperature empty-oven test.
7. Run a sacrificial-board test with an independent contact thermocouple.
8. Do not leave the oven unattended.
