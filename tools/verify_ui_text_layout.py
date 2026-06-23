#!/usr/bin/env python3
"""Static regression checks for 240x240 UI text fit contracts."""

from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
CPP = (ROOT / "UiManager.cpp").read_text(encoding="utf-8")
HEADER = (ROOT / "UiManager.h").read_text(encoding="utf-8")
CONFIG = (ROOT / "Config.h").read_text(encoding="utf-8")

errors: list[str] = []

required_tokens = {
    "fitted text helper declaration": "void drawFittedText(",
    "wrapped text helper declaration": "void drawWrappedText(",
    "header fitting": "drawFittedText(title, 14, 7, titleWidth, 23, 2",
    "button fitting": "drawFittedText(labels[i], xs[i] + 4, BUTTON_Y, 64, 20, 1",
    "list primary fitting": "drawFittedText(primary, 24, y + 4, textWidth, 23, 2",
    "fault detail wrapping": "drawWrappedText(engine_.faultDetail(), 22, 89, 196, 2",
    "home profile fitting": "drawFittedText(profile.name, 24, 158, 192, 27, 2",
    "name editor fitting": "const uint8_t nameSize = fitText(buffer, fittedName",
    "OTA detail fitting": "drawFittedText(ota_.detail(), 20, 181, 200, 14, 1",
}
for description, token in required_tokens.items():
    if token not in CPP and token not in HEADER:
        errors.append(f"missing {description}: {token}")

# All fixed centered strings should fit the physical screen at their requested
# scale without relying on clipping. Dynamic strings are covered by the helper.
for match in re.finditer(r'drawCentered\("([^"]*)",\s*\d+,\s*(\d+),', CPP):
    text = match.group(1)
    size = int(match.group(2))
    width = len(text) * 6 * size
    if width > 228:
        errors.append(
            f'centered literal too wide ({width}px): "{text}" at size {size}'
        )

# Header contracts use worst-case known status widths. The helper is still a
# last line of defence, but ordinary built-in page titles should remain size 2.
header_contracts = {
    "REFLOW OVEN": 166,
    "EDIT PROFILE": 160,
    "RUNNING": 130,
    "RUN INFO": 130,
    "PID AUTOTUNE": 148,
    "TUNE DETAILS": 148,
    "TUNE INFO": 148,
    "OTA UPDATE": 154,
    "OTA HELP": 154,
    "FAULT INFO": 136,
    "DELETE": 154,
}
for title, available in header_contracts.items():
    width = len(title) * 12
    if width > available:
        errors.append(
            f'header title would shrink unexpectedly: "{title}" '
            f'needs {width}px, has {available}px'
        )

config_contracts = {
    "TFT init speed": "constexpr uint32_t TFT_INIT_SPI_HZ = 4000000UL;",
    "TFT runtime speed": "constexpr uint32_t TFT_SPI_HZ = 40000000UL;",
    "fan disabled": "constexpr int8_t PIN_COOLING_FAN = -1;",
    "buzzer disabled": "constexpr int8_t PIN_BUZZER = -1;",
    "RTD nominal": "constexpr float RTD_NOMINAL_OHMS = 100.0f;",
    "RTD reference": "constexpr float RTD_REFERENCE_OHMS = 4300.0f;",
    "RTD wire count": "constexpr uint8_t RTD_WIRE_COUNT = 2;",
}
for description, token in config_contracts.items():
    if token not in CONFIG:
        errors.append(f"missing {description}: {token}")

if errors:
    print("UI text layout audit FAILED")
    for error in errors:
        print(f" - {error}")
    sys.exit(1)

print("UI text layout audit PASSED")
print("Checked fitted/wrapped helpers, fixed centered strings, header widths, and requested config values.")
