#pragma once

#include <Arduino.h>

// Initializes the SSR output in its inactive state before any peripheral setup.
void safetyBegin();

// Releases the startup inhibit after setup has completed.
void safetyArmHeaterControl();

// True while the startup/hard-off inhibit is active.
bool safetyHeaterInhibited();

// Immediately commands the SSR inactive and latches the software inhibit.
void safetyHardHeaterOff();
