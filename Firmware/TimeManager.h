#pragma once

#include <Arduino.h>
#include <time.h>

void configureTime();
bool timeIsValid();
int timeStringToMinutes(const String& hhmm);
bool isNightDimTime();
void syncTimeIfNeeded();
