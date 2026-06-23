#pragma once

#include <Arduino.h>

// -----------------------------------------------------------------------------
// Target hardware: ESP32-S3-WROOM-1-N16, Arduino-ESP32 3.x
// Display: 240x240 ST7789 module with NO exposed CS pin
// Temperature: MAX31865 + PT100 on an independent SPI controller
// Heater: zero-cross AC SSR, time-proportioned output
// -----------------------------------------------------------------------------

// The CS-less display is permanently selected, so it must not share SCK/MOSI
// with another SPI peripheral. The ESP32-S3 provides two independent general-
// purpose SPI controllers, used here as FSPI for the display and HSPI for the
// MAX31865.

// ST7789 display bus (FSPI). The display is write-only, so MISO is unused.
constexpr int8_t PIN_TFT_SCK  = 12;  // Display SCL
constexpr int8_t PIN_TFT_MOSI = 11;  // Display SDA
constexpr int8_t PIN_TFT_DC   = 9;
constexpr int8_t PIN_TFT_RST  = 8;   // Display RES
constexpr int8_t PIN_TFT_BL   = 7;   // Display BLK
constexpr bool TFT_BACKLIGHT_ACTIVE_HIGH = true;
constexpr bool TFT_INVERT_COLORS = true;
constexpr uint8_t TFT_ROTATION = 0;
constexpr uint32_t TFT_SPI_HZ = 40000000UL;

// MAX31865 bus (HSPI).
constexpr int8_t PIN_MAX31865_CLK = 14;
constexpr int8_t PIN_MAX31865_SDO = 13;  // MAX31865 -> ESP32 MISO
constexpr int8_t PIN_MAX31865_SDI = 10;  // ESP32 MOSI -> MAX31865
constexpr int8_t PIN_MAX31865_CS  = 21;
constexpr int8_t PIN_MAX31865_RDY = -1;  // Optional; library polls instead
constexpr float RTD_NOMINAL_OHMS = 100.0f;    // PT100
constexpr float RTD_REFERENCE_OHMS = 430.0f;  // Confirm fitted reference resistor
constexpr uint8_t RTD_WIRE_COUNT = 3;         // 2, 3, or 4
constexpr bool RTD_USE_50HZ_FILTER = true;    // Tanzania / UK mains frequency

// User controls. Buttons connect the GPIO to GND when pressed.
constexpr int8_t PIN_BUTTON_LEFT   = 4;
constexpr int8_t PIN_BUTTON_MIDDLE = 5;
constexpr int8_t PIN_BUTTON_RIGHT  = 6;

// Emergency stop: normally-closed switch from GPIO to GND.
// Add an external 10 kOhm pull-up to 3.3 V. Healthy = LOW, fault/open = HIGH.
constexpr int8_t PIN_ESTOP = 15;
constexpr uint8_t ESTOP_ACTIVE_LEVEL = HIGH;

// SSR output. Active-high SSR input is strongly recommended.
constexpr int8_t PIN_SSR = 16;
constexpr bool SSR_ACTIVE_HIGH = true;

// Optional buzzer. Set to -1 to disable.
constexpr int8_t PIN_BUZZER = 17;
constexpr bool BUZZER_ACTIVE_HIGH = true;

// Optional cooling fan relay/MOSFET. Set to -1 to disable.
constexpr int8_t PIN_COOLING_FAN = 18;
constexpr bool FAN_ACTIVE_HIGH = true;

constexpr uint32_t SERIAL_BAUD = 115200;
constexpr uint32_t SENSOR_SAMPLE_INTERVAL_MS = 250;
constexpr uint32_t UI_REFRESH_INTERVAL_MS = 200;
constexpr uint32_t SSR_WINDOW_MS = 2000;
constexpr uint32_t SSR_MIN_PULSE_MS = 100;
constexpr float GLOBAL_MAX_TEMPERATURE_C = 285.0f;
constexpr float GLOBAL_MIN_VALID_TEMPERATURE_C = -40.0f;
constexpr float GLOBAL_MAX_VALID_TEMPERATURE_C = 350.0f;
constexpr uint8_t MAX_CONSECUTIVE_SENSOR_FAILURES = 3;

constexpr uint8_t MAX_PROFILES = 8;
constexpr uint8_t MAX_PROFILE_STAGES = 7;
constexpr uint8_t MAX_RUN_LOGS = 8;
constexpr uint16_t PROFILE_STORE_VERSION = 3;

constexpr uint8_t ssrOnLevel() {
  return SSR_ACTIVE_HIGH ? HIGH : LOW;
}

constexpr uint8_t ssrOffLevel() {
  return SSR_ACTIVE_HIGH ? LOW : HIGH;
}

constexpr uint8_t buzzerOnLevel() {
  return BUZZER_ACTIVE_HIGH ? HIGH : LOW;
}

constexpr uint8_t buzzerOffLevel() {
  return BUZZER_ACTIVE_HIGH ? LOW : HIGH;
}

constexpr uint8_t fanOnLevel() {
  return FAN_ACTIVE_HIGH ? HIGH : LOW;
}

constexpr uint8_t fanOffLevel() {
  return FAN_ACTIVE_HIGH ? LOW : HIGH;
}

static_assert(PIN_SSR >= 0, "SSR pin must be configured");
static_assert(PIN_ESTOP >= 0, "E-stop pin must be configured");
static_assert(PIN_TFT_SCK != PIN_MAX31865_CLK,
              "CS-less TFT and MAX31865 must use separate clock pins");
static_assert(PIN_TFT_MOSI != PIN_MAX31865_SDI,
              "CS-less TFT and MAX31865 must use separate data pins");
