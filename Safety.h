#pragma once

#include <Arduino.h>

void safetyBegin();
bool safetyEstopLatched();
bool safetyHeaterInhibited();
bool safetyEstopCircuitHealthy();
bool safetyResetEstopLatch();
void safetyHardHeaterOff();
