#pragma once

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

void setupWebServer();
void handleRoot();
void handleSave();
String htmlHeader();
String htmlFooter();
String htmlEscape(String text);
