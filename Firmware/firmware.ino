#include <Arduino.h>

#include "Config.h"
#include "Settings.h"
#include "DisplayManager.h"
#include "TimeManager.h"
#include "ClockWifi.h"
#include "WebUi.h"

void setup() {
  Serial.begin(115200);
  delay(200);

  initDisplay();
  showMessage("BOOT");

  loadSettings();
  applyBrightness();

  connectWiFi();
  configureTime();

  setupWebServer();
}

void loop() {
  server.handleClient();
  updateDisplayIfNeeded();
  syncTimeIfNeeded();
}
