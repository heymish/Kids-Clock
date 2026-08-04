#include "Timezones.h"

const TimezoneOption timezoneOptions[] = {
  { "New Zealand", "NZST-12NZDT,M9.5.0,M4.1.0/3" },
  { "Australia Sydney / Melbourne", "AEST-10AEDT,M10.1.0,M4.1.0/3" },
  { "Australia Brisbane", "AEST-10" },
  { "Australia Perth", "AWST-8" },
  { "United Kingdom", "GMT0BST,M3.5.0/1,M10.5.0" },
  { "US Eastern", "EST5EDT,M3.2.0,M11.1.0" },
  { "US Central", "CST6CDT,M3.2.0,M11.1.0" },
  { "US Mountain", "MST7MDT,M3.2.0,M11.1.0" },
  { "US Pacific", "PST8PDT,M3.2.0,M11.1.0" },
  { "UTC", "UTC0" }
};

const int timezoneOptionCount = sizeof(timezoneOptions) / sizeof(timezoneOptions[0]);

bool isKnownTimezone(const String& tz) {
  for (int i = 0; i < timezoneOptionCount; i++) {
    if (tz == timezoneOptions[i].posix) {
      return true;
    }
  }

  return false;
}
