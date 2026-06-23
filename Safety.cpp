#include "Safety.h"

#include "Config.h"

namespace {
volatile bool g_heaterInhibit = true;
}  // namespace

void safetyBegin() {
  // Must run before SPI, display, sensor, or UI initialization. Preload the
  // inactive level before enabling the output driver. An external resistor on
  // the SSR driver input remains the authoritative reset/boot-state guard.
  digitalWrite(PIN_SSR, ssrOffLevel());
  pinMode(PIN_SSR, OUTPUT);
  digitalWrite(PIN_SSR, ssrOffLevel());
  g_heaterInhibit = true;
}

void safetyArmHeaterControl() {
  digitalWrite(PIN_SSR, ssrOffLevel());
  g_heaterInhibit = false;
}

bool safetyHeaterInhibited() {
  return g_heaterInhibit;
}

void safetyHardHeaterOff() {
  digitalWrite(PIN_SSR, ssrOffLevel());
  g_heaterInhibit = true;
}
