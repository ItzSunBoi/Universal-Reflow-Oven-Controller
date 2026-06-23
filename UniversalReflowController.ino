#include <Adafruit_GFX.h>
#include <Adafruit_MAX31865.h>
#include <SPI.h>

#include "BacklightController.h"
#include "CslessST7789.h"
#include "ButtonInput.h"
#include "Config.h"
#include "HeaterController.h"
#include "ProfileStore.h"
#include "OtaManager.h"
#include "PidAutotuner.h"
#include "ReflowEngine.h"
#include "Safety.h"
#include "TemperatureSensor.h"
#include "UiManager.h"

// The display has no exposed CS pin and is permanently selected. It therefore
// receives its own hardware SPI controller and physical SCK/MOSI wires.
SPIClass displaySpi(FSPI);
SPIClass max31865Spi(HSPI);

// Custom driver reproducing the known-working no-CS mode-2 command stream.
CslessST7789 display(displaySpi, PIN_TFT_DC, PIN_TFT_RST);

BacklightController backlight;
ButtonInput buttons;
ProfileStore profileStore;
TemperatureSensor temperatureSensor(max31865Spi);
HeaterController heater;
ReflowEngine reflowEngine(heater);
PidAutotuner pidAutotuner(heater);
OtaManager otaManager(heater);
UiManager ui(display, profileStore, reflowEngine, temperatureSensor, backlight,
             heater, pidAutotuner, otaManager);

namespace {
max31865_numwires_t configuredWireMode() {
  switch (RTD_WIRE_COUNT) {
    case 2: return MAX31865_2WIRE;
    case 4: return MAX31865_4WIRE;
    case 3:
    default: return MAX31865_3WIRE;
  }
}

void initializePeripheralPins() {
#if !USE_NTC_100K_SENSOR
  // The TFT has no CS line. Only the MAX31865 CS needs an idle level.
  pinMode(PIN_MAX31865_CS, OUTPUT);
  digitalWrite(PIN_MAX31865_CS, HIGH);
#endif

  // Keep the display control lines in benign states before bus startup.
  pinMode(PIN_TFT_DC, OUTPUT);
  digitalWrite(PIN_TFT_DC, HIGH);
  if (PIN_TFT_RST >= 0) {
    pinMode(PIN_TFT_RST, OUTPUT);
    digitalWrite(PIN_TFT_RST, HIGH);
  }
}

void updateCoolingFan() {
  if (PIN_COOLING_FAN < 0) return;

  bool fanOn = false;
  if (profileStore.settings().fanDuringCool && temperatureSensor.valid()) {
    if (reflowEngine.state() == RunState::COMPLETE) {
      fanOn = temperatureSensor.temperatureC() > 50.0f;
    } else if (reflowEngine.state() == RunState::RUNNING &&
               reflowEngine.stageIndex() <
                   reflowEngine.activeProfile().stageCount) {
      const ReflowStage &stage =
          reflowEngine.activeProfile().stages[reflowEngine.stageIndex()];
      fanOn = stage.mode == StageMode::COOL &&
              temperatureSensor.temperatureC() > stage.targetC;
    }
  }

  if (reflowEngine.state() == RunState::FAULT) {
    fanOn = false;
  }
  digitalWrite(PIN_COOLING_FAN, fanOn ? fanOnLevel() : fanOffLevel());
}

void printStartupSummary() {
  Serial.println();
  Serial.println("Universal Reflow Controller v1.9.1");
  Serial.println("Target: ESP32-S3-WROOM-1-N16");
  Serial.printf("TFT FSPI mode 2: SCK=%d MOSI=%d CS=none DC=%d RST=%d init=%lu Hz draw=%lu Hz\n",
                PIN_TFT_SCK, PIN_TFT_MOSI, PIN_TFT_DC, PIN_TFT_RST,
                static_cast<unsigned long>(TFT_INIT_SPI_HZ),
                static_cast<unsigned long>(TFT_SPI_HZ));
#if USE_NTC_100K_SENSOR
  Serial.printf("Temperature: 100k NTC on ADC GPIO%d, beta=%.0f, fixed=%.1f ohm\n",
                PIN_NTC_ADC, NTC_BETA_COEFFICIENT_K,
                NTC_FIXED_RESISTOR_OHMS);
#else
  Serial.printf("Temperature: MAX31865 HSPI CLK=%d SDI=%d SDO=%d CS=%d\n",
                PIN_MAX31865_CLK, PIN_MAX31865_SDI,
                PIN_MAX31865_SDO, PIN_MAX31865_CS);
#endif
  Serial.printf("Profiles loaded: %u\n", profileStore.profileCount());
}
}  // namespace

void setup() {
  Serial.begin(SERIAL_BAUD);

  // Safety comes first. The SSR is forced inactive before any peripheral
  // initialization or potentially blocking display operation.
  safetyBegin();
  heater.begin();

  if (PIN_COOLING_FAN >= 0) {
    pinMode(PIN_COOLING_FAN, OUTPUT);
    digitalWrite(PIN_COOLING_FAN, fanOffLevel());
  }

  initializePeripheralPins();

  // Attach PWM before display startup and hold the backlight off. This avoids
  // the bright white flash common while an ST7789 is being initialized.
  if (!backlight.begin()) {
    Serial.println("WARNING: TFT backlight PWM initialization failed");
  }
  backlight.off();

#if !USE_NTC_100K_SENSOR
  // Independent physical buses are mandatory because the display is always
  // selected and would interpret MAX31865 clock edges as display traffic.
  max31865Spi.begin(PIN_MAX31865_CLK, PIN_MAX31865_SDO,
                    PIN_MAX31865_SDI, PIN_MAX31865_CS);
#endif

  // This uses the exact configuration proven on the physical module:
  // no CS, SPI mode 2, COLMOD 0x05, INVON, NORON, then DISPON.
  display.begin(PIN_TFT_SCK, PIN_TFT_MOSI, TFT_INIT_SPI_HZ, TFT_ROTATION,
                TFT_INVERT_COLORS);
  display.setSPISpeed(TFT_SPI_HZ);
  display.fillScreen(0x0000);

  profileStore.begin();
  heater.setPidTunings(profileStore.settings().pidKp,
                       profileStore.settings().pidKi,
                       profileStore.settings().pidKd);
  otaManager.begin();
  temperatureSensor.setCalibrationOffset(
      profileStore.settings().temperatureOffsetC);
  const bool sensorStarted = temperatureSensor.begin(configuredWireMode());

  buttons.begin();
  Serial.printf("Button scanner: %s\n",
                buttons.asynchronous() ? "asynchronous core task" :
                                         "loop fallback");
  ui.begin();
  ui.update(millis());
  backlight.setPercent(profileStore.settings().backlightPercent);

  // Peripheral initialization is complete. Release the startup-only software
  // inhibit; sensor validity and the reflow state still gate every SSR pulse.
  safetyArmHeaterControl();

  if (!sensorStarted) {
    reflowEngine.triggerFault(FaultCode::SENSOR,
                              "Temperature sensor initialization failed");
  }

  printStartupSummary();
}

void loop() {
  const uint32_t nowMs = millis();

  // Normally button scanning runs in a dedicated FreeRTOS task on core 0.
  // service() is a no-op unless task creation failed, in which case it
  // provides a cooperative fallback.
  buttons.service(nowMs);
  ButtonEvent event;
  while (buttons.nextEvent(event)) {
    ui.handleButton(event, nowMs);
  }

  temperatureSensor.update(nowMs);
  otaManager.update(nowMs);

  if (pidAutotuner.active()) {
    pidAutotuner.update(temperatureSensor.reading(), nowMs);
  } else if (!otaManager.active()) {
    reflowEngine.update(temperatureSensor.reading(), nowMs);
  }

  if (reflowEngine.hasPendingLog()) {
    profileStore.addRunLog(reflowEngine.consumePendingLog());
  }

  float heaterDemand = reflowEngine.heaterDemandPercent();
  if (pidAutotuner.active()) {
    heaterDemand = pidAutotuner.demandPercent();
  }
  if (otaManager.active()) {
    heaterDemand = 0.0f;
  }
  heater.setDemand(heaterDemand);

  const bool inhibit = safetyHeaterInhibited() || otaManager.active() ||
                       reflowEngine.state() == RunState::FAULT ||
                       pidAutotuner.failed() || !temperatureSensor.valid();
  heater.update(nowMs, inhibit);

  updateCoolingFan();
  ui.update(nowMs);

  // The loop contains no long delay-based waits. LCD transfers are bounded and
  // button sampling continues independently on core 0.
  delay(1);
}
