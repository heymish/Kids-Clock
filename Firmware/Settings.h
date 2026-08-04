#pragma once

#include <Arduino.h>

struct ClockSettings {
  String wifiSsid;
  String wifiPassword;

  bool useDhcp;
  String staticIp;
  String gateway;
  String subnet;
  String dns1;
  String dns2;

  String ntpServer;
  String timezoneString;

  int dayBrightness;
  int nightBrightness;
  String dimStart;
  String dimEnd;
};

extern ClockSettings settings;

void loadSettings();
void saveSettings();
