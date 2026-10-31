#include "wifi_config.h"
#include "globals.h"

String loadWifiSetting(const char* key, const char* fallback) {
  Preferences wifiPrefs;
  if (!wifiPrefs.begin("wifi_config", true)) return String(fallback);
  bool exists = wifiPrefs.isKey(key);
  String value = wifiPrefs.getString(key, "");
  wifiPrefs.end();
  return exists ? value : String(fallback);
}

uint8_t loadWifiAuth() {
  Preferences wifiPrefs;
  if (!wifiPrefs.begin("wifi_config", true)) return 0;
  uint8_t auth = wifiPrefs.getUChar("auth", 0);
  wifiPrefs.end();
  return auth <= 4 ? auth : 0;
}

void saveWifiConnection(const String& ssid, const String& pass, uint8_t auth) {
  Preferences wifiPrefs;
  if (!wifiPrefs.begin("wifi_config", false)) return;
  wifiPrefs.putString("ssid", ssid);
  wifiPrefs.putString("pass", auth == 1 ? "" : pass);
  wifiPrefs.putUChar("auth", auth <= 4 ? auth : 0);
  wifiPrefs.end();
}

void applyWifiSecurity(uint8_t auth) {
  // ESP32 的 WiFi.begin() 会自动协商实际加密方式；这里设置最低可接受
  // 安全等级，避免用户明确选择 WPA/WPA2 时被默认 WPA2 门槛拒绝。
  switch (auth) {
    case 1: WiFi.setMinSecurity(WIFI_AUTH_OPEN); break;
    case 2: WiFi.setMinSecurity(WIFI_AUTH_WPA_PSK); break;
    case 3: WiFi.setMinSecurity(WIFI_AUTH_WPA2_PSK); break;
    case 4: WiFi.setMinSecurity(WIFI_AUTH_WPA2_WPA3_PSK); break;
    default: WiFi.setMinSecurity(WIFI_AUTH_WPA2_PSK); break;
  }
}

String wifiAuthChoiceText(uint8_t auth) {
  switch (auth) {
    case 1: return "开放（无密码）";
    case 2: return "WPA / WPA2";
    case 3: return "WPA2";
    case 4: return "WPA3";
    default: return "自动";
  }
}

String wifiAuthScanText(uint8_t auth) {
  switch (auth) {
    case WIFI_AUTH_OPEN: return "开放";
    case WIFI_AUTH_WEP: return "WEP";
    case WIFI_AUTH_WPA_PSK: return "WPA";
    case WIFI_AUTH_WPA2_PSK: return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK: return "WPA / WPA2";
    case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2 企业";
    case WIFI_AUTH_WPA3_PSK: return "WPA3";
    case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA2 / WPA3";
    default: return "未知";
  }
}
