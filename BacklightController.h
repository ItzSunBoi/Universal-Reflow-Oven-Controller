#pragma once

#include <Arduino.h>

class BacklightController {
 public:
  bool begin();
  void setPercent(uint8_t percent);
  void off();
  uint8_t percent() const { return percent_; }
  bool attached() const { return attached_; }

 private:
  void writePercent(uint8_t percent);

  bool attached_ = false;
  uint8_t percent_ = 0;
};
