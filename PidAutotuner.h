#pragma once

#include <Arduino.h>

#include "Config.h"
#include "HeaterController.h"
#include "Types.h"

class PidAutotuner {
 public:
  enum class State : uint8_t {
    IDLE,
    PREHEAT,
    COOLING,
    HEATING,
    COMPLETE,
    ABORTED,
    FAULT,
  };

  explicit PidAutotuner(HeaterController &heater) : heater_(heater) {}

  bool start(float targetC, const TemperatureReading &reading,
             uint32_t nowMs);
  void update(const TemperatureReading &reading, uint32_t nowMs);
  void abort(const char *reason = "Cancelled");
  void reset();

  bool active() const {
    return state_ == State::PREHEAT || state_ == State::COOLING ||
           state_ == State::HEATING;
  }
  bool complete() const { return state_ == State::COMPLETE; }
  bool failed() const { return state_ == State::FAULT; }
  State state() const { return state_; }
  const char *stateName() const;
  const char *detail() const { return detail_; }

  float targetC() const { return targetC_; }
  float demandPercent() const { return demandPercent_; }
  uint8_t completedCycles() const { return completedCycles_; }
  uint8_t requiredCycles() const { return PID_AUTOTUNE_CYCLES; }
  float latestPeakC() const { return latestPeakC_; }
  float latestTroughC() const { return latestTroughC_; }
  float kp() const { return resultKp_; }
  float ki() const { return resultKi_; }
  float kd() const { return resultKd_; }
  float ultimateGain() const { return ultimateGain_; }
  float ultimatePeriodS() const { return ultimatePeriodS_; }

 private:
  HeaterController &heater_;
  State state_ = State::IDLE;
  char detail_[48] = "Ready";

  float targetC_ = PID_AUTOTUNE_DEFAULT_TARGET_C;
  float demandPercent_ = 0.0f;
  float phaseExtremeC_ = NAN;
  float latestPeakC_ = NAN;
  float latestTroughC_ = NAN;
  float amplitudeSumC_ = 0.0f;
  float periodSumS_ = 0.0f;
  uint8_t completedCycles_ = 0;
  uint8_t usableCycles_ = 0;
  uint32_t startedMs_ = 0;
  uint32_t phaseStartedMs_ = 0;
  uint32_t previousHighCrossMs_ = 0;

  float resultKp_ = 0.0f;
  float resultKi_ = 0.0f;
  float resultKd_ = 0.0f;
  float ultimateGain_ = 0.0f;
  float ultimatePeriodS_ = 0.0f;

  void setFault(const char *reason);
  void enterCooling(float temperatureC, uint32_t nowMs,
                    bool firstCrossing);
  void enterHeating(float temperatureC, uint32_t nowMs);
  bool calculateResult();
};
