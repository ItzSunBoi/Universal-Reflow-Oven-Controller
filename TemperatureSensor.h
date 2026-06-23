#pragma once

#include <Adafruit_MAX31865.h>
#include <SPI.h>

#include "Config.h"
#include "Types.h"

class TemperatureSensor {
 public:
  explicit TemperatureSensor(SPIClass &spi);

  // wireMode is used only when USE_NTC_100K_SENSOR is 0.
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
  const char *backendName() const;
  bool usingNtc() const { return USE_NTC_100K_SENSOR != 0; }

 private:
  enum NtcFault : uint8_t {
    NTC_FAULT_NONE = 0,
    NTC_FAULT_OPEN = 1U << 0,
    NTC_FAULT_SHORT = 1U << 1,
    NTC_FAULT_RESISTANCE_RANGE = 1U << 2,
    NTC_FAULT_TEMPERATURE_RANGE = 1U << 3,
    NTC_FAULT_ADC = 1U << 4,
  };

  bool updateMax31865(uint32_t nowMs);
  bool updateNtc(uint32_t nowMs);
  void acceptSample(float rawC, uint32_t nowMs, float filterAlpha);
  void rejectSample(float fallbackRawC, uint32_t nowMs);

  Adafruit_MAX31865 max_;
  TemperatureReading reading_{};
  uint32_t lastSampleMs_ = 0;
  uint8_t consecutiveFailures_ = 0;
  float calibrationOffsetC_ = 0.0f;
  bool initialized_ = false;
};
