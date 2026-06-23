#include "HeaterController.h"

#include <cmath>

void HeaterController::begin() {
  pinMode(PIN_SSR, OUTPUT);
  writeOutput(false);
  windowStartMs_ = millis();
}

void HeaterController::setPidTunings(float kp, float ki, float kd) {
  kp_ = max(0.0f, kp);
  ki_ = max(0.0f, ki);
  kd_ = max(0.0f, kd);
}

void HeaterController::resetPid(float measurementC, uint32_t nowMs) {
  integral_ = 0.0f;
  lastMeasurement_ = measurementC;
  lastPidMs_ = nowMs;
  pidInitialized_ = true;
}

float HeaterController::computePid(float setpointC, float measurementC,
                                   uint32_t nowMs) {
  if (!pidInitialized_) {
    resetPid(measurementC, nowMs);
  }

  const uint32_t elapsedMs = nowMs - lastPidMs_;
  if (elapsedMs < 80) {
    return demandPercent_;
  }

  const float dt = elapsedMs / 1000.0f;
  const float error = setpointC - measurementC;
  const float dMeasurement = (measurementC - lastMeasurement_) / dt;

  const float candidateIntegral = integral_ + ki_ * error * dt;
  const float proportional = kp_ * error;
  const float derivative = -kd_ * dMeasurement;
  const float unclamped = proportional + candidateIntegral + derivative;
  const float output = constrain(unclamped, 0.0f, 100.0f);

  // Conditional integration limits wind-up at 0% and 100%.
  const bool notSaturated = unclamped == output;
  const bool drivesBackFromHigh = output >= 100.0f && error < 0.0f;
  const bool drivesBackFromLow = output <= 0.0f && error > 0.0f;
  if (notSaturated || drivesBackFromHigh || drivesBackFromLow) {
    integral_ = constrain(candidateIntegral, -50.0f, 100.0f);
  }

  lastMeasurement_ = measurementC;
  lastPidMs_ = nowMs;
  demandPercent_ = output;
  return output;
}

void HeaterController::setDemand(float percent) {
  demandPercent_ = constrain(percent, 0.0f, 100.0f);
}

void HeaterController::update(uint32_t nowMs, bool hardInhibit) {
  if (hardInhibit || demandPercent_ <= 0.0f) {
    writeOutput(false);
    return;
  }

  if ((nowMs - windowStartMs_) >= SSR_WINDOW_MS) {
    const uint32_t windowsPassed = (nowMs - windowStartMs_) / SSR_WINDOW_MS;
    windowStartMs_ += windowsPassed * SSR_WINDOW_MS;
  }

  uint32_t onTimeMs = static_cast<uint32_t>(SSR_WINDOW_MS *
                                             demandPercent_ / 100.0f);
  if (onTimeMs < SSR_MIN_PULSE_MS) {
    onTimeMs = 0;
  } else if ((SSR_WINDOW_MS - onTimeMs) < SSR_MIN_PULSE_MS) {
    onTimeMs = SSR_WINDOW_MS;
  }

  const bool shouldBeOn = (nowMs - windowStartMs_) < onTimeMs;
  writeOutput(shouldBeOn);
}

void HeaterController::forceOff() {
  demandPercent_ = 0.0f;
  writeOutput(false);
}

void HeaterController::writeOutput(bool on) {
  if (outputOn_ == on) {
    return;
  }
  outputOn_ = on;
  digitalWrite(PIN_SSR, on ? ssrOnLevel() : ssrOffLevel());
}
