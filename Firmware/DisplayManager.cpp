#include "DisplayManager.h"
#include "Config.h"
#include "Settings.h"
#include "TimeManager.h"

MD_Parola display = MD_Parola(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);

static unsigned long lastDisplayUpdate = 0;
static String lastDisplayedTime = "";

void initDisplay() {
  display.begin();
  display.setIntensity(8);
  display.displayClear();
  display.setTextAlignment(PA_CENTER);
}

void showMessage(const char* message) {
  display.displayClear();
  display.setTextAlignment(PA_CENTER);
  display.print(message);
}

void applyBrightness() {
  int intensity = isNightDimTime() ? settings.nightBrightness : settings.dayBrightness;
  display.setIntensity(constrain(intensity, 0, 15));
}

void updateDisplayIfNeeded() {
  if (millis() - lastDisplayUpdate < DISPLAY_UPDATE_INTERVAL_MS) {
    return;
  }

  lastDisplayUpdate = millis();
  applyBrightness();

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 10)) {
    if (lastDisplayedTime != "----") {
      lastDisplayedTime = "----";
      showMessage("----");
    }
    return;
  }

  char buffer[6];
  strftime(buffer, sizeof(buffer), "%H:%M", &timeinfo);
  String currentTime = String(buffer);

  if (currentTime != lastDisplayedTime) {
    lastDisplayedTime = currentTime;
    display.displayClear();
    display.setTextAlignment(PA_CENTER);
    display.print(currentTime);
  }
}
