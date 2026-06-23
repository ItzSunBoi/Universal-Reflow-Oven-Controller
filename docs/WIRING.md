# Default wiring for the CS-less 240x240 display

Change these assignments in `Config.h` if they do not match your PCB.

## Why two SPI buses are required

The 1.3-inch display exposes `GND VCC SCL SDA RES DC BLK` and has no chip-select pin. Its ST7789 controller is therefore permanently selected. It must not share `SCL/SCK` or `SDA/MOSI` with the MAX31865, because MAX31865 traffic would also be clocked into the display.

The ESP32-S3 has two independent general-purpose SPI controllers. This project uses:

- **FSPI** for the ST7789 display
- **HSPI** for the MAX31865

The two buses use different physical clock and data pins.

## ST7789 display, FSPI

| Display pin | ESP32-S3 GPIO | Notes |
|---|---:|---|
| GND | GND | Common logic ground |
| VCC | 3.3 V | Verify the module voltage before connection |
| SCL | 12 | TFT SPI clock |
| SDA | 11 | TFT MOSI/data, not I2C SDA |
| RES | 8 | Display reset |
| DC | 9 | Command/data selection |
| BLK | 7 | Backlight control; drive appropriately for the module |

There is deliberately no TFT CS definition. The Adafruit constructor receives `-1` for CS.

## MAX31865, HSPI

| MAX31865 pin | ESP32-S3 GPIO | Meaning |
|---|---:|---|
| GND | GND | Common logic ground |
| CLK | 14 | HSPI clock |
| SDO | 13 | MAX31865 data out to ESP32 MISO |
| SDI | 10 | ESP32 MOSI to MAX31865 data in |
| CS | 21 | Active-low chip select |
| RDY | not connected | Optional data-ready output; firmware polls instead |

### MAX31865 power pins

Many modules with both `VIN` and `3V3` are Adafruit-style boards where `VIN` feeds an onboard regulator and `3V3` is the regulator output. Do not connect both power pins together.

Before powering the board, check its product page or trace the regulator:

- If `3V3` is explicitly an input, power it from ESP32 3.3 V and leave `VIN` open.
- If `3V3` is a regulator output, power `VIN` according to the module specification and leave `3V3` open.
- Adafruit-style boards normally accept 3–5 V on `VIN`, but an unverified clone should not be assumed to have identical protection or level shifting.

The MAX31865 logic signals must remain compatible with 3.3 V ESP32 GPIO.

## User buttons

| Function | GPIO | Wiring |
|---|---:|---|
| Left | 4 | Momentary switch to GND |
| Middle | 5 | Momentary switch to GND |
| Right | 6 | Momentary switch to GND |

The firmware enables internal pull-ups. External 10 kOhm pull-ups are useful in an electrically noisy oven enclosure.

## E-stop

| Function | GPIO | Wiring |
|---|---:|---|
| Normally-closed E-stop input | 15 | NC contact to GND, 10 kOhm pull-up to 3.3 V |

Healthy wiring produces LOW. Pressing the E-stop, disconnecting the switch, or breaking the wire produces HIGH and latches a fault.

For real emergency-stop behavior, use a dual-contact E-stop:

- one NC contact for GPIO15 fault detection
- one NC contact in the hardware SSR-enable, contactor-coil, or heater-power safety chain

## SSR

| Function | GPIO | Wiring |
|---|---:|---|
| SSR command | 16 | Through a suitable transistor/opto interface to SSR input |

Do not assume every SSR input is directly compatible with 3.3 V. Verify required input voltage and current. Add a defined hardware pull-down so the SSR remains off while the ESP32 is unpowered or resetting.

The firmware is intended for a **zero-cross AC SSR** and uses slow time-proportioned control, not high-frequency PWM.

## Optional outputs

| Function | GPIO | Notes |
|---|---:|---|
| Active buzzer | 17 | Drive through a transistor if required |
| Cooling fan | 18 | Use a MOSFET/relay driver and flyback diode for inductive loads |

## ESP32-S3 pin cautions

- GPIO19 and GPIO20 are commonly used for native USB.
- GPIO0, GPIO3, GPIO45, and GPIO46 have boot/strapping considerations.
- The default map avoids those pins.
- Recheck every pin against the exact ESP32-S3 carrier board, not only the WROOM module datasheet.
