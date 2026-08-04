#include "ClockWifi.h"
#include "Config.h"
#include "Settings.h"
#include "DisplayManager.h"
#include "TimeManager.h"

#include <WiFi.h>

bool parseIpAddress(const String& text, IPAddress& ip) {
  return ip.fromString(text);
}

void startSetupAccessPoint() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(DEFAULT_AP_SSID, DEFAULT_AP_PASSWORD);
  showMessage("SETUP");
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);

  if (!settings.useDhcp) {
    IPAddress ip;
    IPAddress gw;
    IPAddress sn;
    IPAddress d1;
    IPAddress d2;

    if (
      parseIpAddress(settings.staticIp, ip) &&
      parseIpAddress(settings.gateway, gw) &&
      parseIpAddress(settings.subnet, sn) &&
      parseIpAddress(settings.dns1, d1) &&
      parseIpAddress(settings.dns2, d2)
    ) {
      WiFi.config(ip, gw, sn, d1, d2);
    }
  }

  if (settings.wifiSsid.length() > 0) {
    WiFi.begin(settings.wifiSsid.c_str(), settings.wifiPassword.c_str());
    showMessage("WiFi");

    unsigned long startAttempt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < WIFI_CONNECT_TIMEOUT_MS) {
      delay(250);
    }

    if (WiFi.status() == WL_CONNECTED) {
      showMessage("OK");
      delay(1000);
      configureTime();
      return;
    }
  }

  startSetupAccessPoint();
}
