#pragma once

#include <Arduino.h>

#include "Config.h"
#include "HeaterController.h"
#include "Types.h"

class ReflowEngine {
 public:
  explicit ReflowEngine(HeaterController &heater);

  bool startProfile(const ReflowProfile &profile, float currentTemperatureC,
                    uint32_t nowMs);
  bool startManual(float targetC, float currentTemperatureC, uint32_t nowMs);
  void setManualTarget(float targetC);

  void update(const TemperatureReading &reading, uint32_t nowMs);
  void pauseOrResume(uint32_t nowMs);
  void abortRun();
  void triggerFault(FaultCode code, const char *detail = nullptr);
  bool clearFault();

  RunState state() const { return state_; }
  FaultCode faultCode() const { return faultCode_; }
  const char *faultDetail() const { return faultDetail_; }
  const ReflowProfile &activeProfile() const { return activeProfile_; }

  uint8_t stageIndex() const { return stageIndex_; }
  const char *stageName() const;
  float targetTemperatureC() const { return targetTemperatureC_; }
  float heaterDemandPercent() const { return heaterDemandPercent_; }
  float peakTemperatureC() const { return peakTemperatureC_; }
  uint32_t timeAboveLiquidusMs() const { return timeAboveLiquidusMs_; }
  uint32_t runElapsedMs(uint32_t nowMs) const;
  uint32_t stageElapsedMs(uint32_t nowMs) const;
  uint32_t expectedDurationMs() const;
  float progress(uint32_t nowMs) const;
  float manualTargetC() const { return manualTargetC_; }

  static constexpr uint16_t HISTORY_CAPACITY = 180;
  uint16_t historyCount() const { return historyCount_; }
  float historyTemperature(uint16_t index) const;
  float historyTarget(uint16_t index) const;

  bool hasPendingLog() const { return logPending_; }
  RunSummary consumePendingLog();

 private:
  HeaterController &heater_;
  RunState state_ = RunState::IDLE;
  FaultCode faultCode_ = FaultCode::NONE;
  char faultDetail_[48] = "";

  ReflowProfile activeProfile_{};
  uint8_t stageIndex_ = 0;
  uint32_t runStartMs_ = 0;
  uint32_t stageStartMs_ = 0;
  uint32_t pauseStartMs_ = 0;
  uint32_t lastUpdateMs_ = 0;
  float stageStartTargetC_ = 25.0f;
  float targetTemperatureC_ = 25.0f;
  float previousTargetC_ = 25.0f;
  float heaterDemandPercent_ = 0.0f;
  float peakTemperatureC_ = -1000.0f;
  uint32_t timeAboveLiquidusMs_ = 0;
  float manualTargetC_ = 120.0f;

  uint32_t heatingMonitorStartMs_ = 0;
  float heatingMonitorStartC_ = 0.0f;
  uint32_t offRiseMonitorStartMs_ = 0;
  float offRiseMonitorStartC_ = 0.0f;

  float historyTemperature_[HISTORY_CAPACITY]{};
  float historyTarget_[HISTORY_CAPACITY]{};
  uint16_t historyCount_ = 0;
  uint32_t lastHistoryMs_ = 0;

  bool logPending_ = false;
  RunSummary pendingLog_{};
  uint32_t runSequence_ = 0;

  void advanceStage(float currentTemperatureC, uint32_t nowMs);
  float calculateStageTarget(uint32_t nowMs) const;
  void updateSafetyMonitors(float temperatureC, uint32_t nowMs);
  void completeRun(uint32_t nowMs);
  void appendHistory(float actualC, float targetC, uint32_t nowMs);
  void resetRunMetrics(float currentTemperatureC, uint32_t nowMs);
};
