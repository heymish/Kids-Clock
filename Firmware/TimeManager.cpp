#include "TimeManager.h"
#include "Config.h"
#include "Settings.h"

#include <WiFi.h>

static unsigned long lastNtpSyncAttempt = 0;

void configureTime() {
  configTzTime(settings.timezoneString.c_str(), settings.ntpServer.c_str());
  lastNtpSyncAttempt = millis();
}

bool timeIsValid() {
  struct tm timeinfo;
  return getLocalTime(&timeinfo, 1000);
}

int timeStringToMinutes(const String& hhmm) {
  if (hhmm.length() < 5) {
    return 0;
  }

  int hour = hhmm.substring(0, 2).toInt();
  int minute = hhmm.substring(3, 5).toInt();

  hour = constrain(hour, 0, 23);
  minute = constrain(minute, 0, 59);

  return hour * 60 + minute;
}

bool isNightDimTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 10)) {
    return false;
  }

  int nowMinutes = timeinfo.tm_hour * 60 + timeinfo.tm_min;
  int startMinutes = timeStringToMinutes(settings.dimStart);
  int endMinutes = timeStringToMinutes(settings.dimEnd);

  if (startMinutes == endMinutes) {
    return false;
  }

  if (startMinutes < endMinutes) {
    return nowMinutes >= startMinutes && nowMinutes < endMinutes;
  }

  return nowMinutes >= startMinutes || nowMinutes < endMinutes;
}

void syncTimeIfNeeded() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  if (millis() - lastNtpSyncAttempt >= NTP_SYNC_INTERVAL_MS) {
    configureTime();
  }
}
