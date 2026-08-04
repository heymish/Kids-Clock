#pragma once

#include <Arduino.h>

struct TimezoneOption {
  const char* label;
  const char* posix;
};

extern const TimezoneOption timezoneOptions[];
extern const int timezoneOptionCount;

bool isKnownTimezone(const String& tz);
