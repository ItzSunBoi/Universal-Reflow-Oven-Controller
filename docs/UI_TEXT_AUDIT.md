# 240x240 UI text audit

## Scope

Every page title, status badge, footer label, list row, fixed explanatory line,
and dynamic profile/stage/fault/OTA string in `UiManager.cpp` was reviewed
against its actual 240x240 drawing region.

## Problems corrected

1. Header titles could overlap a status badge because title and badge widths
   were calculated independently.
2. Long custom profile names could leave their list row, home card, log card,
   header, name editor, or delete-confirmation panel.
3. Long fault details could run into later diagnostic rows.
4. The OTA progress percentage and heap line overlapped by two pixels during
   an upload.
5. Several size-2 headings were wider than the content panel, including the
   browser-update, firmware-upload, relay-tuning, and delete prompts.
6. A long dynamic OTA detail or autotune detail could cross the panel edge.

## Rendering safeguards

- `fitText()` selects the requested font size when possible, falls back to a
  smaller size, and finally adds an ellipsis when necessary.
- `drawFittedText()` constrains left, centered, or right-aligned text to an
  explicit rectangle.
- `drawWrappedText()` word-wraps detail messages and ellipsizes the final line.
- Header titles now reserve space for the measured status badge before drawing.
- Footer labels are constrained to the 64-pixel interior of each button.
- List rows reserve space for their colored status dot.

## Fixed headings retained at size 2

The following title/status combinations were checked at their worst built-in
status width and remain at text size 2:

- REFLOW OVEN
- EDIT PROFILE / CUSTOM
- RUNNING / longest stage name
- RUN INFO / longest stage name
- PID AUTOTUNE / COMPLETE
- TUNE DETAILS / COMPLETE
- TUNE INFO / COMPLETE
- OTA UPDATE / SUCCESS
- OTA HELP / SUCCESS
- FAULT INFO / HEATER OFF
- DELETE / CONFIRM

## Automated checks

Run:

```bash
python tools/verify_ui_text_layout.py
python tools/verify_button_contracts.py
```

The layout audit checks helper usage, all fixed centered-string widths, header
width contracts, and the requested `Config.h` values. `UiManager.cpp` also
passes a C++17 warning-as-error syntax check against an Arduino/Adafruit API
shape harness.
