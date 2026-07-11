/*
  Kids Clock - ESP32 + MAX7219
  File: wifi_clock.ino

  Changes included:
Fix first-boot setup AP when SSID is empty
Add wi-fi reconnect logicc after boot
Validate web form  input before saving 
Check write results for preferences and report failures
Track NTP Validity
Add second NTP server incase first does not work
Split static HTML out of main file
Fix WebUI layout
Add custom timezone input

  Board: ESP32-C6
  Display: MAX7219 4-module FC16 matrix
*/

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <time.h>
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
#include <ArduinoOTA.h>

// --------------------------------------------------
// MAX7219 display settings
// --------------------------------------------------
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4
#define DATA_PIN 15
#define CLK_PIN 7
#define CS_PIN 6

MD_Parola display = MD_Parola(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);

// --------------------------------------------------
// Web server, DNS server and persistent storage
// --------------------------------------------------
WebServer server(80);
DNSServer dnsServer;
Preferences prefs;

const byte DNS_PORT = 53;
const IPAddress AP_IP(192, 168, 4, 1);
const IPAddress AP_GATEWAY(192, 168, 4, 1);
const IPAddress AP_SUBNET(255, 255, 255, 0);

bool setupMode = false;
String setupApName = "";

// --------------------------------------------------
// Timezone presets
// POSIX timezone signs are intentionally reversed.
// NZST-12 means UTC+12.
// --------------------------------------------------
struct TimezoneOption
{
  const char *label;
  const char *posix;
};

TimezoneOption timezoneOptions[] = {
    {"New Zealand", "NZST-12NZDT,M9.5.0,M4.1.0/3"},
    {"Australia Sydney / Melbourne", "AEST-10AEDT,M10.1.0,M4.1.0/3"},
    {"Australia Brisbane", "AEST-10"},
    {"Australia Perth", "AWST-8"},
    {"United Kingdom", "GMT0BST,M3.5.0/1,M10.5.0"},
    {"US Eastern", "EST5EDT,M3.2.0,M11.1.0"},
    {"US Central", "CST6CDT,M3.2.0,M11.1.0"},
    {"US Mountain", "MST7MDT,M3.2.0,M11.1.0"},
    {"US Pacific", "PST8PDT,M3.2.0,M11.1.0"},
    {"UTC", "UTC0"}};

const int timezoneOptionCount = sizeof(timezoneOptions) / sizeof(timezoneOptions[0]);

// --------------------------------------------------
// Default settings
// --------------------------------------------------
String wifiSsid = "";
String wifiPassword = "";
bool useDhcp = true;
String staticIp = "192.168.1.80";
String gateway = "192.168.1.1";
String subnet = "255.255.255.0";
String dns1 = "8.8.8.8";
String dns2 = "1.1.1.1";
String ntpServer = "nz.pool.ntp.org";
String timezoneString = "NZST-12NZDT,M9.5.0,M4.1.0/3";
int dayBrightness = 8;      // 0-15
int nightBrightness = 1;    // 0-15
String dimStart = "20:00";
String dimEnd = "07:00";
bool use24HourClock = true;
String hostname = "kidsclock";

unsigned long lastDisplayUpdate = 0;
unsigned long lastNtpSyncAttempt = 0;
String lastDisplayedTime = "";


// --------------------------------------------------
// OTA settings
// --------------------------------------------------
String otaHostname = "kids-clock";
String otaPassword = "clockota";
bool otaReady = false;


// --------------------------------------------------
// Helper functions
// --------------------------------------------------
String htmlEscape(String text)
{
  text.replace("&", "&amp;");
  text.replace("<", "&lt;");
  text.replace(">", "&gt;");
  text.replace("\"", "&quot;");
  text.replace("'", "&#39;");
  return text;
}

bool parseIpAddress(const String &text, IPAddress &ip)
{
  return ip.fromString(text);
}

bool isKnownTimezone(const String &tz)
{
  for (int i = 0; i < timezoneOptionCount; i++)
  {
    if (tz == timezoneOptions[i].posix)
    {
      return true;
    }
  }
  return false;
}

int timeStringToMinutes(const String &hhmm)
{
  if (hhmm.length() < 5)
    return 0;

  int hour = hhmm.substring(0, 2).toInt();
  int min = hhmm.substring(3, 5).toInt();

  hour = constrain(hour, 0, 23);
  min = constrain(min, 0, 59);

  return hour * 60 + min;
}

bool isNightTime(int currentMinutes, int startMinutes, int endMinutes)
{
  if (startMinutes > endMinutes)
  {
    return currentMinutes >= startMinutes || currentMinutes < endMinutes;
  }
  return currentMinutes >= startMinutes && currentMinutes < endMinutes;
}

String makeSetupApName()
{
  uint64_t mac = ESP.getEfuseMac();
  char suffix[5];
  snprintf(suffix, sizeof(suffix), "%04X", (uint16_t)(mac & 0xFFFF));
  return "KidsClock-" + String(suffix);
}

void showMessage(const char *message)
{
  display.displayClear();
  display.setTextAlignment(PA_CENTER);
  display.print(message);
}

void configureTime()
{
  configTzTime(timezoneString.c_str(), ntpServer.c_str());
  lastNtpSyncAttempt = millis();
}

bool timeIsValid()
{
  struct tm timeinfo;
  return getLocalTime(&timeinfo, 1000);
}

String currentIpText()
{
  if (setupMode)
  {
    return WiFi.softAPIP().toString();
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    return WiFi.localIP().toString();
  }

  return "Not connected";
}

//ToDO: Need to use this code.
//This is very much a basic check
bool isValidPOSIX_TZ(const char *tz) {
    if (!tz || strlen(tz) < 3) return false;

    // 1. Standard Time Name (e.g., EST or GMT)
    const char *p = tz;
    while (*p && isalpha(*p)) p++;
    if (p == tz) return false; // No letters found

    // 2. Standard Time Offset (e.g., 5, -5, +01:30)
    if (*p == '+' || *p == '-') p++;
    if (!isdigit(*p)) return false;
    while (*p && (isdigit(*p) || *p == ':')) p++;

    // If string ends here, it's a valid POSIX TZ without DST (e.g., GMT0, JST-9)
    if (*p == '\0') return true; 

    // 3. Daylight Saving Time Name (e.g., EDT, DST)
    if (*p == ',') p++;
    else if (!isalpha(*p)) return false; // Next part should be DST name or transition
    
    while (*p && isalpha(*p)) p++;
    if (*p == '\0') return false; // Found DST name but no offset

    // 4. Daylight Saving Time Offset (Optional)
    if (*p == '+' || *p == '-') p++;
    while (*p && (isdigit(*p) || *p == ':')) p++;

    // If string ends here, DST offset defaults to 1 hour ahead
    if (*p == '\0') return true; 

    // 5. Rule Transitions (e.g., ,M3.2.0/2:00,M11.1.0/2:00)
    if (*p != ',') return false; 
    
    // Minimal check for M-rule (M<month>.<week>.<day>/<time>)
    // For a strict compliance check, you would parse and validate the numeric boundaries (e.g., M1..12)
    return true; 
}

void setupOTA() {
  if (WiFi.status() != WL_CONNECTED) {
    otaReady = false;
    return;
  }

  ArduinoOTA.setHostname(otaHostname.c_str());
  ArduinoOTA.setPassword(otaPassword.c_str());

  ArduinoOTA
    .onStart([]() {
      display.displayClear();
      display.setTextAlignment(PA_CENTER);
      display.print("OTA");
    })
    .onEnd([] () {
      display.displayClear();
      display.setTextAlignment(PA_CENTER);
      display.print("DONE");
    })
    .onProgress([] (unsigned int progress, unsigned int total) {
      int percent = (progress * 100) / total;
      display.displayClear();
      display.setTextAlignment(PA_CENTER);
      display.print(String(percent) + "%");
    })
    .onError([] (ota_error_t error) {
      display.displayClear();
      display.setTextAlignment(PA_CENTER);
      display.print("ERR");
    });

  ArduinoOTA.begin();
  otaReady = true;
}


// --------------------------------------------------
// Persistent settings
// --------------------------------------------------
void loadSettings()
{
  prefs.begin("clock", true);
  wifiSsid = prefs.getString("ssid", "");
  wifiPassword = prefs.getString("pass", "");
  useDhcp = prefs.getBool("dhcp", true);
  staticIp = prefs.getString("ip", "192.168.1.80");
  gateway = prefs.getString("gw", "192.168.1.1");
  subnet = prefs.getString("subnet", "255.255.255.0");
  dns1 = prefs.getString("dns1", "8.8.8.8");
  dns2 = prefs.getString("dns2", "1.1.1.1");
  ntpServer = prefs.getString("ntp", "nz.pool.ntp.org");
  timezoneString = prefs.getString("tz", "NZST-12NZDT,M9.5.0,M4.1.0/3");
  dayBrightness = prefs.getInt("dayBright", 8);
  nightBrightness = prefs.getInt("nightBright", 1);
  dimStart = prefs.getString("dimStart", "20:00");
  dimEnd = prefs.getString("dimEnd", "07:00");
  use24HourClock = prefs.getBool("24hour", true);
  hostname = prefs.getString("hostname", "kidsclock");
  prefs.end();

  dayBrightness = constrain(dayBrightness, 0, 15);
  nightBrightness = constrain(nightBrightness, 0, 15);
}

void saveSettings()
{
  prefs.begin("clock", false);
  prefs.putString("ssid", wifiSsid);
  prefs.putString("pass", wifiPassword);
  prefs.putBool("dhcp", useDhcp);
  prefs.putString("ip", staticIp);
  prefs.putString("gw", gateway);
  prefs.putString("subnet", subnet);
  prefs.putString("dns1", dns1);
  prefs.putString("dns2", dns2);
  prefs.putString("ntp", ntpServer);
  prefs.putString("tz", timezoneString);
  prefs.putInt("dayBright", dayBrightness);
  prefs.putInt("nightBright", nightBrightness);
  prefs.putString("dimStart", dimStart);
  prefs.putString("dimEnd", dimEnd);
  prefs.putBool("24hour", use24HourClock);
  prefs.putString("hostname", hostname);
  prefs.end();
}

void clearWifiSettings()
{
  prefs.begin("clock", false);
  prefs.remove("ssid");
  prefs.remove("pass");
  prefs.remove("dhcp");
  prefs.remove("ip");
  prefs.remove("gw");
  prefs.remove("subnet");
  prefs.remove("dns1");
  prefs.remove("dns2");
  prefs.end();

  wifiSsid = "";
  wifiPassword = "";
  useDhcp = true;
  staticIp = "192.168.1.80";
  gateway = "192.168.1.1";
  subnet = "255.255.255.0";
  dns1 = "8.8.8.8";
  dns2 = "1.1.1.1";
}

// --------------------------------------------------
// Wi-Fi and captive portal
// --------------------------------------------------
void startSetupAccessPoint()
{
  setupMode = true;
  setupApName = makeSetupApName();

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(AP_IP, AP_GATEWAY, AP_SUBNET);

  // Open AP by design: no hard-coded setup password.
  // The dynamic SSID reduces clashes if multiple clocks are nearby.
  WiFi.softAP(setupApName.c_str());

  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(DNS_PORT, "*", AP_IP);

  showMessage("SETUP");

  Serial.println("Setup captive portal started");
  Serial.print("AP SSID: ");
  Serial.println(setupApName);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());
}

bool connectWiFiStation()
{
  Serial.println("connectionWiFIStation() Called");
  if (wifiSsid.length() == 0)
  {
    Serial.println("No SSID configured");
    return false;
  }

  if (wifiPassword.length() == 0)
  {
    Serial.println("No Passowrd configured");
    return false;
  }

  Serial.print("Trying SSID:");
  Serial.println(wifiSsid);
  WiFi.mode(WIFI_STA);

  if (!useDhcp)
  {
    IPAddress ip;
    IPAddress gw;
    IPAddress sn;
    IPAddress d1;
    IPAddress d2;

    if (parseIpAddress(staticIp, ip) &&
        parseIpAddress(gateway, gw) &&
        parseIpAddress(subnet, sn) &&
        parseIpAddress(dns1, d1) &&
        parseIpAddress(dns2, d2))
    {
      WiFi.config(ip, gw, sn, d1, d2);
    }
    else
    {
      Serial.println("Invalid static IP settings; falling back to DHCP for this boot.");
    }
  }
  WiFi.setHostname(hostname);
  showMessage("WiFi");
  WiFi.begin(wifiSsid.c_str(), wifiPassword.c_str());

  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 20000)
  {
    delay(250);
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    setupMode = false;
    showMessage("OK");
    delay(750);
    configureTime();
    return true;
  }

  return false;
}

void connectWiFi()
{
  Serial.println("connectionWifi()");
  if (!connectWiFiStation())
  {
    Serial.println("Starting AP becasue Wifi connection failed");
    startSetupAccessPoint();
  }
  else
  {
    Serial.println("Wifi connected successfully");
  }
}

void redirectToPortal()
{
  String url = "http://" + WiFi.softAPIP().toString() + "/";
  server.sendHeader("Location", url, true);
  server.send(302, "text/plain", "Redirecting to Kids Clock setup");
}

// --------------------------------------------------
// Web UI
// --------------------------------------------------
String htmlHeader()
{
  String html = "";
  html += "<!doctype html><html><head>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>Kids Clock Setup</title>";
  html += "<style>";
  html += "body {font-family:Arial,Helvetica,sans-serif;margin:24px;background:#f5f5f5;color:#222;}";
  html += ".card {max-width:760px;margin:auto;background:white;padding:24px;border-radius:16px;box-shadow:0 4px 16px rgba(0,0,0,.08);}";
  html += "h1 {margin-top:0;}";
  html += "label {display:block;margin-top:14px;font-weight:bold;}";
  html += "input,select {width:100%;padding:10px;margin-top:6px;box-sizing:border-box;border:1px solid #ccc;border-radius:8px;}";
  html += "button {margin-top:22px;padding:12px 18px;border:0;border-radius:10px;background:#2563eb;color:white;font-size:16px;cursor:pointer;}";
  html += ".danger {background:#b91c1c;}";
  html += ".muted {background:#555;}";
  html += ".hint {font-size:13px;color:#555;}";
  html += ".status {background:#eef2ff;padding:12px;border-radius:10px;margin-bottom:18px;line-height:1.5;}";
  html += ".warning {background:#fff7ed;padding:12px;border-radius:10px;border:1px solid #fed7aa;}";
  html += ".section {border-top:1px solid #e5e7eb;margin-top:24px;padding-top:16px;}";
  html += ".row {display:grid;grid-template-columns:1fr 1fr;gap:12px;}";
  html += "@media (max-width: 640px) {.row {grid-template-columns:1fr;}}";
  html += "</style>";
  html += "</head><body><div class='card'>";
  return html;
}

String htmlFooter()
{
  return "</div></body></html>";
}

String selected(bool value)
{
  return value ? " selected" : "";
}

String checked(bool value)
{
  return value ? " checked" : "";
}

void handleRoot()
{
  String html = htmlHeader();

  html += "<h1>Kids Clock Setup</h1>";
  html += "<div class='status'>";
  html += "<b>Mode:</b> ";
  html += setupMode ? "Setup captive portal" : "Normal clock";
  html += "<br><b>IP address:</b> " + htmlEscape(currentIpText());

  if (setupMode)
  {
    html += "<br><b>Setup Wi-Fi:</b> " + htmlEscape(setupApName);
    html += "<br><span class='hint'>Join this Wi-Fi network, then open any web page. The clock should redirect you here automatically.</span>";
  }
  else
  {
    html += "<br><b>Connected SSID:</b> " + htmlEscape(WiFi.SSID());
  }

  html += "</div>";

  if (setupMode)
  {
    html += "<div class='warning'><b>Setup mode is open Wi-Fi.</b> Configure your home Wi-Fi, save, and the clock will restart into normal mode.</div>";
  }

  html += "<form method='POST' action='/save'>";

  html += "<div class='section'><h2>Wi-Fi</h2>";
  html += "<label>SSID</label>";
  html += "<input name='ssid' value='" + htmlEscape(wifiSsid) + "' maxlength='64'>";
  html += "<label>Password</label>";
  html += "<input name='pass' type='password' placeholder='Leave blank to keep existing password'>";
  html += "<p class='hint'>Saved passwords are not displayed. Enter a new password only if you want to change it.</p>";

  html += "<label>IP mode</label>";
  html += "<select name='dhcp'>";
  html += "<option value='1'" + selected(useDhcp) + ">DHCP</option>";
  html += "<option value='0'" + selected(!useDhcp) + ">Static IP</option>";
  html += "</select>";

  html += "<div class='row'>";
  html += "<div><label>Static IP</label><input name='ip' value='" + htmlEscape(staticIp) + "'></div>";
  html += "<div><label>Gateway</label><input name='gw' value='" + htmlEscape(gateway) + "'></div>";
  html += "<div><label>Subnet</label><input name='subnet' value='" + htmlEscape(subnet) + "'></div>";
  html += "<div><label>DNS 1</label><input name='dns1' value='" + htmlEscape(dns1) + "'></div>";
  html += "<div><label>DNS 2</label><input name='dns2' value='" + htmlEscape(dns2) + "'></div>";
  html += "<div><label>Hostname</label><input name='hostname' value='" + htmlEscape(hostname) + "'></div>";
  html += "</div></div>";

  html += "<div class='section'><h2>Time</h2>";
  html += "<label>NTP server</label>";
  html += "<input name='ntp' value='" + htmlEscape(ntpServer) + "'>";

  html += "<label>Timezone</label>";
  html += "<select name='tz'>";
  for (int i = 0; i < timezoneOptionCount; i++)
  {
    html += "<option value='" + htmlEscape(String(timezoneOptions[i].posix)) + "'";
    if (timezoneString == timezoneOptions[i].posix)
    {
      html += " selected";
    }
    html += ">" + htmlEscape(String(timezoneOptions[i].label)) + "</option>";
  }

  if (!isKnownTimezone(timezoneString))
  {
    html += "<option value='" + htmlEscape(timezoneString) + "' selected>Custom: " + htmlEscape(timezoneString) + "</option>";
  }
  html += "</select>";
  html += "</div>";

  html += "<div class='section'><h2>Brightness</h2>";
  html += "<div class='row'>";
  html += "<div><label>Day brightness 0-15</label><input name='dayBright' type='number' min='0' max='15' value='" + String(dayBrightness) + "'></div>";
  html += "<div><label>Night brightness 0-15</label><input name='nightBright' type='number' min='0' max='15' value='" + String(nightBrightness) + "'></div>";
  html += "<div><label>Dim start</label><input name='dimStart' type='time' value='" + htmlEscape(dimStart) + "'></div>";
  html += "<div><label>Dim end</label><input name='dimEnd' type='time' value='" + htmlEscape(dimEnd) + "'></div>";
  
html += "<div class='status'>";
html += "IP: " + htmlEscape(currentIpText()) + "<br>";
html += "OTA: ";
html += otaReady ? "Enabled" : "Not available";
html += "<br>OTA Hostname: " + htmlEscape(otaHostname);
html += "</div>";
	
  html += "<label for='clockFormat'>Clock display</label>";
  html += "<select id='clockFormat' name='clockFormat'>";
  html += "<option value='24'";
  if (use24HourClock) html += " selected";
     html += ">24 hour, e.g. 21:05</option>";

   html += "<option value='12'";
  if (!use24HourClock) html += " selected";
    html += ">12 hour, e.g. 9:05 PM</option>";
  html += "</select>";

  html += "</div></div>";

  html += "<button type='submit'>Save and Restart</button>";
  html += "</form>";

  html += "<div class='section'><h2>Maintenance</h2>";
  html += "<form method='POST' action='/reset-wifi' onsubmit=\"return confirm('Reset Wi-Fi settings and restart into setup mode?');\">";
  html += "<button class='danger' type='submit'>Reset Wi-Fi</button>";
  html += "</form>";
  html += "<p class='hint'>This clears only Wi-Fi and IP settings. Timezone and brightness settings are kept.</p>";
  html += "</div>";

  html += htmlFooter();
  server.send(200, "text/html", html);
}

String getArgOrCurrent(const char *name, const String &currentValue)
{
  if (server.hasArg(name))
  {
    return server.arg(name);
  }
  return currentValue;
}

void handleSave()
{
  if (server.arg("ssid").length() > 0)
  {
   wifiSsid = getArgOrCurrent("ssid", wifiSsid);
  }
  else
  {
    server.send(400, "text/html", "SSID cannot be empty");
    return; 
  }

  // Keep the existing stored password if the password field is left blank.
  if (server.hasArg("pass") && server.arg("pass").length() > 0)
  {
    wifiPassword = server.arg("pass");
  }

  useDhcp = server.arg("dhcp") == "1";

  if (!useDhcp) {
  IPAddress ip, gw, sn, d1, d2;
  if (!parseIpAddress(staticIp, ip) || !parseIpAddress(gateway, gw) || 
      !parseIpAddress(subnet, sn) || !parseIpAddress(dns1, d1) || 
      !parseIpAddress(dns2, d2)) {
    server.send(400, "text/html", "Invalid IP settings");
    return;
  }
}

 
  staticIp = getArgOrCurrent("ip", staticIp);
  gateway = getArgOrCurrent("gw", gateway);
  subnet = getArgOrCurrent("subnet", subnet);
  dns1 = getArgOrCurrent("dns1", dns1);
  dns2 = getArgOrCurrent("dns2", dns2);
  ntpServer = getArgOrCurrent("ntp", ntpServer);
  timezoneString = getArgOrCurrent("tz", timezoneString);
  dayBrightness = constrain(getArgOrCurrent("dayBright", String(dayBrightness)).toInt(), 0, 15);
  nightBrightness = constrain(getArgOrCurrent("nightBright", String(nightBrightness)).toInt(), 0, 15);
  dimStart = getArgOrCurrent("dimStart", dimStart);
  dimEnd = getArgOrCurrent("dimEnd", dimEnd);
  use24HourClock = server.arg("clockFormat") == "24";
  hostname = getArgOrCurrent("hostname", hostname);
	

  saveSettings();

  String html = htmlHeader();
  html += "<h1>Saved</h1>";
  html += "<p>Settings saved. The clock is restarting now.</p>";
  html += htmlFooter();
  server.send(200, "text/html", html);

  delay(700);
  ESP.restart();
}

void handleResetWifi()
{
  clearWifiSettings();

  String html = htmlHeader();
  html += "<h1>Wi-Fi Reset</h1>";
  html += "<p>Wi-Fi settings were cleared. The clock is restarting into setup mode.</p>";
  html += htmlFooter();
  server.send(200, "text/html", html);

  delay(700);
  ESP.restart();
}

void handleCaptiveProbe()
{
  if (setupMode)
  {
    redirectToPortal();
  }
  else
  {
    server.send(204, "text/plain", "");
  }
}

void setupWebServer()
{
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/reset-wifi", HTTP_POST, handleResetWifi);

  // Common captive portal detection endpoints used by phones and operating systems.
  server.on("/generate_204", HTTP_GET, handleCaptiveProbe);          // Android
  server.on("/gen_204", HTTP_GET, handleCaptiveProbe);               // Android variants
  server.on("/hotspot-detect.html", HTTP_GET, redirectToPortal);      // Apple
  server.on("/library/test/success.html", HTTP_GET, redirectToPortal); // Apple variants
  server.on("/ncsi.txt", HTTP_GET, redirectToPortal);                 // Windows
  server.on("/connecttest.txt", HTTP_GET, redirectToPortal);          // Windows
  server.on("/fwlink", HTTP_GET, redirectToPortal);                   // Windows

  server.onNotFound([]()
	{
	  if (setupMode)
	  {
		redirectToPortal();
	  }
	  else
	  {
		server.send(404, "text/plain", "Not found");
	  }
	});

  server.begin();
}

// --------------------------------------------------
// Clock display
// --------------------------------------------------
void updateDisplay()
{
  if (millis() - lastDisplayUpdate < 500)
  {
    return;
  }
  lastDisplayUpdate = millis();

  if (setupMode)
  {
    // Keep setup text simple on a 4-module display.
    if (lastDisplayedTime != "SETUP")
    {
      lastDisplayedTime = "SETUP";
      showMessage("SETUP");
    }
    return;
  }

  if (WiFi.status() != WL_CONNECTED)
  {
    if (lastDisplayedTime != "NO WIFI")
    {
      lastDisplayedTime = "NO WIFI";
      showMessage("WIFI?");
    }
    return;
  }

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 50))
  {
    if (lastDisplayedTime != "SYNC")
    {
      lastDisplayedTime = "SYNC";
      showMessage("SYNC");
    }
    return;
  }

  int currentMinutes = timeinfo.tm_hour * 60 + timeinfo.tm_min;
  int startMinutes = timeStringToMinutes(dimStart);
  int endMinutes = timeStringToMinutes(dimEnd);
  bool night = isNightTime(currentMinutes, startMinutes, endMinutes);
  display.setIntensity(night ? nightBrightness : dayBrightness);

  char timeText[8];
  //snprintf(timeText, sizeof(timeText), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);

  if (use24HourClock) {
    // 24-hour display, e.g. 21:05
    strftime(timeText, sizeof(timeText), "%H:%M", &timeinfo);
  } else {
    // 12-hour display, e.g. 9:05
    strftime(timeText, sizeof(timeText), "%I:%M", &timeinfo);

    // Remove leading zero, e.g. "09:05" -> "9:05"
    if (timeText[0] == '0') {
      memmove(timeText, timeText + 1, strlen(timeText));
    }
  }
  if (lastDisplayedTime != String(timeText))
  {
    lastDisplayedTime = String(timeText);
    display.displayClear();
    display.setTextAlignment(PA_CENTER);
    display.print(timeText);
  }
}

void maintainWiFi()
{
  if (setupMode)
  {
    return;
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    return;
  }

  static unsigned long lastReconnectAttempt = 0;
  if (millis() - lastReconnectAttempt > 30000)
  {
    lastReconnectAttempt = millis();
    WiFi.disconnect();
    WiFi.begin(wifiSsid.c_str(), wifiPassword.c_str());
  }
}

// --------------------------------------------------
// Arduino lifecycle
// --------------------------------------------------
void setup()
{
  Serial.begin(115200);

  display.begin();
  display.setIntensity(dayBrightness);
  display.displayClear();
  display.setTextAlignment(PA_CENTER);
  display.print("BOOT");

  loadSettings();
  display.setIntensity(dayBrightness);

  connectWiFi();
  setupWebServer();

  //startSetupAccessPoint();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Wifi connected");
    Serial.print("Wifi status before OTA: ");
    Serial.println(WiFi.status());
    setupOTA();
  }

}

void loop()
{
  if (setupMode)
  {
    dnsServer.processNextRequest();
  }

  server.handleClient();
  maintainWiFi();
  updateDisplay();
  

 if (otaReady) {
    ArduinoOTA.handle();
}


  if (!setupMode && WiFi.status() == WL_CONNECTED && !timeIsValid() && millis() - lastNtpSyncAttempt > 60000)
  {
    configureTime();
  }
}
