// 编译期兜底配置；运行时优先使用 NVS 中保存的 WiFi 连接信息
#include <Arduino.h>

#define WIFI_SSID "你家wifi"
#define WIFI_PASS "你家wifi密码"

String loadWifiSetting(const char* key, const char* fallback);
uint8_t loadWifiAuth();
void saveWifiConnection(const String& ssid, const String& pass, uint8_t auth);
void applyWifiSecurity(uint8_t auth);
String wifiAuthChoiceText(uint8_t auth);
String wifiAuthScanText(uint8_t auth);
