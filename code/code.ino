#include "globals.h"
#include "wifi_config.h"
#include "ap_config.h"
#include "config.h"
#include "web_handlers.h"
#include "web_handlers.h"
#include "modem.h"
#include "web_handlers.h"
#include "push.h"
#include "web_handlers.h"
#include "sms_process.h"
#include "history.h"
#include "web_handlers.h"

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  Serial.begin(115200);
  // 缩短初始化延时，WiFi连接会处理自己的超时
  delay(200);
  Serial1.setRxBufferSize(SERIAL_BUFFER_SIZE);
  Serial1.begin(115200, SERIAL_8N1, RXD, TXD);
  while (Serial1.available()) Serial1.read();
  modemPowerCycle();
  while (Serial1.available()) Serial1.read();
  initConcatBuffer();
  loadConfig();
  initForwardHistory();
  configValid = isConfigValid();

  // ---- WiFi 连接优化 ----
  bool apEnabledAtBoot = loadApEnabled();
  WiFi.disconnect(true, true);             // 清除 SDK 历史连接缓存，避免双频合一 AP 残留
  delay(100);
  WiFi.mode(apEnabledAtBoot ? WIFI_AP_STA : WIFI_STA);
  WiFi.setSleep(false);                    // 关闭 Modem Sleep，提高连接响应速度
  WiFi.setAutoReconnect(true);             // 断线后自动重连
  WiFi.setTxPower(WIFI_POWER_17dBm);       // 稳定射频功耗
  // 使用快速扫描而非全信道扫描（全信道扫描在空信道上等待超时极慢）
  // 首次连接成功后 ESP32 会自动记住信道，下次启动更快
  WiFi.setScanMethod(WIFI_FAST_SCAN);
  WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
  String wifiSsid = loadWifiSetting("ssid", WIFI_SSID);
  String wifiPass = loadWifiSetting("pass", WIFI_PASS);
  applyWifiSecurity(loadWifiAuth());

  // ESP32-C3 只支持 2.4G；双频合一 SSID 下显式锁定扫描到的最佳物理 BSSID
  // 和信道，避免连接落到不可用的 5G 虚拟 AP 或反复协商失败。
  int scanCount = WiFi.scanNetworks(false, false, false, 300);
  int bestRssi = -1000;
  int bestChannel = 0;
  uint8_t bestBssid[6] = {0};
  bool found = false;
  for (int i = 0; i < scanCount; i++) {
    if (WiFi.SSID(i) == wifiSsid && WiFi.RSSI(i) > bestRssi) {
      bestRssi = WiFi.RSSI(i);
      bestChannel = WiFi.channel(i);
      memcpy(bestBssid, WiFi.BSSID(i), 6);
      found = true;
    }
  }
  WiFi.scanDelete();
  delay(50);

  if (found) {
    logCaptureLn(String("连接wifi: ") + wifiSsid +
                 " (2.4G 信道: " + String(bestChannel) + ")");
    WiFi.begin(wifiSsid.c_str(), wifiPass.c_str(), bestChannel, bestBssid);
  } else {
    logCaptureLn(String("连接wifi: ") + wifiSsid);
    WiFi.begin(wifiSsid.c_str(), wifiPass.c_str());
  }

  // 带超时的等待连接，失败则重启重试
  unsigned long wifiStart = millis();
  const unsigned long WIFI_TIMEOUT = 20000; // 20秒超时
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < WIFI_TIMEOUT) {
    blink_short(200);
  }

  if (WiFi.status() == WL_CONNECTED) {
    logCaptureLn(String("wifi已连接"));
    logCapture(String("IP地址: "));
    logCaptureLn(WiFi.localIP().toString());
    logCapture(String("信号强度(RSSI): "));
    logCaptureLn(String(WiFi.RSSI()) + " dBm");
  } else {
    logCaptureLn(String("[!] WiFi连接超时，继续启动；仅4G推送和短信接收仍可工作"));
  }

  if (apEnabledAtBoot) {
    startAccessPoint();
  }

  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/history", handleHistory);
  server.on("/testpush", HTTP_POST, handleTestPush);
  server.on("/pushnet", HTTP_POST, handlePushNetwork);
  server.on("/tools", handleRoot);
  server.on("/sms", handleRoot);
  server.on("/sendsms", HTTP_POST, handleSendSms);
  server.on("/ping", HTTP_POST, handlePing);
  server.on("/query", handleQuery);
  server.on("/flight", handleFlightMode);
  server.on("/at", handleATCommand);
  server.on("/log", handleLog);
  server.on("/modem", handleModem);
  server.on("/wifi", handleWifi);
  server.on("/ap", handleAp);
  server.on("/system", handleSystem);
  server.begin();
  logCaptureLn(String("HTTP服务器已启动"));

  // ---- NTP 时间同步 ----
  logCaptureLn(String("正在同步NTP时间..."));
  configTzTime("CST-8", "ntp.ntsc.ac.cn", "ntp.aliyun.com", "pool.ntp.org");
  int ntpRetry = 0;
  while (time(nullptr) < 100000 && ntpRetry < 100) {
    delay(1);
    server.handleClient();
    ntpRetry++;
  }
  if (time(nullptr) >= 100000) {
    timeSynced = true;
    logCaptureLn(String("NTP时间同步成功"));
    time_t now = time(nullptr);
    logCapture(String("当前UTC时间戳: "));
    logCaptureLn(String(now));
  } else {
    logCaptureLn(String("NTP时间同步失败，将使用设备时间"));
  }

  digitalWrite(LED_BUILTIN, LOW);

  // ---- 模组初始化（较慢，但网页已可访问） ----
  modemInit();
}

void loop() {
  server.handleClient();
  if (!configValid) {
    if (millis() - lastPrintTime >= 1000) {
      lastPrintTime = millis();
      logCaptureLn(String("[!] 请访问 " + getDeviceUrl() + " 配置系统参数"));
    }
  }
  if (Serial.available()) Serial1.write(Serial.read());
  checkSerial1URC();
  checkConcatTimeout();
  processPendingSms();

  static unsigned long lastSmsPollMs = 0;
  if (millis() - lastSmsPollMs >= 60000UL) {
    lastSmsPollMs = millis();
    pollStoredSms();
  }
}
