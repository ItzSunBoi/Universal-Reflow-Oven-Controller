#!/usr/bin/env python3
"""Static regression checks for the three-button UI contract."""

from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
CPP = (ROOT / "UiManager.cpp").read_text(encoding="utf-8")
HEADER = (ROOT / "UiManager.h").read_text(encoding="utf-8")

REQUIRED = {
    "PID live footer": 'drawButtons("STOP", "DETAIL", "INFO")',
    "PID detail action": "pidInfoView_ = PidInfoView::DIAGNOSTICS;",
    "PID info action": "pidInfoView_ = PidInfoView::HELP;",
    "PID info handler": "void UiManager::handlePidAutotuneInfo",
    "OTA waiting footer": 'drawButtons("STOP", "INFO", "STOP")',
    "OTA upload lock": 'drawButtons("LOCKED", "UPLOAD", "LOCKED")',
    "OTA info handler": "void UiManager::handleOtaInfo",
    "Fault footer": 'drawButtons("HOME", "DETAIL", "HOLD RST")',
    "Fault detail handler": "void UiManager::handleFaultDetail",
    "Home lock label": 'ready ? "START" : "LOCKED"',
    "Maximum profile guard": "profiles_.profileCount() < MAX_PROFILES",
    "Empty log guard": 'count > 1U ? "NEXT" : "NONE"',
    "Final name save label": 'atLastNamePosition ? "SAVE" : "NEXT"',
}

PAGES = (
    "PID_AUTOTUNE_INFO",
    "OTA_INFO",
    "FAULT_DETAIL",
)

errors: list[str] = []
for description, token in REQUIRED.items():
    if token not in CPP:
        errors.append(f"missing {description}: {token}")

for page in PAGES:
    if page not in HEADER:
        errors.append(f"page enum missing: {page}")
    if f"case Page::{page}:" not in CPP:
        errors.append(f"page switch case missing: {page}")

if errors:
    print("Button contract audit FAILED")
    for error in errors:
        print(f" - {error}")
    sys.exit(1)

print("Button contract audit PASSED")
print(f"Checked {len(REQUIRED)} behavior contracts and {len(PAGES)} new pages.")
