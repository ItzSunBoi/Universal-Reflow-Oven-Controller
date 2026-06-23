#include "TemperatureSensor.h"

#include <cmath>

TemperatureSensor::TemperatureSensor(SPIClass &spi)
    : max_(PIN_MAX31865_CS, &spi) {}

bool TemperatureSensor::begin(max31865_numwires_t wireMode) {
  initialized_ = max_.begin(wireMode);
  if (!initialized_) {
    return false;
  }
  max_.enable50Hz(RTD_USE_50HZ_FILTER);
  max_.clearFault();
  reading_.valid = false;
  return true;
}

bool TemperatureSensor::update(uint32_t nowMs) {
  if (!initialized_ || (nowMs - lastSampleMs_) < SENSOR_SAMPLE_INTERVAL_MS) {
    return false;
  }
  lastSampleMs_ = nowMs;

  const uint16_t rawRtd = max_.readRTD();
  const uint8_t fault = max_.readFault();
  float rawC = max_.calculateTemperature(rawRtd, RTD_NOMINAL_OHMS,
                                         RTD_REFERENCE_OHMS);
  rawC += calibrationOffsetC_;

  const bool inRange = std::isfinite(rawC) &&
                       rawC >= GLOBAL_MIN_VALID_TEMPERATURE_C &&
                       rawC <= GLOBAL_MAX_VALID_TEMPERATURE_C;
  const bool good = fault == 0 && inRange;

  reading_.rawRtd = rawRtd;
  reading_.max31865Fault = fault;
  reading_.timestampMs = nowMs;

  if (good) {
    reading_.rawC = rawC;
    if (!reading_.valid || !std::isfinite(reading_.filteredC)) {
      reading_.filteredC = rawC;
    } else {
      // Mild low-pass filtering. Safety checks still have access to rawC.
      constexpr float alpha = 0.22f;
      reading_.filteredC += alpha * (rawC - reading_.filteredC);
    }
    reading_.valid = true;
    consecutiveFailures_ = 0;
  } else {
    if (consecutiveFailures_ < 255) {
      ++consecutiveFailures_;
    }
    // Tolerate isolated SPI/noise errors without feeding the bad sample into
    // control. Three consecutive failures invalidate the sensor and trip the
    // running controller.
    if (consecutiveFailures_ >= MAX_CONSECUTIVE_SENSOR_FAILURES) {
      reading_.valid = false;
      reading_.rawC = rawC;
    } else if (reading_.valid) {
      reading_.rawC = reading_.filteredC;
    }
    max_.clearFault();
  }

  return true;
}

const char *TemperatureSensor::faultDescription() const {
  const uint8_t fault = reading_.max31865Fault;
  if (fault & MAX31865_FAULT_HIGHTHRESH) return "RTD above threshold";
  if (fault & MAX31865_FAULT_LOWTHRESH) return "RTD below threshold";
  if (fault & MAX31865_FAULT_REFINLOW) return "REFIN- above 0.85 bias";
  if (fault & MAX31865_FAULT_REFINHIGH) return "REFIN open";
  if (fault & MAX31865_FAULT_RTDINLOW) return "RTDIN- below 0.85 bias";
  if (fault & MAX31865_FAULT_OVUV) return "Over/undervoltage";
  if (!reading_.valid) return "Invalid temperature";
  return "No sensor fault";
}
