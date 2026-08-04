#include "Settings.h"
#include "Config.h"

#include <Preferences.h>

ClockSettings settings;
static Preferences prefs;

void loadSettings() {
  prefs.begin("clock", true);

  settings.wifiSsid = prefs.getString("ssid", "");
  settings.wifiPassword = prefs.getString("pass", "");

  settings.useDhcp = prefs.getBool("dhcp", true);
  settings.staticIp = prefs.getString("ip", "192.168.1.80");
  settings.gateway = prefs.getString("gw", "192.168.1.1");
  settings.subnet = prefs.getString("subnet", "255.255.255.0");
  settings.dns1 = prefs.getString("dns1", "8.8.8.8");
  settings.dns2 = prefs.getString("dns2", "1.1.1.1");

  settings.ntpServer = prefs.getString("ntp", DEFAULT_NTP_SERVER);
  settings.timezoneString = prefs.getString("tz", DEFAULT_TIMEZONE);

  settings.dayBrightness = prefs.getInt("dayBright", 8);
  settings.nightBrightness = prefs.getInt("nightBright", 1);
  settings.dimStart = prefs.getString("dimStart", "20:00");
  settings.dimEnd = prefs.getString("dimEnd", "07:00");

  prefs.end();

  settings.dayBrightness = constrain(settings.dayBrightness, 0, 15);
  settings.nightBrightness = constrain(settings.nightBrightness, 0, 15);
}

void saveSettings() {
  prefs.begin("clock", false);

  prefs.putString("ssid", settings.wifiSsid);
  prefs.putString("pass", settings.wifiPassword);

  prefs.putBool("dhcp", settings.useDhcp);
  prefs.putString("ip", settings.staticIp);
  prefs.putString("gw", settings.gateway);
  prefs.putString("subnet", settings.subnet);
  prefs.putString("dns1", settings.dns1);
  prefs.putString("dns2", settings.dns2);

  prefs.putString("ntp", settings.ntpServer);
  prefs.putString("tz", settings.timezoneString);

  prefs.putInt("dayBright", constrain(settings.dayBrightness, 0, 15));
  prefs.putInt("nightBright", constrain(settings.nightBrightness, 0, 15));
  prefs.putString("dimStart", settings.dimStart);
  prefs.putString("dimEnd", settings.dimEnd);

  prefs.end();
}
