#pragma once

#include <Arduino.h>
#include "Config.h"

class HeaterController {
 public:
  void begin();

  void setPidTunings(float kp, float ki, float kd);
  void resetPid(float measurementC, uint32_t nowMs);
  float computePid(float setpointC, float measurementC, uint32_t nowMs);

  void setDemand(float percent);
  void update(uint32_t nowMs, bool hardInhibit);
  void forceOff();

  float demandPercent() const { return demandPercent_; }
  bool outputOn() const { return outputOn_; }

 private:
  float kp_ = 5.0f;
  float ki_ = 0.10f;
  float kd_ = 18.0f;

  float integral_ = 0.0f;
  float lastMeasurement_ = 0.0f;
  uint32_t lastPidMs_ = 0;
  bool pidInitialized_ = false;

  float demandPercent_ = 0.0f;
  bool outputOn_ = false;
  uint32_t windowStartMs_ = 0;

  void writeOutput(bool on);
};
