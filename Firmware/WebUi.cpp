#include "WebUi.h"
#include "Settings.h"
#include "Timezones.h"
#include "DisplayManager.h"
#include "TimeManager.h"
#include "ClockWifi.h"

#include <WiFi.h>

WebServer server(80);

String htmlEscape(String text) {
  text.replace("&", "&amp;");
  text.replace("<", "&lt;");
  text.replace(">", "&gt;");
  text.replace("\"", "&quot;");
  text.replace("'", "&#39;");
  return text;
}

String htmlHeader() {
  String html;
  html += "<!doctype html><html><head>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>ESP32 Clock Setup</title>";
  html += "<style>";
  html += "body{font-family:Arial,Helvetica,sans-serif;margin:20px;max-width:720px;background:#f7f7f7;color:#222}";
  html += "form{background:white;padding:16px;border-radius:12px;box-shadow:0 2px 8px #0001}";
  html += "label{display:block;margin-top:12px;font-weight:bold}";
  html += "input,select{width:100%;box-sizing:border-box;padding:8px;margin-top:4px}";
  html += "button{margin-top:18px;padding:10px 16px;border:0;border-radius:8px;background:#2563eb;color:white;font-weight:bold}";
  html += ".hint{color:#666;font-size:.9em}.row{display:grid;grid-template-columns:1fr 1fr;gap:10px}";
  html += "</style></head><body>";
  html += "<h1>ESP32 Clock Setup</h1>";
  return html;
}

String htmlFooter() {
  return "</body></html>";
}

static String selected(bool value) {
  return value ? " selected" : "";
}

void handleRoot() {
  String html = htmlHeader();

  html += "<p class='hint'>Mode: ";
  html += (WiFi.getMode() == WIFI_AP) ? "Setup access point" : "Wi-Fi client";
  html += "</p>";

  if (WiFi.status() == WL_CONNECTED) {
    html += "<p class='hint'>IP address: ";
    html += WiFi.localIP().toString();
    html += "</p>";
  } else if (WiFi.getMode() == WIFI_AP) {
    html += "<p class='hint'>Setup AP IP address: ";
    html += WiFi.softAPIP().toString();
    html += "</p>";
  }

  html += "<form method='POST' action='/save'>";

  html += "<label>Wi-Fi SSID</label>";
  html += "<input name='ssid' value='" + htmlEscape(settings.wifiSsid) + "'>";

  html += "<label>Wi-Fi Password</label>";
  html += "<input name='pass' type='password' value='" + htmlEscape(settings.wifiPassword) + "'>";

  html += "<label>IP Mode</label>";
  html += "<select name='dhcp'>";
  html += "<option value='1'" + selected(settings.useDhcp) + ">DHCP</option>";
  html += "<option value='0'" + selected(!settings.useDhcp) + ">Static</option>";
  html += "</select>";

  html += "<div class='row'>";
  html += "<div><label>Static IP</label><input name='ip' value='" + htmlEscape(settings.staticIp) + "'></div>";
  html += "<div><label>Gateway</label><input name='gw' value='" + htmlEscape(settings.gateway) + "'></div>";
  html += "</div>";

  html += "<div class='row'>";
  html += "<div><label>Subnet</label><input name='subnet' value='" + htmlEscape(settings.subnet) + "'></div>";
  html += "<div><label>DNS 1</label><input name='dns1' value='" + htmlEscape(settings.dns1) + "'></div>";
  html += "</div>";

  html += "<label>DNS 2</label>";
  html += "<input name='dns2' value='" + htmlEscape(settings.dns2) + "'>";

  html += "<label>NTP Server</label>";
  html += "<input name='ntp' value='" + htmlEscape(settings.ntpServer) + "'>";

  html += "<label>Timezone</label>";
  html += "<select name='tz'>";
  for (int i = 0; i < timezoneOptionCount; i++) {
    html += "<option value='";
    html += htmlEscape(String(timezoneOptions[i].posix));
    html += "'";
    if (settings.timezoneString == timezoneOptions[i].posix) {
      html += " selected";
    }
    html += ">";
    html += htmlEscape(String(timezoneOptions[i].label));
    html += "</option>";
  }
  if (!isKnownTimezone(settings.timezoneString)) {
    html += "<option selected value='" + htmlEscape(settings.timezoneString) + "'>Custom current timezone</option>";
  }
  html += "</select>";

  html += "<label>Custom POSIX Timezone</label>";
  html += "<input name='customTz' value='" + htmlEscape(settings.timezoneString) + "'>";

  html += "<div class='row'>";
  html += "<div><label>Day brightness 0-15</label><input name='dayBright' type='number' min='0' max='15' value='" + String(settings.dayBrightness) + "'></div>";
  html += "<div><label>Night brightness 0-15</label><input name='nightBright' type='number' min='0' max='15' value='" + String(settings.nightBrightness) + "'></div>";
  html += "</div>";

  html += "<div class='row'>";
  html += "<div><label>Dim start</label><input name='dimStart' type='time' value='" + htmlEscape(settings.dimStart) + "'></div>";
  html += "<div><label>Dim end</label><input name='dimEnd' type='time' value='" + htmlEscape(settings.dimEnd) + "'></div>";
  html += "</div>";

  html += "<button type='submit'>Save and restart Wi-Fi</button>";
  html += "</form>";
  html += htmlFooter();

  server.send(200, "text/html", html);
}

void handleSave() {
  settings.wifiSsid = server.arg("ssid");
  settings.wifiPassword = server.arg("pass");

  settings.useDhcp = server.arg("dhcp") != "0";
  settings.staticIp = server.arg("ip");
  settings.gateway = server.arg("gw");
  settings.subnet = server.arg("subnet");
  settings.dns1 = server.arg("dns1");
  settings.dns2 = server.arg("dns2");

  settings.ntpServer = server.arg("ntp");

  String customTz = server.arg("customTz");
  if (customTz.length() > 0) {
    settings.timezoneString = customTz;
  } else {
    settings.timezoneString = server.arg("tz");
  }

  settings.dayBrightness = constrain(server.arg("dayBright").toInt(), 0, 15);
  settings.nightBrightness = constrain(server.arg("nightBright").toInt(), 0, 15);
  settings.dimStart = server.arg("dimStart");
  settings.dimEnd = server.arg("dimEnd");

  saveSettings();
  applyBrightness();
  configureTime();

  String html = htmlHeader();
  html += "<p>Settings saved.</p>";
  html += "<p class='hint'>Reconnect to the clock if its Wi-Fi mode or IP address changes.</p>";
  html += "<p><a href='/'>Back to setup</a></p>";
  html += htmlFooter();
  server.send(200, "text/html", html);

  delay(500);
  connectWiFi();
}

void setupWebServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.begin();
}
