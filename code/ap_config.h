#ifndef AP_CONFIG_H
#define AP_CONFIG_H

#include <Arduino.h>

#define AP_DEFAULT_PASSWORD "12345678"

String defaultApSsid();
bool loadApEnabled();
String loadApSsid();
String loadApPassword();
bool loadApPasswordChanged();
bool loadApPromptShown();
void markApPromptShown();
void resetApPrompt();
void saveApConfig(bool enabled, const String& ssid, const String& password);
bool startAccessPoint();
void stopAccessPoint();

#endif
