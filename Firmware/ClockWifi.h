#pragma once

#include <Arduino.h>
#include <IPAddress.h>

void connectWiFi();
void startSetupAccessPoint();
bool parseIpAddress(const String& text, IPAddress& ip);
