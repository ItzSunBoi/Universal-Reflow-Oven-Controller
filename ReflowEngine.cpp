#include "ReflowEngine.h"

#include <cmath>
#include <cstring>


ReflowEngine::ReflowEngine(HeaterController &heater) : heater_(heater) {}

bool ReflowEngine::startProfile(const ReflowProfile &profile,
                                float currentTemperatureC,
                                uint32_t nowMs) {
  if (state_ == RunState::RUNNING || state_ == RunState::PAUSED ||
      state_ == RunState::MANUAL) {
    return false;
  }
  if (profile.stageCount == 0 || profile.stageCount > MAX_PROFILE_STAGES) {
    triggerFault(FaultCode::INVALID_PROFILE, "No usable profile stages");
    return false;
  }

  activeProfile_ = profile;
  stageIndex_ = 0;
  resetRunMetrics(currentTemperatureC, nowMs);
  state_ = RunState::RUNNING;
  targetTemperatureC_ = currentTemperatureC;
  previousTargetC_ = currentTemperatureC;
  stageStartTargetC_ = currentTemperatureC;
  heater_.resetPid(currentTemperatureC, nowMs);
  return true;
}

bool ReflowEngine::startManual(float targetC, float currentTemperatureC,
                               uint32_t nowMs) {
  if (state_ == RunState::RUNNING || state_ == RunState::PAUSED) {
    return false;
  }
  manualTargetC_ = constrain(targetC, 40.0f, 250.0f);
  resetRunMetrics(currentTemperatureC, nowMs);
  state_ = RunState::MANUAL;
  targetTemperatureC_ = manualTargetC_;
  heater_.resetPid(currentTemperatureC, nowMs);
  return true;
}

void ReflowEngine::setManualTarget(float targetC) {
  manualTargetC_ = constrain(targetC, 40.0f, 250.0f);
}

void ReflowEngine::update(const TemperatureReading &reading,
                          uint32_t nowMs) {
  if (state_ == RunState::FAULT || state_ == RunState::IDLE ||
      state_ == RunState::COMPLETE) {
    heaterDemandPercent_ = 0.0f;
    return;
  }

  if (!reading.valid) {
    heaterDemandPercent_ = 0.0f;
    if (reading.timestampMs != 0) {
      triggerFault(FaultCode::SENSOR, "MAX31865 reading invalid");
    }
    return;
  }

  const float temperatureC = reading.filteredC;
  peakTemperatureC_ = max(peakTemperatureC_, reading.rawC);

  if (reading.rawC >= GLOBAL_MAX_TEMPERATURE_C) {
    triggerFault(FaultCode::OVERTEMPERATURE, "Global temperature limit");
    return;
  }

  const uint32_t dtMs = (lastUpdateMs_ == 0) ? 0 : (nowMs - lastUpdateMs_);
  lastUpdateMs_ = nowMs;

  if (state_ == RunState::PAUSED) {
    heaterDemandPercent_ = 0.0f;
    appendHistory(temperatureC, targetTemperatureC_, nowMs);
    return;
  }

  if (state_ == RunState::MANUAL) {
    targetTemperatureC_ = manualTargetC_;
    if (temperatureC > manualTargetC_ + 15.0f ||
        temperatureC > 260.0f) {
      triggerFault(FaultCode::OVERTEMPERATURE, "Manual mode limit");
      return;
    }
    heaterDemandPercent_ =
        heater_.computePid(targetTemperatureC_, temperatureC, nowMs);
    updateSafetyMonitors(temperatureC, nowMs);
    appendHistory(temperatureC, targetTemperatureC_, nowMs);
    return;
  }

  if (stageIndex_ >= activeProfile_.stageCount) {
    completeRun(nowMs);
    return;
  }

  const ReflowStage &stage = activeProfile_.stages[stageIndex_];
  const float rawTarget = calculateStageTarget(nowMs);

  // Enforce the profile's maximum upward target ramp even if a user creates
  // a stage that requests a faster jump.
  if (dtMs > 0 && rawTarget > previousTargetC_) {
    const float maxRise = activeProfile_.maxRampCPerSecond * dtMs / 1000.0f;
    targetTemperatureC_ = min(rawTarget, previousTargetC_ + maxRise);
  } else {
    targetTemperatureC_ = rawTarget;
  }
  previousTargetC_ = targetTemperatureC_;

  if (temperatureC > activeProfile_.maxTemperatureC + 8.0f) {
    triggerFault(FaultCode::OVERTEMPERATURE, "Profile temperature limit");
    return;
  }

  if (dtMs > 0 && temperatureC >= activeProfile_.liquidusC) {
    timeAboveLiquidusMs_ += dtMs;
  }

  if (stage.mode == StageMode::COOL) {
    heaterDemandPercent_ = 0.0f;
  } else {
    heaterDemandPercent_ =
        heater_.computePid(targetTemperatureC_, temperatureC, nowMs);
  }

  updateSafetyMonitors(temperatureC, nowMs);
  appendHistory(temperatureC, targetTemperatureC_, nowMs);

  const uint32_t elapsedMs = stageElapsedMs(nowMs);
  const uint32_t plannedMs = static_cast<uint32_t>(stage.durationS) * 1000UL;
  bool stageComplete = elapsedMs >= plannedMs;
  if (stage.mode == StageMode::COOL) {
    stageComplete =
        (elapsedMs >= plannedMs && temperatureC <= stage.targetC + 4.0f) ||
        elapsedMs >= plannedMs * 3UL;
  }

  if (stageComplete) {
    advanceStage(temperatureC, nowMs);
  }
}

void ReflowEngine::pauseOrResume(uint32_t nowMs) {
  if (state_ == RunState::RUNNING) {
    state_ = RunState::PAUSED;
    pauseStartMs_ = nowMs;
    heaterDemandPercent_ = 0.0f;
    heater_.forceOff();
  } else if (state_ == RunState::PAUSED) {
    const uint32_t pausedDuration = nowMs - pauseStartMs_;
    runStartMs_ += pausedDuration;
    stageStartMs_ += pausedDuration;
    lastUpdateMs_ = nowMs;
    state_ = RunState::RUNNING;
  }
}

void ReflowEngine::abortRun() {
  heaterDemandPercent_ = 0.0f;
  heater_.forceOff();
  state_ = RunState::IDLE;
  faultCode_ = FaultCode::NONE;
  faultDetail_[0] = '\0';
}

void ReflowEngine::triggerFault(FaultCode code, const char *detail) {
  if (state_ == RunState::FAULT && faultCode_ == code) {
    return;
  }
  faultCode_ = code;
  strlcpy(faultDetail_, detail ? detail : faultCodeName(code),
          sizeof(faultDetail_));
  heaterDemandPercent_ = 0.0f;
  heater_.forceOff();
  state_ = RunState::FAULT;
}

bool ReflowEngine::clearFault() {
  if (state_ != RunState::FAULT) {
    return false;
  }
  faultCode_ = FaultCode::NONE;
  faultDetail_[0] = '\0';
  state_ = RunState::IDLE;
  return true;
}

const char *ReflowEngine::stageName() const {
  if (state_ == RunState::MANUAL) return "Manual";
  if (stageIndex_ >= activeProfile_.stageCount) return "Complete";
  return activeProfile_.stages[stageIndex_].name;
}

uint32_t ReflowEngine::runElapsedMs(uint32_t nowMs) const {
  if (runStartMs_ == 0) return 0;
  if (state_ == RunState::PAUSED) return pauseStartMs_ - runStartMs_;
  return nowMs - runStartMs_;
}

uint32_t ReflowEngine::stageElapsedMs(uint32_t nowMs) const {
  if (stageStartMs_ == 0) return 0;
  if (state_ == RunState::PAUSED) return pauseStartMs_ - stageStartMs_;
  return nowMs - stageStartMs_;
}

uint32_t ReflowEngine::expectedDurationMs() const {
  uint32_t total = 0;
  for (uint8_t i = 0; i < activeProfile_.stageCount; ++i) {
    total += static_cast<uint32_t>(activeProfile_.stages[i].durationS) *
             1000UL;
  }
  return total;
}

float ReflowEngine::progress(uint32_t nowMs) const {
  const uint32_t expected = expectedDurationMs();
  if (expected == 0) return 0.0f;
  return constrain(static_cast<float>(runElapsedMs(nowMs)) / expected,
                   0.0f, 1.0f);
}

float ReflowEngine::historyTemperature(uint16_t index) const {
  if (index >= historyCount_) return NAN;
  return historyTemperature_[index];
}

float ReflowEngine::historyTarget(uint16_t index) const {
  if (index >= historyCount_) return NAN;
  return historyTarget_[index];
}

RunSummary ReflowEngine::consumePendingLog() {
  logPending_ = false;
  return pendingLog_;
}

void ReflowEngine::advanceStage(float currentTemperatureC, uint32_t nowMs) {
  stageStartTargetC_ = targetTemperatureC_;
  stageStartMs_ = nowMs;
  ++stageIndex_;
  heatingMonitorStartMs_ = 0;
  offRiseMonitorStartMs_ = 0;

  if (stageIndex_ >= activeProfile_.stageCount) {
    completeRun(nowMs);
  } else {
    heater_.resetPid(currentTemperatureC, nowMs);
  }
}

float ReflowEngine::calculateStageTarget(uint32_t nowMs) const {
  const ReflowStage &stage = activeProfile_.stages[stageIndex_];
  if (stage.mode == StageMode::HOLD || stage.durationS == 0) {
    return stage.targetC;
  }
  const float fraction = constrain(
      static_cast<float>(stageElapsedMs(nowMs)) /
          (static_cast<float>(stage.durationS) * 1000.0f),
      0.0f, 1.0f);
  return stageStartTargetC_ + (stage.targetC - stageStartTargetC_) * fraction;
}

void ReflowEngine::updateSafetyMonitors(float temperatureC,
                                        uint32_t nowMs) {
  const bool strongHeating = heaterDemandPercent_ >= 75.0f &&
                             targetTemperatureC_ - temperatureC >= 10.0f;
  if (strongHeating) {
    if (heatingMonitorStartMs_ == 0) {
      heatingMonitorStartMs_ = nowMs;
      heatingMonitorStartC_ = temperatureC;
    } else if ((nowMs - heatingMonitorStartMs_) >= 60000UL) {
      if ((temperatureC - heatingMonitorStartC_) < 2.0f) {
        triggerFault(FaultCode::HEATING_FAILURE,
                     "Less than 2C rise in 60 seconds");
        return;
      }
      heatingMonitorStartMs_ = nowMs;
      heatingMonitorStartC_ = temperatureC;
    }
  } else {
    heatingMonitorStartMs_ = 0;
  }

  // Conservative software-only SSR-stuck detector. Thermal inertia can cause
  // a short rise after switch-off, so it waits 90 seconds and requires +25 C.
  if (heaterDemandPercent_ <= 0.5f && temperatureC > 80.0f) {
    if (offRiseMonitorStartMs_ == 0) {
      offRiseMonitorStartMs_ = nowMs;
      offRiseMonitorStartC_ = temperatureC;
    } else if ((nowMs - offRiseMonitorStartMs_) >= 90000UL) {
      if ((temperatureC - offRiseMonitorStartC_) > 25.0f) {
        triggerFault(FaultCode::SSR_STUCK_SUSPECTED,
                     "Temperature rose with SSR commanded off");
        return;
      }
      offRiseMonitorStartMs_ = nowMs;
      offRiseMonitorStartC_ = temperatureC;
    }
  } else {
    offRiseMonitorStartMs_ = 0;
  }
}

void ReflowEngine::completeRun(uint32_t nowMs) {
  heaterDemandPercent_ = 0.0f;
  heater_.forceOff();
  state_ = RunState::COMPLETE;

  memset(&pendingLog_, 0, sizeof(pendingLog_));
  pendingLog_.sequence = ++runSequence_;
  pendingLog_.profileUid = activeProfile_.uid;
  strlcpy(pendingLog_.profileName, activeProfile_.name,
          sizeof(pendingLog_.profileName));
  pendingLog_.totalTimeS = static_cast<uint16_t>(
      min(runElapsedMs(nowMs) / 1000UL, 65535UL));
  pendingLog_.timeAboveLiquidusS = static_cast<uint16_t>(
      min(timeAboveLiquidusMs_ / 1000UL, 65535UL));
  pendingLog_.peakTemperatureC = peakTemperatureC_;
  pendingLog_.completed = true;
  logPending_ = true;
}

void ReflowEngine::appendHistory(float actualC, float targetC,
                                 uint32_t nowMs) {
  if (lastHistoryMs_ != 0 && (nowMs - lastHistoryMs_) < 2000UL) {
    return;
  }
  lastHistoryMs_ = nowMs;

  if (historyCount_ < HISTORY_CAPACITY) {
    historyTemperature_[historyCount_] = actualC;
    historyTarget_[historyCount_] = targetC;
    ++historyCount_;
    return;
  }

  memmove(historyTemperature_, historyTemperature_ + 1,
          sizeof(float) * (HISTORY_CAPACITY - 1U));
  memmove(historyTarget_, historyTarget_ + 1,
          sizeof(float) * (HISTORY_CAPACITY - 1U));
  historyTemperature_[HISTORY_CAPACITY - 1U] = actualC;
  historyTarget_[HISTORY_CAPACITY - 1U] = targetC;
}

void ReflowEngine::resetRunMetrics(float currentTemperatureC,
                                   uint32_t nowMs) {
  faultCode_ = FaultCode::NONE;
  faultDetail_[0] = '\0';
  runStartMs_ = nowMs;
  stageStartMs_ = nowMs;
  pauseStartMs_ = 0;
  lastUpdateMs_ = nowMs;
  stageStartTargetC_ = currentTemperatureC;
  targetTemperatureC_ = currentTemperatureC;
  previousTargetC_ = currentTemperatureC;
  heaterDemandPercent_ = 0.0f;
  peakTemperatureC_ = currentTemperatureC;
  timeAboveLiquidusMs_ = 0;
  heatingMonitorStartMs_ = 0;
  offRiseMonitorStartMs_ = 0;
  historyCount_ = 0;
  lastHistoryMs_ = 0;
  logPending_ = false;
}
