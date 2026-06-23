# Safety notes v1.8

## Independent protection remains mandatory

Use a non-software thermal fuse in intimate thermal contact with the oven, an appropriately rated mains fuse, protective earthing, insulated mains terminals, strain relief, and a grounded enclosure. The accessible plug or isolation switch must remove heater power.

An AC SSR can fail short-circuit. Firmware switching the GPIO off cannot stop a welded or failed-short SSR.

## OTA

OTA can only be started explicitly from Settings while normal profile control is idle. During an OTA session:

- the heater output is forced off continuously;
- reflow and manual heating cannot run;
- Wi-Fi exists only for the temporary session;
- the access point uses a generated password;
- the upload form uses a generated per-session token;
- only an ESP32 application image is accepted;
- power must remain connected during flash writing and verification.

Do not perform OTA while the oven is hot or unattended. Update only firmware that you built or obtained from a trusted source.

## PID autotune

Autotune deliberately heats and cools the oven repeatedly. Run it only with:

- an empty oven;
- the normal sensor securely installed;
- the door closed in its normal operating position;
- the normal insulation and airflow arrangement;
- direct supervision throughout the process.

Autotune is bounded by a 70% high output, target limits, target-plus-25 C overshoot limit, global 285 C limit, sensor validity checks, phase timeout, and total timeout. These reduce risk but do not replace independent thermal protection.

Review the proposed PID gains before saving. Closely supervise the first profile run after changing gains and be ready to disconnect mains power.

## Initial testing

Keep mains and the heater disconnected while confirming display operation, buttons, MAX31865 readings, profile storage, themes, and the OTA web page. Test the SSR output initially with a low-voltage indicator load.
