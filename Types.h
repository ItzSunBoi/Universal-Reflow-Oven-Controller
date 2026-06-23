#pragma once

#include <Arduino.h>
#include "Config.h"

enum class StageMode : uint8_t {
  RAMP = 0,
  HOLD = 1,
  COOL = 2,
};

struct ReflowStage {
  char name[12];
  StageMode mode;
  float targetC;
  uint16_t durationS;
  uint8_t reserved = 0;
};

struct ReflowProfile {
  uint32_t uid;
  char name[20];
  bool factoryProfile;
  uint8_t stageCount;
  float liquidusC;
  float maxTemperatureC;
  float maxRampCPerSecond;
  uint16_t targetTimeAboveLiquidusS;
  ReflowStage stages[MAX_PROFILE_STAGES];
};

struct SystemSettings {
  float temperatureOffsetC;
  bool buzzerEnabled;
  bool fanDuringCool;
  uint8_t backlightPercent;
  uint8_t reserved[4];
};

struct RunSummary {
  uint32_t sequence;
  uint32_t profileUid;
  char profileName[20];
  uint16_t totalTimeS;
  uint16_t timeAboveLiquidusS;
  float peakTemperatureC;
  bool completed;
  uint8_t reserved[3];
};

enum class RunState : uint8_t {
  IDLE,
  RUNNING,
  PAUSED,
  MANUAL,
  COMPLETE,
  FAULT,
};

enum class FaultCode : uint8_t {
  NONE,
  ESTOP,
  SENSOR,
  OVERTEMPERATURE,
  HEATING_FAILURE,
  SSR_STUCK_SUSPECTED,
  INVALID_PROFILE,
};

struct TemperatureReading {
  bool valid;
  float rawC;
  float filteredC;
  uint16_t rawRtd;
  uint8_t max31865Fault;
  uint32_t timestampMs;
};

inline const char *stageModeName(StageMode mode) {
  switch (mode) {
    case StageMode::RAMP: return "Ramp";
    case StageMode::HOLD: return "Hold";
    case StageMode::COOL: return "Cool";
    default: return "?";
  }
}

inline const char *faultCodeName(FaultCode code) {
  switch (code) {
    case FaultCode::NONE: return "No fault";
    case FaultCode::ESTOP: return "Emergency stop";
    case FaultCode::SENSOR: return "Temperature sensor";
    case FaultCode::OVERTEMPERATURE: return "Overtemperature";
    case FaultCode::HEATING_FAILURE: return "Heating failure";
    case FaultCode::SSR_STUCK_SUSPECTED: return "SSR stuck suspected";
    case FaultCode::INVALID_PROFILE: return "Invalid profile";
    default: return "Unknown fault";
  }
}
