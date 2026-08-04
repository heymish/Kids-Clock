#pragma once

#include <MD_MAX72xx.h>

// --------------------------------------------------
// MAX7219 display settings
// --------------------------------------------------
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4
#define DATA_PIN 7
#define CLK_PIN 6
#define CS_PIN 10

// --------------------------------------------------
// Defaults
// --------------------------------------------------
#define DEFAULT_NTP_SERVER "nz.pool.ntp.org"
#define DEFAULT_TIMEZONE "NZST-12NZDT,M9.5.0,M4.1.0/3"
#define DEFAULT_AP_SSID "ESP32-Clock-Setup"
#define DEFAULT_AP_PASSWORD "clock1234"

// Update intervals
const unsigned long DISPLAY_UPDATE_INTERVAL_MS = 1000;
const unsigned long NTP_SYNC_INTERVAL_MS = 6UL * 60UL * 60UL * 1000UL;
const unsigned long WIFI_CONNECT_TIMEOUT_MS = 20000;
