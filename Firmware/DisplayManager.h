#pragma once

#include <Arduino.h>
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>

extern MD_Parola display;

void initDisplay();
void showMessage(const char* message);
void applyBrightness();
void updateDisplayIfNeeded();
