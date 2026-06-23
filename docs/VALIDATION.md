# Validation notes for v1.9.2

## Button contract audit

The three-button footer and handler logic were reviewed page by page. The automated regression script reports:

```text
Button contract audit PASSED
Checked 13 behavior contracts and 3 new pages.
```

Run it with:

```bash
python tools/verify_button_contracts.py
```

The audit specifically checks the live PID autotune `DETAIL` and `INFO` actions, OTA session controls, fault detail controls, locked heating labels, profile-capacity guard, log navigation guard, and final-position name saving.

## Static C++ checks

The modified `UiManager.cpp` and main Arduino sketch were checked using C++17 syntax compilation with warnings promoted to errors against a local Arduino/Adafruit API-shape stub environment:

```text
-Wall -Wextra -Wpedantic -Werror
```

The page enum contains 24 pages, and every page has both a draw-switch case and a button-handler switch case.

## Hardware status

The changes have not yet been compiled in the user's exact Arduino-ESP32 3.3.2 environment or tested on the physical panel. The code changes are limited to UI navigation, labels, and information pages; heater, sensor, NVS, OTA partition, and PID control algorithms are unchanged.


## OTA v1.9.2 checks

- `OtaManager.cpp` passed a C++17 warning-as-error syntax check against an Arduino-ESP32 API-shape harness including WiFi, WebServer, Update, Preferences, reset-reason, and heap APIs.
- The OTA session marker is cleared on normal stop, AP-start failure, and successful image verification.
- The only deliberate `ESP.restart()` remains the post-verification restart path.
- UI backlight reduction now occurs before radio startup and is restored on failure or normal exit.
- Physical SoftAP startup, browser upload, brownout behavior, and Arduino-ESP32 3.3.2 compilation still require testing on the target controller.
