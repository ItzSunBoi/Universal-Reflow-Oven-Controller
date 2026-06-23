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

// Physical connector groups on the reused carrier PCB:
// A: GPIO13, GPIO14, GPIO35 + 3.3 V + GND
// B: GPIO36, GPIO3, GPIO21, GPIO47, GPIO48, GPIO46, GPIO45
// C: GPIO4, GPIO5, GPIO6, GPIO42 + 5 V + GND
// D: GPIO10, GPIO11, GPIO12, GPIO41 + 5 V + GND
// E: GPIO8, GPIO9, GPIO18, GPIO40 + 5 V + GND
// F: GPIO39, GPIO17, GPIO16, GPIO15 + 5 V + GND
// G: GPIO1, GPIO2, GPIO7, GPIO38 + 5 V + GND
//
// Allocation:
//   A + D = TFT display (A provides PWM backlight and optional 3.3 V power;
//                       D provides SCK, MOSI, DC, and RESET)
//   B     = optional buzzer
//   C     = three-button control panel
//   E     = MAX31865
//   F     = SSR interface
//   G     = optional cooling fan
//
// A module may occupy more than one connector group, but connector groups are
// never shared between different modules.

// ST7789 display bus (FSPI). The display is write-only, so MISO is unused.
constexpr int8_t PIN_TFT_SCK  = 12;  // Display SCL; connector group D
constexpr int8_t PIN_TFT_MOSI = 11;  // Display SDA; connector group D
constexpr int8_t PIN_TFT_DC   = 41;  // Connector group D
constexpr int8_t PIN_TFT_RST  = 10;  // Display RES; connector group D
constexpr int8_t PIN_TFT_BL   = 13;  // PWM to module BLK MOSFET; group A
constexpr bool TFT_BACKLIGHT_ACTIVE_HIGH = true;
constexpr uint32_t TFT_BACKLIGHT_PWM_HZ = 20000UL;
constexpr uint8_t TFT_BACKLIGHT_PWM_BITS = 10;
constexpr uint8_t TFT_BACKLIGHT_DEFAULT_PERCENT = 80;
constexpr uint8_t TFT_BACKLIGHT_MIN_PERCENT = 10;
constexpr uint8_t TFT_BACKLIGHT_STEP_PERCENT = 10;
constexpr bool TFT_INVERT_COLORS = true;
constexpr uint8_t TFT_ROTATION = 0;
constexpr uint32_t TFT_SPI_HZ = 40000000UL;

// MAX31865 bus (HSPI).
constexpr int8_t PIN_MAX31865_CLK = 8;   // Connector group E
constexpr int8_t PIN_MAX31865_SDO = 9;   // MAX31865 -> ESP32 MISO; group E
constexpr int8_t PIN_MAX31865_SDI = 18;  // ESP32 MOSI -> MAX31865; group E
constexpr int8_t PIN_MAX31865_CS  = 40;  // Connector group E
constexpr int8_t PIN_MAX31865_RDY = -1;  // Optional; library polls instead
constexpr float RTD_NOMINAL_OHMS = 100.0f;    // PT100
constexpr float RTD_REFERENCE_OHMS = 430.0f;  // Confirm fitted reference resistor
constexpr uint8_t RTD_WIRE_COUNT = 3;         // 2, 3, or 4
constexpr bool RTD_USE_50HZ_FILTER = true;    // Tanzania / UK mains frequency

// User controls. Buttons connect the GPIO to GND when pressed.
constexpr int8_t PIN_BUTTON_LEFT   = 4;
constexpr int8_t PIN_BUTTON_MIDDLE = 5;
constexpr int8_t PIN_BUTTON_RIGHT  = 6;

// SSR output. Active-high SSR input is strongly recommended.
constexpr int8_t PIN_SSR = 16;
constexpr bool SSR_ACTIVE_HIGH = true;

// Optional buzzer. Set to -1 to disable.
constexpr int8_t PIN_BUZZER = 21;  // Dedicated header group B
constexpr bool BUZZER_ACTIVE_HIGH = true;

// Optional cooling fan relay/MOSFET. Set to -1 to disable.
constexpr int8_t PIN_COOLING_FAN = 38;  // Dedicated connector group G
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
static_assert(PIN_TFT_SCK != PIN_MAX31865_CLK,
              "CS-less TFT and MAX31865 must use separate clock pins");
static_assert(PIN_TFT_MOSI != PIN_MAX31865_SDI,
              "CS-less TFT and MAX31865 must use separate data pins");
