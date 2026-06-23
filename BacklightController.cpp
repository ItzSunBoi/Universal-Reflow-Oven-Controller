#include "BacklightController.h"

#include "Config.h"

bool BacklightController::begin() {
  if (PIN_TFT_BL < 0) return false;

  // Define a safe inactive level before handing the pin to LEDC.
  pinMode(PIN_TFT_BL, OUTPUT);
  digitalWrite(PIN_TFT_BL, TFT_BACKLIGHT_ACTIVE_HIGH ? LOW : HIGH);

  attached_ = ledcAttach(PIN_TFT_BL, TFT_BACKLIGHT_PWM_HZ,
                         TFT_BACKLIGHT_PWM_BITS);
  if (attached_) writePercent(0);
  return attached_;
}

void BacklightController::setPercent(uint8_t percent) {
  if (percent > 100U) percent = 100U;
  writePercent(percent);
}

void BacklightController::off() {
  writePercent(0);
}

void BacklightController::writePercent(uint8_t percent) {
  percent_ = percent;
  if (!attached_) return;

  const uint32_t maxDuty = (1UL << TFT_BACKLIGHT_PWM_BITS) - 1UL;
  uint32_t duty = (maxDuty * static_cast<uint32_t>(percent) + 50UL) / 100UL;
  if (!TFT_BACKLIGHT_ACTIVE_HIGH) duty = maxDuty - duty;
  ledcWrite(PIN_TFT_BL, duty);
}
