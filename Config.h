#pragma once

#include <Arduino.h>

// -----------------------------------------------------------------------------
// Target hardware: ESP32-S3-WROOM-1-N16, Arduino-ESP32 3.x
// Display: 240x240 ST7789 module with NO exposed CS pin
// Temperature: selectable MAX31865/PT100 or temporary 100 kOhm NTC backend
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
//   E     = selected temperature sensor backend
//   F     = SSR interface
//   G     = optional cooling fan
//
// A module may occupy more than one connector group, but connector groups are
// never shared between different modules.

// ST7789 display bus (FSPI). The display is write-only, so MISO is unused.
// Its fixed-low internal CS requires CPOL=1, CPHA=0 (SPI mode 2).
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

// Idle backlight policy. Values are persisted in NVS, while these constants
// provide defaults for new installations and migration from earlier firmware.
constexpr uint8_t TFT_IDLE_DIM_DEFAULT_SECONDS = 60;
constexpr uint8_t TFT_IDLE_OFF_DEFAULT_MINUTES = 10;
constexpr uint8_t TFT_IDLE_DIM_DEFAULT_PERCENT = 20;
constexpr uint8_t TFT_IDLE_TIMEOUT_DISABLED = 0xFF;
constexpr uint32_t TFT_WAKE_EVENT_GUARD_MS = 900UL;

constexpr bool TFT_INVERT_COLORS = true;
constexpr uint8_t TFT_ROTATION = 0;
// Initialize using the exact speed proven by the successful ESP32-S3 test,
// then use a faster but conservative clock for full-screen UI transfers.
constexpr uint32_t TFT_INIT_SPI_HZ = 4000000UL;
constexpr uint32_t TFT_SPI_HZ = 40000000UL;

// -----------------------------------------------------------------------------
// Temperature sensor backend
// -----------------------------------------------------------------------------
// Set to 1 for the temporary 100 kOhm NTC divider. Set back to 0 when the
// MAX31865/PT100 is repaired or replaced. Both implementations remain compiled
// into the project source; this flag only selects which backend is initialized.
#define USE_NTC_100K_SENSOR 1

// 100 kOhm NTC input. GPIO9 is ADC1-capable on ESP32-S3 and reuses the
// MAX31865 SDO connector position while the MAX31865 backend is disabled.
constexpr int8_t PIN_NTC_ADC = 9;
constexpr float NTC_NOMINAL_OHMS = 100000.0f;
constexpr float NTC_NOMINAL_TEMPERATURE_C = 25.0f;
constexpr float NTC_BETA_COEFFICIENT_K = 3950.0f;

// Recommended divider for the reflow range:
//   3.3 V --- NTC --- ADC GPIO9 --- fixed resistor --- GND
// Set NTC_IS_HIGH_SIDE=false for the opposite arrangement:
//   3.3 V --- fixed resistor --- ADC GPIO9 --- NTC --- GND
constexpr bool NTC_IS_HIGH_SIDE = true;
constexpr float NTC_FIXED_RESISTOR_OHMS = 2200.0f;
constexpr uint16_t NTC_DIVIDER_SUPPLY_MV = 3300;
constexpr uint8_t NTC_ADC_RESOLUTION_BITS = 12;
constexpr uint8_t NTC_ADC_SAMPLE_COUNT = 24;
constexpr uint16_t NTC_ADC_MIN_VALID_MV = 15;
constexpr uint16_t NTC_ADC_MAX_VALID_MV = 3075;
constexpr float NTC_MIN_VALID_RESISTANCE_OHMS = 40.0f;
constexpr float NTC_MAX_VALID_RESISTANCE_OHMS = 2000000.0f;
constexpr float NTC_FILTER_ALPHA = 0.18f;

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
constexpr int8_t PIN_BUZZER = -1;  // Dedicated header group B
constexpr bool BUZZER_ACTIVE_HIGH = true;

// Optional cooling fan relay/MOSFET. Set to -1 to disable.
constexpr int8_t PIN_COOLING_FAN = -1;  // Dedicated connector group G
constexpr bool FAN_ACTIVE_HIGH = true;

constexpr uint32_t SERIAL_BAUD = 115200;
constexpr uint32_t SENSOR_SAMPLE_INTERVAL_MS = 250;
constexpr uint32_t UI_REFRESH_INTERVAL_MS = 200;

// UI rendering uses a 16-bit off-screen framebuffer and only transfers tiles
// whose pixel content changed. 24 px divides the 240x240 panel exactly.
constexpr uint8_t UI_DIRTY_TILE_SIZE = 24;

// Buttons are scanned independently from the Arduino loop on the other ESP32-S3
// core, so a long display transfer cannot hide a press.
constexpr uint32_t BUTTON_SCAN_INTERVAL_MS = 2;
constexpr uint16_t BUTTON_EVENT_QUEUE_LENGTH = 32;
constexpr uint16_t BUTTON_TASK_STACK_BYTES = 3072;
constexpr uint8_t BUTTON_TASK_PRIORITY = 2;
constexpr int8_t BUTTON_TASK_CORE = 0;

// Persisted heater PID defaults. PID autotune can replace these values in NVS.
constexpr float PID_DEFAULT_KP = 5.0f;
constexpr float PID_DEFAULT_KI = 0.10f;
constexpr float PID_DEFAULT_KD = 18.0f;

// Relay-feedback PID autotune. The heater alternates between 0% and this
// bounded high output around the selected target, similar to printer firmware
// M303-style tuning, while global overtemperature limits remain active.
constexpr float PID_AUTOTUNE_DEFAULT_TARGET_C = 200.0f;
constexpr float PID_AUTOTUNE_MIN_TARGET_C = 100.0f;
constexpr float PID_AUTOTUNE_MAX_TARGET_C = 230.0f;
constexpr float PID_AUTOTUNE_TARGET_STEP_C = 5.0f;
constexpr float PID_AUTOTUNE_HYSTERESIS_C = 2.0f;
constexpr float PID_AUTOTUNE_RELAY_HIGH_PERCENT = 70.0f;
constexpr float PID_AUTOTUNE_MAX_OVERSHOOT_C = 25.0f;
constexpr uint8_t PID_AUTOTUNE_CYCLES = 6;
constexpr uint32_t PID_AUTOTUNE_PHASE_TIMEOUT_MS = 8UL * 60UL * 1000UL;
constexpr uint32_t PID_AUTOTUNE_TOTAL_TIMEOUT_MS = 30UL * 60UL * 1000UL;

// Local, explicit browser OTA session. Wi-Fi stays off until the user opens
// Settings -> OTA update and presses START. The ESP32 creates a password-
// protected access point and automatically closes it after the timeout.
constexpr uint8_t OTA_WIFI_CHANNEL = 6;
constexpr uint8_t OTA_MAX_CLIENTS = 1;
constexpr uint32_t OTA_SESSION_TIMEOUT_MS = 10UL * 60UL * 1000UL;
constexpr uint32_t OTA_RESTART_DELAY_MS = 1800UL;

constexpr uint32_t SSR_WINDOW_MS = 2000;
constexpr uint32_t SSR_MIN_PULSE_MS = 100;
constexpr float GLOBAL_MAX_TEMPERATURE_C = 285.0f;
constexpr float GLOBAL_MIN_VALID_TEMPERATURE_C = -40.0f;
constexpr float GLOBAL_MAX_VALID_TEMPERATURE_C = 350.0f;
constexpr uint8_t MAX_CONSECUTIVE_SENSOR_FAILURES = 3;

constexpr uint8_t MAX_PROFILES = 8;
constexpr uint8_t MAX_PROFILE_STAGES = 10;
constexpr uint8_t MAX_RUN_LOGS = 8;
constexpr uint16_t PROFILE_STORE_VERSION = 4;

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

static_assert(USE_NTC_100K_SENSOR == 0 || USE_NTC_100K_SENSOR == 1,
              "USE_NTC_100K_SENSOR must be 0 or 1");
static_assert(PIN_SSR >= 0, "SSR pin must be configured");
static_assert(PIN_NTC_ADC >= 1 && PIN_NTC_ADC <= 10,
              "NTC ADC should use an ESP32-S3 ADC1 GPIO (1-10)");
static_assert(NTC_FIXED_RESISTOR_OHMS > 0.0f,
              "NTC divider resistor must be positive");
static_assert(NTC_BETA_COEFFICIENT_K > 0.0f,
              "NTC beta coefficient must be positive");
static_assert(NTC_ADC_SAMPLE_COUNT > 0,
              "NTC ADC sample count must be nonzero");
static_assert(NTC_ADC_MIN_VALID_MV < NTC_ADC_MAX_VALID_MV,
              "NTC ADC valid voltage range is inverted");
static_assert(NTC_ADC_MAX_VALID_MV < NTC_DIVIDER_SUPPLY_MV,
              "NTC maximum ADC voltage must be below divider supply");
static_assert(PIN_TFT_SCK != PIN_MAX31865_CLK,
              "CS-less TFT and MAX31865 must use separate clock pins");
static_assert(PIN_TFT_MOSI != PIN_MAX31865_SDI,
              "CS-less TFT and MAX31865 must use separate data pins");
static_assert(UI_DIRTY_TILE_SIZE > 0 && (240 % UI_DIRTY_TILE_SIZE) == 0,
              "UI dirty tile size must divide the 240-pixel panel");
static_assert(BUTTON_EVENT_QUEUE_LENGTH >= 8,
              "Button event queue is too small for long display updates");
