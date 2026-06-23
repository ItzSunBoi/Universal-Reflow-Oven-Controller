#pragma once

#include <Adafruit_MAX31865.h>
#include <SPI.h>

#include "Config.h"
#include "Types.h"

class TemperatureSensor {
 public:
  explicit TemperatureSensor(SPIClass &spi);

  bool begin(max31865_numwires_t wireMode = MAX31865_3WIRE);
  bool update(uint32_t nowMs);
  const TemperatureReading &reading() const { return reading_; }
  bool valid() const { return reading_.valid; }
  float temperatureC() const { return reading_.filteredC; }
  float rawTemperatureC() const { return reading_.rawC; }
  uint8_t consecutiveFailures() const { return consecutiveFailures_; }

  void setCalibrationOffset(float offsetC) { calibrationOffsetC_ = offsetC; }
  float calibrationOffset() const { return calibrationOffsetC_; }
  const char *faultDescription() const;

 private:
  Adafruit_MAX31865 max_;
  TemperatureReading reading_{};
  uint32_t lastSampleMs_ = 0;
  uint8_t consecutiveFailures_ = 0;
  float calibrationOffsetC_ = 0.0f;
  bool initialized_ = false;
};
