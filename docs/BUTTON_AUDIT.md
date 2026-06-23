# Three-button UI audit

This table reflects the implemented v1.9.1 behavior. `Adjust` means short press, long press, and repeat are accepted. A fully dark display consumes the first press only to wake the backlight.

| Page/state | Left button | Middle button | Right button |
|---|---|---|---|
| Home | Profiles | Start selected profile, or `LOCKED` | Main menu |
| Profile list | Back | Select profile/add duplicate | Next row |
| Profile detail | Back | Edit | Start, or `LOCKED` |
| Profile editor | Back without saving | Open selected item | Next row |
| Stage list | Back | Edit/add stage | Next row |
| Stage editor | Back | Open/change selected item | Next row |
| Numeric editor | Decrease | Save and return | Increase |
| Name editor | Previous character | Next character; `SAVE` at final position; hold always saves | Next character |
| Reflow running | Stop | Pause/resume | Run details |
| Run details | Return to graph | Pause/resume | Return to graph |
| Complete | Home | Logs | Repeat, or `LOCKED` |
| Main menu | Back | Open | Next row |
| Manual heat | Decrease setpoint | On/off; hold exits | Increase setpoint |
| Calibration | Decrease offset | Save | Increase offset |
| Logs | Back | Home | Next log, or `NONE` |
| Settings | Back | Change/open selected item | Next row |
| PID autotune ready | Decrease target | Start, or `LOCKED` | Increase target |
| PID autotune running | Stop | Live diagnostics | Autotune help |
| PID autotune info | Back to live view | Stop and return | Toggle help/diagnostics |
| PID autotune complete | Back | Save gains | Reset for another tune |
| PID autotune failed | Back | Reset | Reset for another tune |
| OTA ready | Back | Start, or `LOCKED` | Back |
| OTA session waiting | Stop session | OTA help | Stop session |
| OTA uploading | `LOCKED` | Upload status | `LOCKED` |
| OTA help | Back | Stop session when allowed | Back |
| About | Back | Home | Back |
| Fault | Home while fault remains latched | Fault details | Hold to reset fault |
| Fault details | Back | Home while fault remains latched | Hold to reset fault |
| Delete confirmation | Cancel | Delete | Cancel |

## Safety-related intentional locks

- Heating controls show `LOCKED` when the temperature sensor is invalid, the controller is not idle, PID autotune is active, or OTA is active.
- OTA controls are locked during an actual flash write. Interrupting a write from the panel is intentionally prohibited.
- A short press on `HOLD RST` does nothing. The right button must be held long enough to generate a long-press event, and the fault only clears if the engine accepts the reset.
