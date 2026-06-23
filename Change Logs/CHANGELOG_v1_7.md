# v1.7 changes

- Added a 240x240 RGB565 off-screen framebuffer.
- Removed routine physical-screen clearing from UI refreshes.
- Added 24x24 dirty-tile hashing and selective LCD transfer.
- Added complete-frame, no-clear page transitions.
- Added stride-aware RGB565 rectangle transfer to `CslessST7789`.
- Moved button scanning to a dedicated core-0 FreeRTOS task.
- Added a 32-event thread-safe button queue.
- Retained cooperative button scanning as a task-creation fallback.
- Preserved all UI colors, placement, page ordering, profiles, NVS data, SSR control, MAX31865 support, and backlight behavior.
