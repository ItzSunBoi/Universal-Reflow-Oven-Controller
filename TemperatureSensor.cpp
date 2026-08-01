#include "TemperatureSensor.h"

#include <cmath>

TemperatureSensor::TemperatureSensor(SPIClass &spi)
    : max31865_(PIN_MAX31865_CS, &spi),
      max31855_(PIN_MAX31855_CS, &spi) {}

bool TemperatureSensor::begin(max31865_numwires_t wireMode) {
  reading_ = {};
  reading_.valid = false;
  reading_.coldJunctionC = NAN;
  consecutiveFailures_ = 0;

#if TEMP_SENSOR_BACKEND == TEMP_SENSOR_BACKEND_NTC_100K
  (void)wireMode;
  pinMode(PIN_NTC_ADC, INPUT);
  analogReadResolution(NTC_ADC_RESOLUTION_BITS);
  // Set both the global and per-pin attenuation. This keeps calibrated
  // analogReadMilliVolts() consistent across Arduino-ESP32 3.x revisions.
  analogSetAttenuation(ADC_11db);
  analogSetPinAttenuation(PIN_NTC_ADC, ADC_11db);

  // Prime the ADC after attenuation/channel setup. The first conversion after
  // switching channels can be less representative on SAR ADCs.
  (void)analogRead(PIN_NTC_ADC);
  (void)analogReadMilliVolts(PIN_NTC_ADC);
  initialized_ = true;
#elif TEMP_SENSOR_BACKEND == TEMP_SENSOR_BACKEND_MAX31855
  (void)wireMode;
  initialized_ = max31855_.begin();
  if (initialized_) {
    max31855_.setFaultChecks(MAX31855_FAULT_ALL);
  }
#else
  initialized_ = max31865_.begin(wireMode);
  if (initialized_) {
    max31865_.enable50Hz(RTD_USE_50HZ_FILTER);
    max31865_.clearFault();
  }
#endif

  return initialized_;
}

bool TemperatureSensor::update(uint32_t nowMs) {
  if (!initialized_ || (nowMs - lastSampleMs_) < SENSOR_SAMPLE_INTERVAL_MS) {
    return false;
  }
  lastSampleMs_ = nowMs;

#if TEMP_SENSOR_BACKEND == TEMP_SENSOR_BACKEND_NTC_100K
  return updateNtc(nowMs);
#elif TEMP_SENSOR_BACKEND == TEMP_SENSOR_BACKEND_MAX31855
  return updateMax31855(nowMs);
#else
  return updateMax31865(nowMs);
#endif
}

void TemperatureSensor::clearBackendDiagnostics() {
  reading_.rawRtd = 0;
  reading_.rawAdc = 0;
  reading_.adcMilliVolts = 0;
  reading_.sensorResistanceOhms = NAN;
  reading_.max31865Fault = 0;
  reading_.max31855Fault = 0;
  reading_.ntcFault = NTC_FAULT_NONE;
  reading_.coldJunctionC = NAN;
}

bool TemperatureSensor::updateMax31865(uint32_t nowMs) {
  const uint16_t rawRtd = max31865_.readRTD();
  const uint8_t fault = max31865_.readFault();
  float rawC = max31865_.calculateTemperature(rawRtd, RTD_NOMINAL_OHMS,
                                               RTD_REFERENCE_OHMS);
  rawC += calibrationOffsetC_;

  clearBackendDiagnostics();
  reading_.rawRtd = rawRtd;
  reading_.sensorResistanceOhms =
      (static_cast<float>(rawRtd) / 32768.0f) * RTD_REFERENCE_OHMS;
  reading_.max31865Fault = fault;

  const bool inRange = std::isfinite(rawC) &&
                       rawC >= GLOBAL_MIN_VALID_TEMPERATURE_C &&
                       rawC <= GLOBAL_MAX_VALID_TEMPERATURE_C;

  if (fault == 0 && inRange) {
    acceptSample(rawC, nowMs, 0.22f);
  } else {
    rejectSample(rawC, nowMs);
    max31865_.clearFault();
  }

  return true;
}

bool TemperatureSensor::updateMax31855(uint32_t nowMs) {
  clearBackendDiagnostics();

  const double thermocoupleC = max31855_.readCelsius();
  const uint8_t fault =
      std::isfinite(thermocoupleC) ? 0U : max31855_.readError();
  const double coldJunctionC = max31855_.readInternal();

  reading_.max31855Fault = fault;
  reading_.coldJunctionC = static_cast<float>(coldJunctionC);

  float rawC = static_cast<float>(thermocoupleC);
  rawC += calibrationOffsetC_;

  const bool coldJunctionValid =
      std::isfinite(coldJunctionC) &&
      coldJunctionC >= MAX31855_MIN_COLD_JUNCTION_C &&
      coldJunctionC <= MAX31855_MAX_COLD_JUNCTION_C;
  const bool temperatureValid =
      std::isfinite(rawC) && rawC >= GLOBAL_MIN_VALID_TEMPERATURE_C &&
      rawC <= GLOBAL_MAX_VALID_TEMPERATURE_C;

  if (fault == 0 && coldJunctionValid && temperatureValid) {
    acceptSample(rawC, nowMs, MAX31855_FILTER_ALPHA);
  } else {
    rejectSample(rawC, nowMs);
  }

  return true;
}

bool TemperatureSensor::updateNtc(uint32_t nowMs) {
  uint32_t rawSum = 0;
  uint32_t milliVoltSum = 0;

  for (uint8_t i = 0; i < NTC_ADC_SAMPLE_COUNT; ++i) {
    rawSum += analogRead(PIN_NTC_ADC);
    milliVoltSum += analogReadMilliVolts(PIN_NTC_ADC);
  }

  const uint16_t rawAdc = static_cast<uint16_t>(
      rawSum / static_cast<uint32_t>(NTC_ADC_SAMPLE_COUNT));
  const uint16_t adcMilliVolts = static_cast<uint16_t>(
      milliVoltSum / static_cast<uint32_t>(NTC_ADC_SAMPLE_COUNT));

  clearBackendDiagnostics();
  reading_.rawAdc = rawAdc;
  reading_.adcMilliVolts = adcMilliVolts;

  if (adcMilliVolts <= NTC_ADC_MIN_VALID_MV) {
    reading_.ntcFault = NTC_IS_HIGH_SIDE ? NTC_FAULT_OPEN : NTC_FAULT_SHORT;
    rejectSample(NAN, nowMs);
    return true;
  }

  if (adcMilliVolts >= NTC_ADC_MAX_VALID_MV ||
      adcMilliVolts >= NTC_DIVIDER_SUPPLY_MV) {
    reading_.ntcFault = NTC_IS_HIGH_SIDE ? NTC_FAULT_SHORT : NTC_FAULT_OPEN;
    rejectSample(NAN, nowMs);
    return true;
  }

  const float voltage = static_cast<float>(adcMilliVolts);
  const float supply = static_cast<float>(NTC_DIVIDER_SUPPLY_MV);
  float resistance = NAN;

  if (NTC_IS_HIGH_SIDE) {
    // 3.3 V -> NTC -> ADC -> fixed resistor -> GND
    resistance = NTC_FIXED_RESISTOR_OHMS * (supply - voltage) / voltage;
  } else {
    // 3.3 V -> fixed resistor -> ADC -> NTC -> GND
    resistance = NTC_FIXED_RESISTOR_OHMS * voltage / (supply - voltage);
  }

  reading_.sensorResistanceOhms = resistance;

  if (!std::isfinite(resistance) ||
      resistance < NTC_MIN_VALID_RESISTANCE_OHMS ||
      resistance > NTC_MAX_VALID_RESISTANCE_OHMS) {
    reading_.ntcFault = NTC_FAULT_RESISTANCE_RANGE;
    rejectSample(NAN, nowMs);
    return true;
  }

  constexpr float kelvinAtZeroC = 273.15f;
  const float nominalKelvin = NTC_NOMINAL_TEMPERATURE_C + kelvinAtZeroC;
  const float inverseKelvin =
      (1.0f / nominalKelvin) +
      (std::log(resistance / NTC_NOMINAL_OHMS) / NTC_BETA_COEFFICIENT_K);

  float rawC = (1.0f / inverseKelvin) - kelvinAtZeroC;
  rawC += calibrationOffsetC_;

  const bool inRange = std::isfinite(rawC) &&
                       rawC >= GLOBAL_MIN_VALID_TEMPERATURE_C &&
                       rawC <= GLOBAL_MAX_VALID_TEMPERATURE_C;

  if (!inRange) {
    reading_.ntcFault = NTC_FAULT_TEMPERATURE_RANGE;
    rejectSample(rawC, nowMs);
    return true;
  }

  acceptSample(rawC, nowMs, NTC_FILTER_ALPHA);
  return true;
}

void TemperatureSensor::acceptSample(float rawC, uint32_t nowMs,
                                     float filterAlpha) {
  reading_.rawC = rawC;
  reading_.timestampMs = nowMs;

  if (!reading_.valid || !std::isfinite(reading_.filteredC)) {
    reading_.filteredC = rawC;
  } else {
    reading_.filteredC += filterAlpha * (rawC - reading_.filteredC);
  }

  reading_.valid = true;
  consecutiveFailures_ = 0;
}

void TemperatureSensor::rejectSample(float fallbackRawC, uint32_t nowMs) {
  if (consecutiveFailures_ < 255) {
    ++consecutiveFailures_;
  }

  reading_.timestampMs = nowMs;
  if (consecutiveFailures_ >= MAX_CONSECUTIVE_SENSOR_FAILURES) {
    reading_.valid = false;
    reading_.rawC = fallbackRawC;
  } else if (reading_.valid) {
    // Preserve the last accepted value during a single noisy sample.
    reading_.rawC = reading_.filteredC;
  } else {
    reading_.rawC = fallbackRawC;
  }
}

const char *TemperatureSensor::backendName() const {
#if TEMP_SENSOR_BACKEND == TEMP_SENSOR_BACKEND_NTC_100K
  return "100k NTC";
#elif TEMP_SENSOR_BACKEND == TEMP_SENSOR_BACKEND_MAX31855
  return "MAX31855 K";
#else
  return "MAX31865 PT100";
#endif
}

const char *TemperatureSensor::faultDescription() const {
#if TEMP_SENSOR_BACKEND == TEMP_SENSOR_BACKEND_NTC_100K
  const uint8_t fault = reading_.ntcFault;
  if (fault & NTC_FAULT_OPEN) return "NTC open circuit";
  if (fault & NTC_FAULT_SHORT) return "NTC short circuit";
  if (fault & NTC_FAULT_RESISTANCE_RANGE) return "NTC resistance invalid";
  if (fault & NTC_FAULT_TEMPERATURE_RANGE) return "NTC temperature invalid";
  if (fault & NTC_FAULT_ADC) return "NTC ADC fault";
  if (!reading_.valid) return "Invalid NTC temperature";
  return "No sensor fault";
#elif TEMP_SENSOR_BACKEND == TEMP_SENSOR_BACKEND_MAX31855
  const uint8_t fault = reading_.max31855Fault;
  if (fault & MAX31855_FAULT_OPEN) return "Thermocouple open";
  if (fault & MAX31855_FAULT_SHORT_GND) return "Thermocouple short to GND";
  if (fault & MAX31855_FAULT_SHORT_VCC) return "Thermocouple short to VCC";
  if (!std::isfinite(reading_.coldJunctionC)) return "Cold junction invalid";
  if (reading_.coldJunctionC < MAX31855_MIN_COLD_JUNCTION_C ||
      reading_.coldJunctionC > MAX31855_MAX_COLD_JUNCTION_C) {
    return "Cold junction out of range";
  }
  if (!reading_.valid) return "Invalid thermocouple temperature";
  return "No sensor fault";
#else
  const uint8_t fault = reading_.max31865Fault;
  if (fault & MAX31865_FAULT_HIGHTHRESH) return "RTD above threshold";
  if (fault & MAX31865_FAULT_LOWTHRESH) return "RTD below threshold";
  if (fault & MAX31865_FAULT_REFINLOW) return "REFIN- above 0.85 bias";
  if (fault & MAX31865_FAULT_REFINHIGH) return "REFIN open";
  if (fault & MAX31865_FAULT_RTDINLOW) return "RTDIN- below 0.85 bias";
  if (fault & MAX31865_FAULT_OVUV) return "Over/undervoltage";
  if (!reading_.valid) return "Invalid temperature";
  return "No sensor fault";
#endif
}
