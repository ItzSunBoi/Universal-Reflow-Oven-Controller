#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <Adafruit_MAX31865.h>
#include <SPI.h>

#include "ButtonInput.h"
#include "Config.h"
#include "HeaterController.h"
#include "ProfileStore.h"
#include "ReflowEngine.h"
#include "Safety.h"
#include "TemperatureSensor.h"
#include "UiManager.h"

// The display has no exposed CS pin and is permanently selected. It therefore
// receives its own hardware SPI controller and physical SCK/MOSI wires.
SPIClass displaySpi(FSPI);
SPIClass max31865Spi(HSPI);

// -1 tells the Adafruit ST7789 driver that the display has no controllable CS.
Adafruit_ST7789 display(&displaySpi, -1, PIN_TFT_DC, PIN_TFT_RST);

ButtonInput buttons;
ProfileStore profileStore;
TemperatureSensor temperatureSensor(max31865Spi);
HeaterController heater;
ReflowEngine reflowEngine(heater);
UiManager ui(display, profileStore, reflowEngine, temperatureSensor);

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
  // The TFT has no CS line. Only the MAX31865 CS needs an idle level.
  pinMode(PIN_MAX31865_CS, OUTPUT);
  digitalWrite(PIN_MAX31865_CS, HIGH);

  // Keep the display control lines in benign states before bus startup.
  pinMode(PIN_TFT_DC, OUTPUT);
  digitalWrite(PIN_TFT_DC, HIGH);
  pinMode(PIN_TFT_RST, OUTPUT);
  digitalWrite(PIN_TFT_RST, HIGH);
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

  if (reflowEngine.state() == RunState::FAULT || safetyEstopLatched()) {
    fanOn = false;
  }
  digitalWrite(PIN_COOLING_FAN, fanOn ? fanOnLevel() : fanOffLevel());
}

void printStartupSummary() {
  Serial.println();
  Serial.println("Universal Reflow Controller v1.2");
  Serial.println("Target: ESP32-S3-WROOM-1-N16");
  Serial.printf("TFT FSPI: SCK=%d MOSI=%d CS=none DC=%d RST=%d\n",
                PIN_TFT_SCK, PIN_TFT_MOSI, PIN_TFT_DC, PIN_TFT_RST);
  Serial.printf("MAX31865 HSPI: CLK=%d SDI=%d SDO=%d CS=%d\n",
                PIN_MAX31865_CLK, PIN_MAX31865_SDI,
                PIN_MAX31865_SDO, PIN_MAX31865_CS);
  Serial.printf("Profiles loaded: %u\n", profileStore.profileCount());
  Serial.printf("E-stop circuit: %s\n",
                safetyEstopCircuitHealthy() ? "healthy" : "OPEN/FAULT");
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

  // Independent physical buses are mandatory because the display is always
  // selected and would interpret MAX31865 clock edges as display traffic.
  displaySpi.begin(PIN_TFT_SCK, -1, PIN_TFT_MOSI, -1);
  max31865Spi.begin(PIN_MAX31865_CLK, PIN_MAX31865_SDO,
                    PIN_MAX31865_SDI, PIN_MAX31865_CS);

  display.init(240, 240, SPI_MODE0);
  display.setSPISpeed(TFT_SPI_HZ);
  display.setRotation(TFT_ROTATION);
  display.invertDisplay(TFT_INVERT_COLORS);
  display.fillScreen(ST77XX_BLACK);

  if (PIN_TFT_BL >= 0) {
    pinMode(PIN_TFT_BL, OUTPUT);
    digitalWrite(PIN_TFT_BL,
                 TFT_BACKLIGHT_ACTIVE_HIGH ? HIGH : LOW);
  }

  profileStore.begin();
  temperatureSensor.setCalibrationOffset(
      profileStore.settings().temperatureOffsetC);
  const bool sensorStarted = temperatureSensor.begin(configuredWireMode());

  buttons.begin();
  ui.begin();

  if (!sensorStarted) {
    reflowEngine.triggerFault(FaultCode::SENSOR,
                              "MAX31865 initialization failed");
  } else if (safetyEstopLatched()) {
    reflowEngine.triggerFault(FaultCode::ESTOP,
                              "E-stop circuit open at boot");
  }

  printStartupSummary();
}

void loop() {
  const uint32_t nowMs = millis();

  buttons.update(nowMs);
  ButtonEvent event;
  while (buttons.nextEvent(event)) {
    ui.handleButton(event, nowMs);
  }

  temperatureSensor.update(nowMs);

  if (safetyEstopLatched() &&
      reflowEngine.faultCode() != FaultCode::ESTOP) {
    reflowEngine.triggerFault(FaultCode::ESTOP,
                              "E-stop circuit opened");
  }

  reflowEngine.update(temperatureSensor.reading(), nowMs);

  if (reflowEngine.hasPendingLog()) {
    profileStore.addRunLog(reflowEngine.consumePendingLog());
  }

  heater.setDemand(reflowEngine.heaterDemandPercent());
  const bool inhibit = safetyHeaterInhibited() ||
                       reflowEngine.state() == RunState::FAULT ||
                       !temperatureSensor.valid();
  heater.update(nowMs, inhibit);

  updateCoolingFan();
  ui.update(nowMs);

  // Cooperative loop: no blocking delays are used in control or UI code.
  delay(1);
}
