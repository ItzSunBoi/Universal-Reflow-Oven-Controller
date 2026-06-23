# v1.6 changes

- Added a three-state inactivity backlight controller: active, dimmed, and off.
- Added persistent Settings entries for idle-dim delay, screen-off delay, and dim brightness.
- Default policy: dim to 20% after 60 seconds and turn the backlight off after 10 minutes.
- Added instant wake on any button event.
- A button event that wakes a fully off display is consumed to prevent unseen actions.
- Active reflow, paused, manual heating, completion, and fault states remain fully illuminated.
- Suspended idle-page redraws while the backlight is fully off.
- Reused previously reserved SystemSettings bytes, preserving the v1.5 NVS database size and existing profiles/logs.
- Retained the proven CS-less ST7789 SPI mode 2 transport and all existing UI placement and colors.
