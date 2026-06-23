#include "Safety.h"

#include "Config.h"

namespace {
volatile bool g_estopLatched = false;
volatile bool g_heaterInhibit = true;

void ARDUINO_ISR_ATTR estopIsr() {
  // Arduino-ESP32 marks digitalWrite's low-level implementation as ISR-safe.
  digitalWrite(PIN_SSR, ssrOffLevel());
  g_heaterInhibit = true;
  g_estopLatched = true;
}
}  // namespace

void safetyBegin() {
  // This must be called before initializing SPI, the display, or the sensor.
  // Preload the inactive level, then enable the output driver. The external
  // hardware pull-down/pull-up remains the authoritative boot-state guard.
  digitalWrite(PIN_SSR, ssrOffLevel());
  pinMode(PIN_SSR, OUTPUT);
  digitalWrite(PIN_SSR, ssrOffLevel());
  g_heaterInhibit = true;

  pinMode(PIN_ESTOP, INPUT_PULLUP);
  if (digitalRead(PIN_ESTOP) == ESTOP_ACTIVE_LEVEL) {
    g_estopLatched = true;
  }

  attachInterrupt(digitalPinToInterrupt(PIN_ESTOP), estopIsr, RISING);

  // A healthy, closed NC circuit allows software control after initialization.
  if (!g_estopLatched) {
    g_heaterInhibit = false;
  }
}

bool safetyEstopLatched() {
  return g_estopLatched;
}

bool safetyHeaterInhibited() {
  return g_heaterInhibit;
}

bool safetyEstopCircuitHealthy() {
  return digitalRead(PIN_ESTOP) != ESTOP_ACTIVE_LEVEL;
}

bool safetyResetEstopLatch() {
  digitalWrite(PIN_SSR, ssrOffLevel());
  if (!safetyEstopCircuitHealthy()) {
    return false;
  }
  noInterrupts();
  g_estopLatched = false;
  g_heaterInhibit = false;
  interrupts();
  return true;
}

void safetyHardHeaterOff() {
  digitalWrite(PIN_SSR, ssrOffLevel());
  g_heaterInhibit = true;
}
