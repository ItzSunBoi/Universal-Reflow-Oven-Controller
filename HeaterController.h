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
  float kp() const { return kp_; }
  float ki() const { return ki_; }
  float kd() const { return kd_; }

 private:
  float kp_ = PID_DEFAULT_KP;
  float ki_ = PID_DEFAULT_KI;
  float kd_ = PID_DEFAULT_KD;

  float integral_ = 0.0f;
  float lastMeasurement_ = 0.0f;
  uint32_t lastPidMs_ = 0;
  bool pidInitialized_ = false;

  float demandPercent_ = 0.0f;
  bool outputOn_ = false;
  uint32_t windowStartMs_ = 0;

  void writeOutput(bool on);
};
