#include "PidAutotuner.h"

#include <cmath>
#include <cstring>

namespace {
constexpr float PI_F = 3.14159265358979323846f;
}

bool PidAutotuner::start(float targetC, const TemperatureReading &reading,
                         uint32_t nowMs) {
  if (active() || !reading.valid) return false;

  reset();
  targetC_ = constrain(targetC, PID_AUTOTUNE_MIN_TARGET_C,
                       PID_AUTOTUNE_MAX_TARGET_C);
  startedMs_ = nowMs;
  phaseStartedMs_ = nowMs;
  phaseExtremeC_ = reading.filteredC;

  if (reading.filteredC > targetC_ + PID_AUTOTUNE_HYSTERESIS_C) {
    state_ = State::COOLING;
    demandPercent_ = 0.0f;
    strlcpy(detail_, "Cooling to target band", sizeof(detail_));
  } else {
    state_ = State::PREHEAT;
    demandPercent_ = PID_AUTOTUNE_RELAY_HIGH_PERCENT;
    strlcpy(detail_, "Preheating", sizeof(detail_));
  }
  heater_.forceOff();
  return true;
}

void PidAutotuner::update(const TemperatureReading &reading,
                          uint32_t nowMs) {
  if (!active()) return;

  if (!reading.valid) {
    setFault("Temperature sensor invalid");
    return;
  }

  const float temperatureC = reading.filteredC;
  const float rawC = reading.rawC;
  if (rawC >= GLOBAL_MAX_TEMPERATURE_C ||
      rawC >= targetC_ + PID_AUTOTUNE_MAX_OVERSHOOT_C) {
    setFault("Autotune overtemperature");
    return;
  }
  if ((nowMs - startedMs_) > PID_AUTOTUNE_TOTAL_TIMEOUT_MS) {
    setFault("Autotune timed out");
    return;
  }
  if ((nowMs - phaseStartedMs_) > PID_AUTOTUNE_PHASE_TIMEOUT_MS) {
    setFault("Thermal cycle stalled");
    return;
  }

  switch (state_) {
    case State::PREHEAT:
      demandPercent_ = PID_AUTOTUNE_RELAY_HIGH_PERCENT;
      phaseExtremeC_ = max(phaseExtremeC_, temperatureC);
      if (temperatureC >= targetC_ + PID_AUTOTUNE_HYSTERESIS_C) {
        enterCooling(temperatureC, nowMs, true);
      }
      break;

    case State::COOLING:
      demandPercent_ = 0.0f;
      phaseExtremeC_ = max(phaseExtremeC_, temperatureC);
      if (temperatureC <= targetC_ - PID_AUTOTUNE_HYSTERESIS_C) {
        latestPeakC_ = phaseExtremeC_;
        enterHeating(temperatureC, nowMs);
      }
      break;

    case State::HEATING:
      demandPercent_ = PID_AUTOTUNE_RELAY_HIGH_PERCENT;
      phaseExtremeC_ = min(phaseExtremeC_, temperatureC);
      if (temperatureC >= targetC_ + PID_AUTOTUNE_HYSTERESIS_C) {
        latestTroughC_ = phaseExtremeC_;

        if (previousHighCrossMs_ != 0 && std::isfinite(latestPeakC_) &&
            std::isfinite(latestTroughC_)) {
          const float amplitudeC = (latestPeakC_ - latestTroughC_) * 0.5f;
          const float periodS = (nowMs - previousHighCrossMs_) / 1000.0f;
          ++completedCycles_;

          // Discard the first cycle from the average because the oven has not
          // yet reached a repeatable limit cycle.
          if (completedCycles_ > 1U && amplitudeC >= 0.25f &&
              periodS >= 2.0f) {
            amplitudeSumC_ += amplitudeC;
            periodSumS_ += periodS;
            ++usableCycles_;
          }
        }

        if (completedCycles_ >= PID_AUTOTUNE_CYCLES) {
          demandPercent_ = 0.0f;
          heater_.forceOff();
          if (calculateResult()) {
            state_ = State::COMPLETE;
            strlcpy(detail_, "Tune complete; review values", sizeof(detail_));
          } else {
            setFault("Oscillation too small or irregular");
          }
          return;
        }
        enterCooling(temperatureC, nowMs, false);
      }
      break;

    default:
      demandPercent_ = 0.0f;
      break;
  }
}

void PidAutotuner::abort(const char *reason) {
  if (!active()) return;
  demandPercent_ = 0.0f;
  heater_.forceOff();
  state_ = State::ABORTED;
  strlcpy(detail_, reason ? reason : "Cancelled", sizeof(detail_));
}

void PidAutotuner::reset() {
  heater_.forceOff();
  state_ = State::IDLE;
  strlcpy(detail_, "Ready", sizeof(detail_));
  demandPercent_ = 0.0f;
  phaseExtremeC_ = NAN;
  latestPeakC_ = NAN;
  latestTroughC_ = NAN;
  amplitudeSumC_ = 0.0f;
  periodSumS_ = 0.0f;
  completedCycles_ = 0;
  usableCycles_ = 0;
  startedMs_ = 0;
  phaseStartedMs_ = 0;
  previousHighCrossMs_ = 0;
  resultKp_ = 0.0f;
  resultKi_ = 0.0f;
  resultKd_ = 0.0f;
  ultimateGain_ = 0.0f;
  ultimatePeriodS_ = 0.0f;
}

const char *PidAutotuner::stateName() const {
  switch (state_) {
    case State::IDLE: return "READY";
    case State::PREHEAT: return "PREHEAT";
    case State::COOLING: return "COOLING";
    case State::HEATING: return "HEATING";
    case State::COMPLETE: return "COMPLETE";
    case State::ABORTED: return "STOPPED";
    case State::FAULT: return "FAULT";
    default: return "?";
  }
}

void PidAutotuner::setFault(const char *reason) {
  demandPercent_ = 0.0f;
  heater_.forceOff();
  state_ = State::FAULT;
  strlcpy(detail_, reason ? reason : "Autotune failed", sizeof(detail_));
}

void PidAutotuner::enterCooling(float temperatureC, uint32_t nowMs,
                                bool firstCrossing) {
  state_ = State::COOLING;
  demandPercent_ = 0.0f;
  phaseExtremeC_ = temperatureC;
  phaseStartedMs_ = nowMs;
  if (firstCrossing || previousHighCrossMs_ == 0) {
    previousHighCrossMs_ = nowMs;
  } else {
    previousHighCrossMs_ = nowMs;
  }
  strlcpy(detail_, "Heater off; measuring peak", sizeof(detail_));
}

void PidAutotuner::enterHeating(float temperatureC, uint32_t nowMs) {
  state_ = State::HEATING;
  demandPercent_ = PID_AUTOTUNE_RELAY_HIGH_PERCENT;
  phaseExtremeC_ = temperatureC;
  phaseStartedMs_ = nowMs;
  strlcpy(detail_, "Heater on; measuring trough", sizeof(detail_));
}

bool PidAutotuner::calculateResult() {
  if (usableCycles_ == 0U) return false;

  const float amplitudeC = amplitudeSumC_ / usableCycles_;
  ultimatePeriodS_ = periodSumS_ / usableCycles_;
  if (!std::isfinite(amplitudeC) || !std::isfinite(ultimatePeriodS_) ||
      amplitudeC < 0.25f || ultimatePeriodS_ < 2.0f) {
    return false;
  }

  const float relayAmplitudePercent =
      PID_AUTOTUNE_RELAY_HIGH_PERCENT * 0.5f;
  ultimateGain_ = 4.0f * relayAmplitudePercent / (PI_F * amplitudeC);

  // Classic Ziegler-Nichols PID form, matching this firmware's continuous
  // Ki*error*seconds and Kd*dMeasurement/seconds implementation.
  resultKp_ = constrain(0.60f * ultimateGain_, 0.05f, 100.0f);
  resultKi_ = constrain(1.20f * ultimateGain_ / ultimatePeriodS_,
                        0.0001f, 10.0f);
  resultKd_ = constrain(0.075f * ultimateGain_ * ultimatePeriodS_,
                        0.0f, 500.0f);
  return std::isfinite(resultKp_) && std::isfinite(resultKi_) &&
         std::isfinite(resultKd_);
}
