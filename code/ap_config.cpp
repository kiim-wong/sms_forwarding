#include "ap_config.h"
#include "globals.h"
#include "web_handlers.h"

String defaultApSsid() {
  String mac = WiFi.macAddress();
  mac.replace(":", "");
  mac.toUpperCase();
  if (mac.length() < 4) return "SMS_FWD短信转发";
  return mac.substring(mac.length() - 4) + "短信转发";
}

bool loadApEnabled() {
  Preferences prefs;
  if (!prefs.begin("ap_config", true)) return true;
  bool enabled = prefs.getBool("enabled", true);
  prefs.end();
  return enabled;
}

String loadApSsid() {
  Preferences prefs;
  String fallback = defaultApSsid();
  if (!prefs.begin("ap_config", true)) return fallback;
  bool exists = prefs.isKey("ssid");
  String value = prefs.getString("ssid", "");
  prefs.end();
  return exists && value.length() > 0 ? value : fallback;
}

String loadApPassword() {
  Preferences prefs;
  if (!prefs.begin("ap_config", true)) return AP_DEFAULT_PASSWORD;
  bool exists = prefs.isKey("pass");
  String value = prefs.getString("pass", "");
  prefs.end();
  return exists && value.length() >= 8 ? value : String(AP_DEFAULT_PASSWORD);
}

bool loadApPasswordChanged() {
  Preferences prefs;
  if (!prefs.begin("ap_config", true)) return false;
  bool changed = prefs.getBool("passChanged", false);
  prefs.end();
  return changed;
}

bool loadApPromptShown() {
  Preferences prefs;
  if (!prefs.begin("ap_config", true)) return false;
  bool shown = prefs.getBool("promptShown", false);
  prefs.end();
  return shown;
}

void markApPromptShown() {
  Preferences prefs;
  if (!prefs.begin("ap_config", false)) return;
  prefs.putBool("promptShown", true);
  prefs.end();
}

void resetApPrompt() {
  Preferences prefs;
  if (!prefs.begin("ap_config", false)) return;
  prefs.putBool("promptShown", false);
  prefs.end();
}

void saveApConfig(bool enabled, const String& ssid, const String& password) {
  Preferences prefs;
  if (!prefs.begin("ap_config", false)) return;
  prefs.putBool("enabled", enabled);
  prefs.putString("ssid", ssid);
  prefs.putString("pass", password);
  prefs.putBool("passChanged", password != AP_DEFAULT_PASSWORD);
  prefs.end();
}

bool startAccessPoint() {
  if (!loadApEnabled()) {
    stopAccessPoint();
    return false;
  }

  String ssid = loadApSsid();
  String password = loadApPassword();
  WiFi.softAPdisconnect(true);
  delay(100);
  WiFi.mode(WIFI_AP_STA);
  bool ok = WiFi.softAP(ssid.c_str(), password.c_str(), 1, 0, 4);
  if (ok) {
    logCaptureLn(String("热点已启用: ") + ssid + " / " + WiFi.softAPIP().toString());
  } else {
    logCaptureLn(String("[!] 热点启动失败: ") + ssid);
  }
  return ok;
}

void stopAccessPoint() {
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  logCaptureLn(String("热点已关闭"));
}
