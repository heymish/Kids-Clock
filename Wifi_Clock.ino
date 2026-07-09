#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <time.h>
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
#include <ESPmDNS.h>

// --------------------------------------------------
// MAX7219 display settings
// --------------------------------------------------
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4

#define DATA_PIN 15
#define CLK_PIN  7
#define CS_PIN   6

MD_Parola display = MD_Parola(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);

// --------------------------------------------------
// Web server and persistent storage
// --------------------------------------------------
WebServer server(80);
Preferences prefs;

// --------------------------------------------------
// Timezone presets
// --------------------------------------------------
struct TimezoneOption {
  const char* label;
  const char* posix;
};

TimezoneOption timezoneOptions[] = {
  {
    "New Zealand",
    "NZST-12NZDT,M9.5.0,M4.1.0/3"
  },
  {
    "Australia Sydney / Melbourne",
    "AEST-10AEDT,M10.1.0,M4.1.0/3"
  },
  {
    "Australia Brisbane",
    "AEST-10"
  },
  {
    "Australia Perth",
    "AWST-8"
  },
  {
    "United Kingdom",
    "GMT0BST,M3.5.0/1,M10.5.0"
  },
  {
    "US Eastern",
    "EST5EDT,M3.2.0,M11.1.0"
  },
  {
    "US Central",
    "CST6CDT,M3.2.0,M11.1.0"
  },
  {
    "US Mountain",
    "MST7MDT,M3.2.0,M11.1.0"
  },
  {
    "US Pacific",
    "PST8PDT,M3.2.0,M11.1.0"
  },
  {
    "UTC",
    "UTC0"
  }
};

const int timezoneOptionCount = sizeof(timezoneOptions) / sizeof(timezoneOptions[0]);

// --------------------------------------------------
// Default settings
// --------------------------------------------------
String wifiSsid     = "";
String wifiPassword = "";

bool useDhcp = true;

String staticIp = "192.168.1.80";
String gateway  = "192.168.1.1";
String subnet   = "255.255.255.0";
String dns1     = "8.8.8.8";
String dns2     = "1.1.1.1";
String dnsName = "esp32-clock";

String ntpServer = "nz.pool.ntp.org";

// New Zealand timezone with daylight saving.
// NZST UTC+12, NZDT UTC+13.
// POSIX format uses reversed signs.
String timezoneString = "NZST-12NZDT,M9.5.0,M4.1.0/3";

int dayBrightness = 8;     // 0-15
int nightBrightness = 1;   // 0-15

String dimStart = "20:00";
String dimEnd   = "07:00";

unsigned long lastDisplayUpdate = 0;
unsigned long lastNtpSyncAttempt = 0;

String lastDisplayedTime = "";

// --------------------------------------------------
// Helper functions
// --------------------------------------------------
String htmlEscape(String text) {
  text.replace("&", "&amp;");
  text.replace("<", "&lt;");
  text.replace(">", "&gt;");
  text.replace("\"", "&quot;");
  text.replace("'", "&#39;");
  return text;
}

bool parseIpAddress(const String &text, IPAddress &ip) {
  return ip.fromString(text);
}

bool isKnownTimezone(const String &tz) {
  for (int i = 0; i < timezoneOptionCount; i++) {
    if (tz == timezoneOptions[i].posix) {
      return true;
    }
  }

  return false;
}

int timeStringToMinutes(const String &hhmm) {
  if (hhmm.length() < 5) return 0;

  int hour = hhmm.substring(0, 2).toInt();
  int min  = hhmm.substring(3, 5).toInt();

  if (hour < 0) hour = 0;
  if (hour > 23) hour = 23;
  if (min < 0) min = 0;
  if (min > 59) min = 59;

  return hour * 60 + min;
}

bool isNightTime(int currentMinutes, int startMinutes, int endMinutes) {
  // Example: start 20:00, end 07:00 crosses midnight.
  if (startMinutes > endMinutes) {
    return currentMinutes >= startMinutes || currentMinutes < endMinutes;
  }

  // Example: start 09:00, end 17:00 same day.
  return currentMinutes >= startMinutes && currentMinutes < endMinutes;
}

void loadSettings() {
  prefs.begin("clock", true);

  wifiSsid     = prefs.getString("ssid", "");
  wifiPassword = prefs.getString("pass", "");

  useDhcp  = prefs.getBool("dhcp", true);
  staticIp = prefs.getString("ip", "192.168.1.80");
  gateway  = prefs.getString("gw", "192.168.1.1");
  subnet   = prefs.getString("subnet", "255.255.255.0");
  dns1     = prefs.getString("dns1", "8.8.8.8");
  dns2     = prefs.getString("dns2", "1.1.1.1");
  dnsName  = prefs.getString("hostname", "esp32-clock");

  ntpServer = prefs.getString("ntp", "nz.pool.ntp.org");
  timezoneString = prefs.getString("tz", "NZST-12NZDT,M9.5.0,M4.1.0/3");

  dayBrightness   = prefs.getInt("dayBright", 8);
  nightBrightness = prefs.getInt("nightBright", 1);

  dimStart = prefs.getString("dimStart", "20:00");
  dimEnd   = prefs.getString("dimEnd", "07:00");

  prefs.end();

  dayBrightness = constrain(dayBrightness, 0, 15);
  nightBrightness = constrain(nightBrightness, 0, 15);
}

void saveSettings() {
  prefs.begin("clock", false);

  prefs.putString("ssid", wifiSsid);
  prefs.putString("pass", wifiPassword);

  prefs.putBool("dhcp", useDhcp);
  prefs.putString("ip", staticIp);
  prefs.putString("gw", gateway);
  prefs.putString("subnet", subnet);
  prefs.putString("dns1", dns1);
  prefs.putString("dns2", dns2);
  prefs.putString("hostname", dnsName);

  prefs.putString("ntp", ntpServer);
  prefs.putString("tz", timezoneString);

  prefs.putInt("dayBright", dayBrightness);
  prefs.putInt("nightBright", nightBrightness);

  prefs.putString("dimStart", dimStart);
  prefs.putString("dimEnd", dimEnd);

  prefs.end();
}

void showMessage(const char *message) {
  display.displayClear();
  display.setTextAlignment(PA_CENTER);
  display.print(message);
}

void configureTime() {
  Serial.println("Configure time ...");
  Serial.println("NTP server: ");
  Serial.println(ntpServer);
  Serial.println("Timezone string: ");
  Serial.println(timezoneString);

 //Apply timezone first
 setenv("TZ", timezoneString.c_str(), 1);
 tzset();

 //Configure NTP in UTC
 configTime(0,0, ntpServer.c_str());

 //Wait breifly for time sync
 struct tm timeinfo;
 if (getLocalTime(&timeinfo, 10000)){
Serial.println("Time synced successfully");
Serial.println(&timeinfo, "%Y-%m-%d %H:%M:%S %Z");
 }
 else
 {
  Serial.println("Failed to sync time");
 }

  //configTzTime(timezoneString.c_str(), ntpServer.c_str());
}

bool timeIsValid() {
  struct tm timeinfo;
  return getLocalTime(&timeinfo, 1000);
}

void applyTimezone(){
  setenv("TZ", timezoneString.c_str(), 1);
  tzset();
}

// --------------------------------------------------
// Wi-Fi connection
// --------------------------------------------------
void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(dnsName.c_str());
  MDNS.begin(dnsName.c_str());
  
  Serial.println();
  Serial.println("============ connectWifi() Called ============");

  if (!useDhcp) {
    Serial.println("Using Static IP Configuration");

    IPAddress ip;
    IPAddress gw;
    IPAddress sn;
    IPAddress d1;
    IPAddress d2;

    if (
      parseIpAddress(staticIp, ip) &&
      parseIpAddress(gateway, gw) &&
      parseIpAddress(subnet, sn) &&
      parseIpAddress(dns1, d1) &&
      parseIpAddress(dns2, d2)
    ) {
      WiFi.config(ip, gw, sn, d1, d2);
    }
  }

  if (wifiSsid.length() > 0) {
    WiFi.begin(wifiSsid.c_str(), wifiPassword.c_str());

    showMessage("WiFi");

    unsigned long startAttempt = millis();

    while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 20000) {
      delay(250);
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    showMessage("OK");
    delay(1000);
    configureTime();
  } else {
    // Setup AP if Wi-Fi is not configured or connection fails.
    Serial.println("Wi-fi connection failed. Starting setup AP");
    WiFi.mode(WIFI_AP);
    WiFi.softAP("ESP32-Clock-Setup", "clock1234");

    showMessage("SETUP");
  }
}

// --------------------------------------------------
// Web UI
// --------------------------------------------------
String htmlHeader() {
  String html = "";
  html += "<!doctype html><html><head>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>ESP32 Clock Setup</title>";
  html += "<style>";
  html += "body{font-family:Arial,Helvetica,sans-serif;margin:24px;background:#f5f5f5;color:#222;}";
  html += ".card{max-width:760px;margin:auto;background:white;padding:24px;border-radius:16px;box-shadow:0 4px 16px rgba(0,0,0,.08);}";
  html += "h1{margin-top:0;}";
  html += "label{display:block;margin-top:14px;font-weight:bold;}";
  html += "input,select{width:100%;padding:10px;margin-top:6px;box-sizing:border-box;border:1px solid #ccc;border-radius:8px;}";
  html += "button{margin-top:22px;padding:12px 18px;border:0;border-radius:10px;background:#2563eb;color:white;font-size:16px;cursor:pointer;}";
  html += ".hint{font-size:13px;color:#555;}";
  html += ".status{background:#eef2ff;padding:12px;border-radius:10px;margin-bottom:18px;line-height:1.5;}";
  html += ".section{border-top:1px solid #e5e7eb;margin-top:24px;padding-top:16px;}";
  html += ".danger{background:#555;}";
  html += "</style>";
  html += "</head><body><div class='card'>";
  return html;
}

String htmlFooter() {
  return "</div></body></html>";
}

void handleRoot() {
  String ipText;

  if (WiFi.getMode() == WIFI_AP) {
    ipText = WiFi.softAPIP().toString();
  } else {
    ipText = WiFi.localIP().toString();
  }

  String html = htmlHeader();

  html += "<h1>ESP32 Clock Setup</h1>";

  html += "<div class='status'>";
  html += "<strong>Current IP:</strong> " + ipText + "<br>";

  html += "<strong>Wi-Fi mode:</strong> ";
  if (WiFi.getMode() == WIFI_AP) {
    html += "Setup Access Point";
  } else {
    html += "Station";
  }

  html += "<br><strong>NTP server:</strong> " + htmlEscape(ntpServer);
  html += "<br><strong>Timezone:</strong> " + htmlEscape(timezoneString);
  html += "</div>";

  html += "<form method='POST' action='/save'>";

  html += "<div class='section'>";
  html += "<h2>Wi-Fi</h2>";

  html += "<label>Wi-Fi SSID</label>";
  html += "<input name='ssid' value='" + htmlEscape(wifiSsid) + "'>";

  html += "<label>Wi-Fi Password</label>";
  html += "<input name='pass' type='password' value='" + htmlEscape(wifiPassword) + "'>";
  html += "</div>";

  html += "<div class='section'>";
  html += "<h2>Network</h2>";

  html += "<label>IP Mode</label>";
  html += "<select name='dhcp'>";

  html += "<option value='1'";
  if (useDhcp) html += " selected";
  html += ">DHCP</option>";

  html += "<option value='0'";
  if (!useDhcp) html += " selected";
  html += ">Static IP</option>";

  html += "</select>";

  html += "<label>Static IP Address</label>";
  html += "<input name='ip' value='" + htmlEscape(staticIp) + "'>";

  html += "<label>Gateway</label>";
  html += "<input name='gw' value='" + htmlEscape(gateway) + "'>";

  html += "<label>Subnet Mask</label>";
  html += "<input name='subnet' value='" + htmlEscape(subnet) + "'>";

  html += "<label>DNS 1</label>";
  html += "<input name='dns1' value='" + htmlEscape(dns1) + "'>";

  html += "<label>DNS 2</label>";
  html += "<input name='dns2' value='" + htmlEscape(dns2) + "'>";
  html += "</div>";

  html += "<label>Device Hostname</label>";
  html += "<input name='hostname' value='" + htmlEscape(dnsName) + "'>";
  html += "<p class='hint'>Example: esp32-clock. Device may be reachable as esp32-clock.local</p>";

  html += "<br><strong>Hostname:</strong> " + htmlEscape(dnsName);

  html += "<div class='section'>";
  html += "<h2>Time</h2>";

  html += "<label>NTP Server</label>";
  html += "<input name='ntp' value='" + htmlEscape(ntpServer) + "'>";
  html += "<p class='hint'>Examples: nz.pool.ntp.org, pool.ntp.org, time.google.com</p>";

  html += "<label>Timezone</label>";
  html += "<select name='tzPreset'>";

  for (int i = 0; i < timezoneOptionCount; i++) {
    html += "<option value='";
    html += timezoneOptions[i].posix;
    html += "'";

    if (timezoneString == timezoneOptions[i].posix) {
      html += " selected";
    }

    html += ">";
    html += htmlEscape(String(timezoneOptions[i].label));
    html += "</option>";
  }

  html += "<option value='custom'";
  if (!isKnownTimezone(timezoneString)) {
    html += " selected";
  }
  html += ">Custom POSIX timezone</option>";

  html += "</select>";

  html += "<label>Custom POSIX Timezone</label>";
  html += "<input name='tzCustom' value='" + htmlEscape(timezoneString) + "'>";

  html += "<p class='hint'>";
  html += "Used only if Custom POSIX timezone is selected. ";
  html += "For New Zealand use: NZST-12NZDT,M9.5.0,M4.1.0/3. ";
  html += "Daylight saving is handled automatically when the timezone rule includes DST.";
  html += "</p>";
  html += "</div>";

  html += "<div class='section'>";
  html += "<h2>Brightness</h2>";

  html += "<label>Day Brightness, 0-15</label>";
  html += "<input name='dayBright' type='number' min='0' max='15' value='" + String(dayBrightness) + "'>";

  html += "<label>Night Brightness, 0-15</label>";
  html += "<input name='nightBright' type='number' min='0' max='15' value='" + String(nightBrightness) + "'>";

  html += "<label>Dim Start Time</label>";
  html += "<input name='dimStart' type='time' value='" + htmlEscape(dimStart) + "'>";

  html += "<label>Dim End Time</label>";
  html += "<input name='dimEnd' type='time' value='" + htmlEscape(dimEnd) + "'>";

  html += "<button type='submit'>Save and Reboot</button>";
  html += "</div>";

  html += "</form>";

  html += "<form method='POST' action='/reboot'>";
  html += "<button type='submit' class='danger'>Reboot Without Saving</button>";
  html += "</form>";

  html += htmlFooter();

  server.send(200, "text/html", html);
}

void handleSave() {
  wifiSsid     = server.arg("ssid");
  wifiPassword = server.arg("pass");

  useDhcp = server.arg("dhcp") == "1";

  staticIp = server.arg("ip");
  gateway  = server.arg("gw");
  subnet   = server.arg("subnet");
  dns1     = server.arg("dns1");
  dns2     = server.arg("dns2");

  dnsName = server.arg("hostname");

  if (dnsName.length() == 0)
  {
    dnsName = "esp32-clock";
  }

  ntpServer = server.arg("ntp");

  String tzPreset = server.arg("tzPreset");
  String tzCustom = server.arg("tzCustom");

  if (tzPreset == "custom") {
    timezoneString = tzCustom;
  } else {
    timezoneString = tzPreset;
  }

  if (timezoneString.length() == 0) {
    timezoneString = "NZST-12NZDT,M9.5.0,M4.1.0/3";
  }

  dayBrightness = server.arg("dayBright").toInt();
  nightBrightness = server.arg("nightBright").toInt();

  dayBrightness = constrain(dayBrightness, 0, 15);
  nightBrightness = constrain(nightBrightness, 0, 15);

  dimStart = server.arg("dimStart");
  dimEnd   = server.arg("dimEnd");

  if (dimStart.length() == 0) {
    dimStart = "20:00";
  }

  if (dimEnd.length() == 0) {
    dimEnd = "07:00";
  }

  saveSettings();

  String html = htmlHeader();
  html += "<h1>Settings saved</h1>";
  html += "<p>The clock will reboot now.</p>";
  html += htmlFooter();

  server.send(200, "text/html", html);

  delay(1000);
  ESP.restart();
}

void handleReboot() {
  server.send(200, "text/plain", "Rebooting...");
  delay(1000);
  ESP.restart();
}

void startWebServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/reboot", HTTP_POST, handleReboot);
  server.begin();
}

// --------------------------------------------------
// Clock display
// --------------------------------------------------
void updateDisplay() {
  applyTimezone();

  struct tm timeinfo;

  if (!getLocalTime(&timeinfo, 100)) {
    display.setIntensity(nightBrightness);
    showMessage("--:--");
    return;
  }

  int currentMinutes = timeinfo.tm_hour * 60 + timeinfo.tm_min;
  int startMinutes = timeStringToMinutes(dimStart);
  int endMinutes   = timeStringToMinutes(dimEnd);

  bool night = isNightTime(currentMinutes, startMinutes, endMinutes);

  if (night) {
    display.setIntensity(nightBrightness);
  } else {
    display.setIntensity(dayBrightness);
  }

  char timeText[9];
  snprintf(timeText, sizeof(timeText), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);

  String currentText = String(timeText);

  if (currentText != lastDisplayedTime) {
    lastDisplayedTime = currentText;
    display.displayClear();
    display.setTextAlignment(PA_CENTER);
    display.print(timeText);
  }
}

// --------------------------------------------------
// Setup and loop
// --------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println("Booting ESP32 Clock ...");


  display.begin();
  display.setIntensity(dayBrightness);
  display.displayClear();
  display.setTextAlignment(PA_CENTER);
  display.print("BOOT");

  loadSettings();

  Serial.println("Settings loaded:");
  Serial.print("SSID: ");
  Serial.println(wifiSsid);
  Serial.print("NTP: ");
  Serial.println(ntpServer);
  Serial.print("TZ: ");
  Serial.println(timezoneString);

  display.setIntensity(dayBrightness);

  connectWiFi();
  startWebServer();

  if (WiFi.status() == WL_CONNECTED) {
    configureTime();
  }
}

void loop() {
  server.handleClient();

  if (millis() - lastDisplayUpdate >= 1000) {
    lastDisplayUpdate = millis();
    updateDisplay();
  }

  // Retry NTP setup periodically if Wi-Fi is connected and time is not valid.
  if (
    WiFi.status() == WL_CONNECTED &&
    millis() - lastNtpSyncAttempt >= 300000
  ) {
    lastNtpSyncAttempt = millis();

    if (!timeIsValid()) {
      configureTime();
    }
  }
}
