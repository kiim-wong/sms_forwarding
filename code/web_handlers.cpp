#include "web_handlers.h"
#include "web_html.h"
#include "config.h"
#include "history.h"
#include "modem.h"
#include "push.h"
#include "wifi_config.h"
#include "ap_config.h"

// ---- 日志环形缓冲区 ----
String logBuffer[LOG_BUF_SIZE];
int logBufIdx = 0;
int logBufCount = 0;
static String _logLine;  // 行缓冲：logCapture 写入这里，logCaptureLn 提交整行

static void _logAppend(const String& line) {
  logBuffer[logBufIdx] = line;
  logBufIdx = (logBufIdx + 1) % LOG_BUF_SIZE;
  if (logBufCount < LOG_BUF_SIZE) logBufCount++;
}

static void _logCommit() {
  if (_logLine.length() > 0) {
    _logAppend(_logLine);
    _logLine = "";
  }
}

void logCapture(const String& msg) {
  Serial.print(msg);
  _logLine += msg;
}

void logCapture(const char* msg) {
  Serial.print(msg);
  _logLine += msg;
}

void logCaptureF(const char* fmt, ...) {
  char buf[256];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  Serial.print(buf);
  _logLine += buf;
  // 如果格式化字符串以 \n 结尾，则提交此行
  size_t len = strlen(buf);
  if (len > 0 && buf[len - 1] == '\n') {
    _logLine.trim();  // 去掉尾部空格和可能多余的 \n
    _logCommit();
  }
}

void logCaptureLn(const String& msg) {
  Serial.println(msg);
  _logLine += msg;
  _logCommit();
}

void logCaptureLn(const char* msg) {
  Serial.println(msg);
  _logLine += msg;
  _logCommit();
}

// 检查HTTP Basic认证
bool checkAuth() {
  if (!server.authenticate(config.webUser.c_str(), config.webPass.c_str())) {
    server.requestAuthentication(BASIC_AUTH, "SMS Forwarding", "请输入管理员账号密码");
    return false;
  }
  return true;
}

// HTML 转义，避免配置值破坏属性或 textarea 内容
static String htmlEscape(const String& input) {
  String output;
  output.reserve(input.length() + 16);
  for (unsigned int i = 0; i < input.length(); i++) {
    char c = input.charAt(i);
    switch (c) {
      case '&': output += "&amp;"; break;
      case '<': output += "&lt;"; break;
      case '>': output += "&gt;"; break;
      case '"': output += "&quot;"; break;
      case '\'': output += "&#39;"; break;
      default: output += c; break;
    }
  }
  return output;
}

// 发送黑白灰 Apple 风格结果页
static void sendResultPage(bool success, const String& title, const String& message, const String& refreshUrl) {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
  <meta http-equiv="refresh" content="3;url=%REFRESH%">
  <title>%TITLE%</title>
  <style>
    :root {
      --ink: #142235;
      --muted: #667085;
      --line: rgba(20, 34, 53, .12);
      --glass: rgba(255, 255, 255, .66);
    }
    * { box-sizing: border-box; }
    html, body { min-height: 100%; }
    body {
      margin: 0;
      min-height: 100vh;
      display: grid;
      place-items: center;
      padding: max(24px, env(safe-area-inset-top)) 24px max(24px, env(safe-area-inset-bottom));
      color: var(--ink);
      font-family: -apple-system, BlinkMacSystemFont, "SF Pro Text", "PingFang SC", "Hiragino Sans GB", "Segoe UI", sans-serif;
      background:
        radial-gradient(60% 55% at 12% 8%, rgba(255,255,255,.92), transparent 72%),
        radial-gradient(50% 45% at 88% 92%, rgba(140,155,175,.28), transparent 76%),
        linear-gradient(145deg, #edf2f7 0%, #dce5ee 52%, #cfd9e4 100%);
    }
    .result-card {
      width: min(430px, 100%);
      padding: 30px 28px;
      text-align: center;
      border: 1px solid rgba(255, 255, 255, .72);
      border-radius: 30px;
      background: var(--glass);
      -webkit-backdrop-filter: blur(28px) saturate(165%);
      backdrop-filter: blur(28px) saturate(165%);
      box-shadow: 0 28px 80px rgba(20, 34, 53, .16), inset 0 1px 0 rgba(255,255,255,.78);
    }
    .result-icon {
      width: 54px;
      height: 54px;
      margin: 0 auto 18px;
      display: grid;
      place-items: center;
      border-radius: 50%;
      color: var(--ink);
      border: 1px solid var(--line);
      background: rgba(20, 34, 53, .055);
      box-shadow: inset 0 1px 0 rgba(255,255,255,.8);
    }
    .result-icon svg {
      width: 25px;
      height: 25px;
      fill: none;
      stroke: currentColor;
      stroke-width: 1.8;
      stroke-linecap: round;
      stroke-linejoin: round;
    }
    h1 { margin: 0 0 10px; font-size: 21px; line-height: 1.3; letter-spacing: -.02em; }
    p { margin: 0; color: var(--muted); font-size: 14px; line-height: 1.7; }
    @media (prefers-reduced-transparency: reduce) {
      .result-card { background: #f7f9fb; -webkit-backdrop-filter: none; backdrop-filter: none; }
    }
  </style>
</head>
<body>
  <main class="result-card" role="status" aria-live="polite">
    <div class="result-icon" aria-hidden="true"><svg viewBox="0 0 24 24">%ICON%</svg></div>
    <h1>%TITLE%</h1>
    <p>%MESSAGE%</p>
  </main>
</body>
</html>
)rawliteral";

  html.replace("%REFRESH%", htmlEscape(refreshUrl));
  html.replace("%TITLE%", htmlEscape(title));
  html.replace("%MESSAGE%", htmlEscape(message));
  html.replace("%ICON%", success ? "<path d=\"M5 12.5 10 17l9-10\"/>" : "<path d=\"M7 7l10 10M17 7 7 17\"/>");
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "text/html; charset=utf-8", html);
}

// 对 HTML 片段应用配置值占位符
static bool apPromptNeededThisPage = false;

static void applyHtmlConfig(String& html) {
  html.replace("%IP%", WiFi.localIP().toString());
  String currentSsid = WiFi.status() == WL_CONNECTED ? WiFi.SSID() : loadWifiSetting("ssid", WIFI_SSID);
  html.replace("%WIFI_SSID%", htmlEscape(currentSsid));
  html.replace("%WIFI_STATUS_TEXT%", WiFi.status() == WL_CONNECTED ? "已连接" : "未连接");
  html.replace("%WIFI_RSSI_TEXT%", WiFi.status() == WL_CONNECTED
                                       ? (String(WiFi.RSSI()) + " dBm")
                                       : String("--"));
  html.replace("%WIFI_SAVED_SSID%", htmlEscape(loadWifiSetting("ssid", WIFI_SSID)));
  html.replace("%WIFI_AUTH_MODE%", String((int)loadWifiAuth()));
  html.replace("%OP_MODE_AUTO%", config.operatorMode == 0 ? " selected" : "");
  html.replace("%OP_MODE_MANUAL%", config.operatorMode == 1 ? " selected" : "");
  html.replace("%OPERATOR_CODE%", htmlEscape(config.operatorCode));
  bool apEnabled = loadApEnabled();
  bool apRunning = (WiFi.getMode() & WIFI_AP) != 0;
  html.replace("%AP_ENABLED%", apEnabled ? "1" : "0");
  html.replace("%AP_ENABLED_CHECKED%", apEnabled ? " checked" : "");
  html.replace("%AP_SSID%", htmlEscape(loadApSsid()));
  html.replace("%AP_DEFAULT_SSID%", htmlEscape(defaultApSsid()));
  html.replace("%AP_STATUS_TEXT%", apRunning ? "已启用" : "已关闭");
  html.replace("%AP_IP%", apRunning ? WiFi.softAPIP().toString() : String("--"));
  html.replace("%AP_CLIENTS%", String(apRunning ? WiFi.softAPgetStationNum() : 0));
  html.replace("%AP_PASSWORD_CHANGED%", loadApPasswordChanged() ? "1" : "0");
  html.replace("%AP_PROMPT_NEEDED%", apPromptNeededThisPage ? "1" : "0");
  html.replace("%AP_DEFAULT_PASSWORD%", AP_DEFAULT_PASSWORD);
  html.replace("%FREE_HEAP%", String(ESP.getFreeHeap() / 1024) + " KB");
  long uptimeSec = millis() / 1000;
  char uptimeBuf[16];
  snprintf(uptimeBuf, sizeof(uptimeBuf), "%ld:%02ld:%02ld", uptimeSec / 3600, (uptimeSec % 3600) / 60, uptimeSec % 60);
  html.replace("%UPTIME%", String(uptimeBuf));
  html.replace("%WEB_USER%", htmlEscape(config.webUser));
  html.replace("%WEB_PASS%", htmlEscape(config.webPass));
  html.replace("%ADMIN_PHONE%", htmlEscape(config.adminPhone));
  html.replace("%NUMBER_BLACK_LIST%", htmlEscape(config.numberBlackList));
  html.replace("%NUMBER_PREFIX_BLACK_LIST%", htmlEscape(config.numberPrefixBlackList));
  html.replace("%NUMBER_SUFFIX_BLACK_LIST%", htmlEscape(config.numberSuffixBlackList));
  html.replace("%CONTENT_BLACK_LIST%", htmlEscape(config.contentBlackList));

  // 概览页面的配置状态
  html.replace("%HISTORY_COUNT%", String(forwardHistoryCount()));
  html.replace("%MODEM_CHECK%", modemReady ? "已就绪" : "未就绪");
  html.replace("%PUSH_NET_MODE%", String((int)config.pushNetworkMode));
  int pushCount = 0;
  for (int i = 0; i < MAX_PUSH_CHANNELS; i++) {
    if (config.pushChannels[i].enabled) pushCount++;
  }
  html.replace("%PUSH_COUNT%", String(pushCount));
}

// 处理配置页面请求
void handleRoot() {
  if (!checkAuth()) return;

  apPromptNeededThisPage = !loadApPasswordChanged() && !loadApPromptShown();
  if (apPromptNeededThisPage) markApPromptShown();

  // 无 PSRAM 的 ESP32 上单个 Arduino String 上限为 65,535 字节，整页约 73 KB，装不下。
  // 以推送通道为界分段流式发送，避免 String 容量上限和堆内存峰值。
  const char* pushToken = "%PUSH_CHANNELS%";
  const char* pushPos = strstr(htmlPage, pushToken);
  if (!pushPos) {
    server.send(500, "text/plain", "HTML template placeholder missing");
    return;
  }

  // 禁用缓存：固件更新后确保浏览器拉取新版页面，避免旧版 UI 与新固件不匹配
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "0");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", "");

  unsigned int beforeLen = (unsigned int)(pushPos - htmlPage);
  {
    String htmlBefore(htmlPage, beforeLen);
    if (htmlBefore.length() != beforeLen) {
      logCaptureLn(String("[ERR] 首页前段内存不足"));
      server.sendContent("", 0);
      return;
    }
    applyHtmlConfig(htmlBefore);
    server.sendContent(htmlBefore);
  }
  
  // 生成推送通道HTML
  String channelsHtml = "";
  channelsHtml.reserve(MAX_PUSH_CHANNELS * 2600);
  for (int i = 0; i < MAX_PUSH_CHANNELS; i++) {
    String idx = String(i);
    String enabledClass = config.pushChannels[i].enabled ? " enabled" : "";
    String checked = config.pushChannels[i].enabled ? " checked" : "";
    
    channelsHtml += "<div class=\"push-channel" + enabledClass + "\" id=\"channel" + idx + "\">";
    channelsHtml += "<div class=\"push-channel-header\">";
    channelsHtml += "<input type=\"checkbox\" name=\"push" + idx + "en\" id=\"push" + idx + "en\" onchange=\"toggleChannel(" + idx + ")\"" + checked + ">";
    channelsHtml += "<label for=\"push" + idx + "en\" class=\"label-inline\">启用推送通道 " + String(i + 1) + "</label>";
    channelsHtml += "<button type=\"button\" class=\"btn btn-secondary btn-sm push-test-btn\" id=\"testBtn" + idx + "\" onclick=\"testPush(" + idx + ")\">测试</button>";
    channelsHtml += "</div>";
    channelsHtml += "<div class=\"result-box push-test-result\" id=\"testResult" + idx + "\"></div>";
    channelsHtml += "<div class=\"push-channel-body\">";
    
    // 通道名称
    channelsHtml += "<div class=\"form-group\">";
    channelsHtml += "<label>通道名称</label>";
    channelsHtml += "<input type=\"text\" name=\"push" + idx + "name\" value=\"" + htmlEscape(config.pushChannels[i].name) + "\" placeholder=\"自定义名称\">";
    channelsHtml += "</div>";
    
    // 推送类型
    channelsHtml += "<div class=\"form-group\">";
    channelsHtml += "<label>推送方式</label>";
    channelsHtml += "<select name=\"push" + idx + "type\" id=\"push" + idx + "type\" onchange=\"updateTypeHint(" + idx + ")\">";
    channelsHtml += "<option value=\"1\"" + String(config.pushChannels[i].type == PUSH_TYPE_POST_JSON ? " selected" : "") + ">POST JSON（通用格式）</option>";
    channelsHtml += "<option value=\"2\"" + String(config.pushChannels[i].type == PUSH_TYPE_BARK ? " selected" : "") + ">Bark（iOS推送）</option>";
    channelsHtml += "<option value=\"3\"" + String(config.pushChannels[i].type == PUSH_TYPE_GET ? " selected" : "") + ">GET请求（参数在URL中）</option>";
    channelsHtml += "<option value=\"4\"" + String(config.pushChannels[i].type == PUSH_TYPE_DINGTALK ? " selected" : "") + ">钉钉机器人</option>";
    channelsHtml += "<option value=\"5\"" + String(config.pushChannels[i].type == PUSH_TYPE_PUSHPLUS ? " selected" : "") + ">PushPlus</option>";
    channelsHtml += "<option value=\"6\"" + String(config.pushChannels[i].type == PUSH_TYPE_SERVERCHAN ? " selected" : "") + ">Server酱</option>";
    channelsHtml += "<option value=\"7\"" + String(config.pushChannels[i].type == PUSH_TYPE_CUSTOM ? " selected" : "") + ">自定义模板</option>";
    channelsHtml += "<option value=\"8\"" + String(config.pushChannels[i].type == PUSH_TYPE_FEISHU ? " selected" : "") + ">飞书机器人</option>";
    channelsHtml += "<option value=\"9\"" + String(config.pushChannels[i].type == PUSH_TYPE_GOTIFY ? " selected" : "") + ">Gotify</option>";
    channelsHtml += "<option value=\"10\"" + String(config.pushChannels[i].type == PUSH_TYPE_TELEGRAM ? " selected" : "") + ">Telegram Bot</option>";
    channelsHtml += "<option value=\"11\"" + String(config.pushChannels[i].type == PUSH_TYPE_WECOM ? " selected" : "") + ">企业微信机器人</option>";
    channelsHtml += "</select>";
    channelsHtml += "<div class=\"push-type-hint\" id=\"hint" + idx + "\"></div>";
    channelsHtml += "</div>";
    
    // URL
    channelsHtml += "<div class=\"form-group\">";
    channelsHtml += "<label>推送URL/Webhook</label>";
    channelsHtml += "<input type=\"text\" name=\"push" + idx + "url\" id=\"url" + idx + "\" value=\"" + htmlEscape(config.pushChannels[i].url) + "\" placeholder=\"http://your-server.com/api 或 webhook地址\">";
    channelsHtml += "</div>";
    
    // 额外参数区域（钉钉/PushPlus/Server酱等需要）
    channelsHtml += "<div id=\"extra" + idx + "\" style=\"display:none;\">";
    channelsHtml += "<div class=\"form-group\">";
    channelsHtml += "<label id=\"key1label" + idx + "\">参数1</label>";
    channelsHtml += "<input type=\"text\" name=\"push" + idx + "key1\" id=\"key1" + idx + "\" value=\"" + htmlEscape(config.pushChannels[i].key1) + "\">";
    channelsHtml += "</div>";
    channelsHtml += "<div class=\"form-group\" id=\"key2group" + idx + "\">";
    channelsHtml += "<label id=\"key2label" + idx + "\">参数2</label>";
    channelsHtml += "<input type=\"text\" name=\"push" + idx + "key2\" id=\"key2" + idx + "\" value=\"" + htmlEscape(config.pushChannels[i].key2) + "\">";
    channelsHtml += "</div>";
    channelsHtml += "</div>";
    
    // 自定义模板区域
    channelsHtml += "<div id=\"custom" + idx + "\" style=\"display:none;\">";
    channelsHtml += "<div class=\"form-group\">";
    channelsHtml += "<label>请求体模板（使用 {sender} {message} {timestamp} 占位符）</label>";
    channelsHtml += "<textarea name=\"push" + idx + "body\" rows=\"4\" style=\"width:100%;font-family:monospace;\">" + htmlEscape(config.pushChannels[i].customBody) + "</textarea>";
    channelsHtml += "</div>";
    channelsHtml += "</div>";
    
    channelsHtml += "</div></div>";
  }
  server.sendContent(channelsHtml);

  {
    String htmlAfter(pushPos + strlen(pushToken));
    if (htmlAfter.length() == 0) {
      logCaptureLn(String("[ERR] 首页后段内存不足"));
      server.sendContent("", 0);
      return;
    }
    applyHtmlConfig(htmlAfter);
    server.sendContent(htmlAfter);
  }
  server.sendContent("", 0);
}

// 处理工具箱页面请求 — 已整合到主页，直接返回主页
void handleToolsPage() {
  handleRoot();
}

// 处理飞行模式控制请求
void handleFlightMode() {
  if (!checkAuth()) return;
  
  String action = server.arg("action");
  String json = "{";
  bool success = false;
  String message = "";
  
  if (action == "query") {
    // 查询当前功能模式
    logCaptureLn(String("网页端查询飞行模式: AT+CFUN?"));
    String resp = sendATCommand("AT+CFUN?", 2000);
    logCaptureLn(String("CFUN查询响应: " + resp));
    
    if (resp.indexOf("+CFUN:") >= 0) {
      success = true;
      int idx = resp.indexOf("+CFUN:");
      int mode = resp.substring(idx + 6).toInt();
      
      String modeStr;
      String statusIcon;
      if (mode == 0) {
        modeStr = "最小功能模式（关机）";
        statusIcon = "<span class=\"mono-mark\">○</span>";
      } else if (mode == 1) {
        modeStr = "全功能模式（正常）";
        statusIcon = "<span class=\"mono-mark\">●</span>";
      } else if (mode == 4) {
        modeStr = "飞行模式（射频关闭）";
        statusIcon = "<span class=\"mono-mark\">◐</span>";
      } else {
        modeStr = "未知模式 (" + String(mode) + ")";
        statusIcon = "<span class=\"mono-mark\">?</span>";
      }
      
      message = "<table class='info-table'>";
      message += "<tr><td>当前状态</td><td>" + statusIcon + " " + modeStr + "</td></tr>";
      message += "<tr><td>CFUN值</td><td>" + String(mode) + "</td></tr>";
      message += "</table>";
    } else {
      message = "查询失败";
    }
  }
  else if (action == "toggle") {
    // 先查询当前状态
    String resp = sendATCommand("AT+CFUN?", 2000);
    logCaptureLn(String("CFUN查询响应: " + resp));
    
    if (resp.indexOf("+CFUN:") >= 0) {
      int idx = resp.indexOf("+CFUN:");
      int currentMode = resp.substring(idx + 6).toInt();
      
      // 切换模式：1(正常) <-> 4(飞行模式)
      int newMode = (currentMode == 1) ? 4 : 1;
      String cmd = "AT+CFUN=" + String(newMode);
      
      logCaptureLn(String("切换飞行模式: " + cmd));
      String setResp = sendATCommand(cmd.c_str(), 5000);
      logCaptureLn(String("CFUN设置响应: " + setResp));
      
      if (setResp.indexOf("OK") >= 0) {
        success = true;
        if (newMode == 4) {
          message = "已开启飞行模式<br>模组射频已关闭，无法收发短信";
        } else {
          message = "已关闭飞行模式<br>模组恢复正常工作";
        }
      } else {
        message = "切换失败: " + setResp;
      }
    } else {
      message = "无法获取当前状态";
    }
  }
  else if (action == "on") {
    // 强制开启飞行模式
    logCaptureLn(String("网页端强制开启飞行模式: AT+CFUN=4"));
    String resp = sendATCommand("AT+CFUN=4", 5000);
    if (resp.indexOf("OK") >= 0) {
      success = true;
      message = "已开启飞行模式";
    } else {
      message = "开启失败: " + resp;
    }
  }
  else if (action == "off") {
    // 强制关闭飞行模式
    logCaptureLn(String("网页端关闭飞行模式: AT+CFUN=1"));
    String resp = sendATCommand("AT+CFUN=1", 5000);
    if (resp.indexOf("OK") >= 0) {
      success = true;
      message = "已关闭飞行模式";
    } else {
      message = "关闭失败: " + resp;
    }
  }
  else {
    message = "未知操作";
  }
  
  json += "\"success\":" + String(success ? "true" : "false") + ",";
  json += "\"message\":\"" + message + "\"";
  json += "}";
  
  server.send(200, "application/json", json);
}

// 处理AT指令测试请求
void handleATCommand() {
  if (!checkAuth()) return;
  
  String cmd = server.arg("cmd");
  bool success = false;
  String message = "";
  
  if (cmd.length() == 0) {
    message = "错误：指令不能为空";
  } else {
    logCaptureLn(String("网页端发送AT指令: " + cmd));
    String resp = sendATCommand(cmd.c_str(), 5000);
    logCaptureLn(String("模组响应: " + resp));
    
    if (resp.length() > 0) {
      success = true;
      message = resp;
    } else {
      message = "超时或无响应";
    }
  }
  
  String json = "{";
  json += "\"success\":" + String(success ? "true" : "false") + ",";
  json += "\"message\":\"" + jsonEscape(message) + "\"";
  json += "}";
  
  server.send(200, "application/json", json);
}

// 处理模组信息查询请求
void handleQuery() {
  if (!checkAuth()) return;
  
  String type = server.arg("type");
  String json = "{";
  bool success = false;
  String message = "";
  
  if (type == "ati") {
    // 固件信息查询
    String resp = sendATCommand("ATI", 2000);
    logCaptureLn(String("ATI响应: " + resp));
    
    if (resp.indexOf("OK") >= 0) {
      success = true;
      // 解析ATI响应
      String manufacturer = "未知";
      String model = "未知";
      String version = "未知";
      
      // 按行解析
      int lineStart = 0;
      int lineNum = 0;
      for (int i = 0; i < resp.length(); i++) {
        if (resp.charAt(i) == '\n' || i == resp.length() - 1) {
          String line = resp.substring(lineStart, i);
          line.trim();
          if (line.length() > 0 && line != "ATI" && line != "OK") {
            lineNum++;
            if (lineNum == 1) manufacturer = line;
            else if (lineNum == 2) model = line;
            else if (lineNum == 3) version = line;
          }
          lineStart = i + 1;
        }
      }
      
      message = "<table class='info-table'>";
      message += "<tr><td>制造商</td><td>" + manufacturer + "</td></tr>";
      message += "<tr><td>模组型号</td><td>" + model + "</td></tr>";
      message += "<tr><td>固件版本</td><td>" + version + "</td></tr>";
      message += "</table>";
    } else {
      message = "查询失败";
    }
  }
  else if (type == "signal") {
    // 信号质量查询
    String resp = sendATCommand("AT+CESQ", 2000);
    logCaptureLn(String("CESQ响应: " + resp));
    
    if (resp.indexOf("+CESQ:") >= 0) {
      success = true;
      // 解析 +CESQ: <rxlev>,<ber>,<rscp>,<ecno>,<rsrq>,<rsrp>
      int idx = resp.indexOf("+CESQ:");
      String params = resp.substring(idx + 6);
      int endIdx = params.indexOf('\r');
      if (endIdx < 0) endIdx = params.indexOf('\n');
      if (endIdx > 0) params = params.substring(0, endIdx);
      params.trim();
      
      // 分割参数
      String values[6];
      int valIdx = 0;
      int startPos = 0;
      for (int i = 0; i <= params.length() && valIdx < 6; i++) {
        if (i == params.length() || params.charAt(i) == ',') {
          values[valIdx] = params.substring(startPos, i);
          values[valIdx].trim();
          valIdx++;
          startPos = i + 1;
        }
      }
      
      // RSRP转换为dBm (0-97映射到-140到-44 dBm, 99表示未知)
      int rsrp = values[5].toInt();
      String rsrpStr;
      if (rsrp == 99 || rsrp == 255) {
        rsrpStr = "未知";
      } else {
        int rsrpDbm = -140 + rsrp;
        rsrpStr = String(rsrpDbm) + " dBm";
        if (rsrpDbm >= -80) rsrpStr += " (信号极好)";
        else if (rsrpDbm >= -90) rsrpStr += " (信号良好)";
        else if (rsrpDbm >= -100) rsrpStr += " (信号一般)";
        else if (rsrpDbm >= -110) rsrpStr += " (信号较弱)";
        else rsrpStr += " (信号很差)";
      }
      
      // RSRQ转换 (0-34映射到-19.5到-3 dB)
      int rsrq = values[4].toInt();
      String rsrqStr;
      if (rsrq == 99 || rsrq == 255) {
        rsrqStr = "未知";
      } else {
        float rsrqDb = -19.5 + rsrq * 0.5;
        rsrqStr = String(rsrqDb, 1) + " dB";
      }
      
      message = "<table class='info-table'>";
      message += "<tr><td>信号强度 (RSRP)</td><td>" + rsrpStr + "</td></tr>";
      message += "<tr><td>信号质量 (RSRQ)</td><td>" + rsrqStr + "</td></tr>";
      message += "<tr><td>原始数据</td><td>" + params + "</td></tr>";
      message += "</table>";
    } else {
      message = "查询失败";
    }
  }
  else if (type == "siminfo") {
    // SIM卡信息查询
    success = true;
    message = "<table class='info-table'>";
    
    // 查询IMSI
    String resp = sendATCommand("AT+CIMI", 2000);
    String imsi = "未知";
    int lineStart = 0;
    while (lineStart < resp.length()) {
      int lineEnd = resp.indexOf('\n', lineStart);
      if (lineEnd < 0) lineEnd = resp.length();
      String line = resp.substring(lineStart, lineEnd);
      line.trim();
      if (line.length() >= 10 && line != "OK" && !line.startsWith("AT+")) {
        imsi = line;
        break;
      }
      lineStart = lineEnd + 1;
    }
    message += "<tr><td>IMSI</td><td>" + imsi + "</td></tr>";
    
    // 查询ICCID
    resp = sendATCommand("AT+ICCID", 2000);
    String iccid = "未知";
    if (resp.indexOf("+ICCID:") >= 0) {
      int idx = resp.indexOf("+ICCID:");
      String tmp = resp.substring(idx + 7);
      int endIdx = tmp.indexOf('\r');
      if (endIdx < 0) endIdx = tmp.indexOf('\n');
      if (endIdx > 0) iccid = tmp.substring(0, endIdx);
      iccid.trim();
    }
    message += "<tr><td>ICCID</td><td>" + iccid + "</td></tr>";
    
    // 查询本机号码 (如果SIM卡支持)
    resp = sendATCommand("AT+CNUM", 2000);
    String phoneNum = "未存储或不支持";
    if (resp.indexOf("+CNUM:") >= 0) {
      int idx = resp.indexOf(",\"");
      if (idx >= 0) {
        int endIdx = resp.indexOf("\"", idx + 2);
        if (endIdx > idx) {
          phoneNum = resp.substring(idx + 2, endIdx);
        }
      }
    }
    message += "<tr><td>本机号码</td><td>" + phoneNum + "</td></tr>";
    
    message += "</table>";
  }
  else if (type == "network") {
    // 网络状态查询
    success = true;
    message = "<table class='info-table'>";
    
    // 查询网络注册状态
    String resp = sendATCommand("AT+CEREG?", 2000);
    String regStatus = "未知";
    if (resp.indexOf("+CEREG:") >= 0) {
      int idx = resp.indexOf("+CEREG:");
      String tmp = resp.substring(idx + 7);
      int commaIdx = tmp.indexOf(',');
      if (commaIdx >= 0) {
        String stat = tmp.substring(commaIdx + 1, commaIdx + 2);
        int s = stat.toInt();
        switch(s) {
          case 0: regStatus = "未注册，未搜索"; break;
          case 1: regStatus = "已注册，本地网络"; break;
          case 2: regStatus = "未注册，正在搜索"; break;
          case 3: regStatus = "注册被拒绝"; break;
          case 4: regStatus = "未知"; break;
          case 5: regStatus = "已注册，漫游"; break;
          default: regStatus = "状态码: " + stat;
        }
      }
    }
    message += "<tr><td>网络注册</td><td>" + regStatus + "</td></tr>";
    
    // 查询运营商
    resp = sendATCommand("AT+COPS?", 2000);
    String oper = "未知";
    if (resp.indexOf("+COPS:") >= 0) {
      int idx = resp.indexOf(",\"");
      if (idx >= 0) {
        int endIdx = resp.indexOf("\"", idx + 2);
        if (endIdx > idx) {
          oper = resp.substring(idx + 2, endIdx);
        }
      }
    }
    message += "<tr><td>运营商</td><td>" + oper + "</td></tr>";
    
    // 查询PDP上下文激活状态
    resp = sendATCommand("AT+CGACT?", 2000);
    String pdpStatus = "未激活";
    if (resp.indexOf("+CGACT: 1,1") >= 0) {
      pdpStatus = "已激活";
    } else if (resp.indexOf("+CGACT:") >= 0) {
      pdpStatus = "未激活";
    }
    message += "<tr><td>数据连接</td><td>" + pdpStatus + "</td></tr>";
    
    // 查询APN
    resp = sendATCommand("AT+CGDCONT?", 2000);
    String apn = "未知";
    if (resp.indexOf("+CGDCONT:") >= 0) {
      int idx = resp.indexOf(",\"");
      if (idx >= 0) {
        idx = resp.indexOf(",\"", idx + 2);  // 跳过PDP类型
        if (idx >= 0) {
          int endIdx = resp.indexOf("\"", idx + 2);
          if (endIdx > idx) {
            apn = resp.substring(idx + 2, endIdx);
            if (apn.length() == 0) apn = "(自动)";
          }
        }
      }
    }
    message += "<tr><td>APN</td><td>" + apn + "</td></tr>";
    
    message += "</table>";
  }
  else if (type == "wifi") {
    // WiFi状态查询
    success = true;
    message = "<table class='info-table'>";
    
    // WiFi连接状态
    String wifiStatus = WiFi.isConnected() ? "已连接" : "未连接";
    message += "<tr><td>连接状态</td><td>" + wifiStatus + "</td></tr>";
    
    // SSID
    String ssid = WiFi.SSID();
    if (ssid.length() == 0) ssid = "未知";
    message += "<tr><td>当前SSID</td><td>" + ssid + "</td></tr>";
    
    // 信号强度 RSSI
    int rssi = WiFi.RSSI();
    String rssiStr = String(rssi) + " dBm";
    if (rssi >= -50) rssiStr += " (信号极好)";
    else if (rssi >= -60) rssiStr += " (信号很好)";
    else if (rssi >= -70) rssiStr += " (信号良好)";
    else if (rssi >= -80) rssiStr += " (信号一般)";
    else if (rssi >= -90) rssiStr += " (信号较弱)";
    else rssiStr += " (信号很差)";
    message += "<tr><td>信号强度 (RSSI)</td><td>" + rssiStr + "</td></tr>";
    
    // IP地址
    message += "<tr><td>IP地址</td><td>" + WiFi.localIP().toString() + "</td></tr>";
    
    // 网关
    message += "<tr><td>网关</td><td>" + WiFi.gatewayIP().toString() + "</td></tr>";
    
    // 子网掩码
    message += "<tr><td>子网掩码</td><td>" + WiFi.subnetMask().toString() + "</td></tr>";
    
    // DNS
    message += "<tr><td>DNS服务器</td><td>" + WiFi.dnsIP().toString() + "</td></tr>";
    
    // MAC地址
    message += "<tr><td>MAC地址</td><td>" + WiFi.macAddress() + "</td></tr>";
    
    // BSSID (路由器MAC)
    message += "<tr><td>路由器BSSID</td><td>" + WiFi.BSSIDstr() + "</td></tr>";
    
    // 信道
    message += "<tr><td>WiFi信道</td><td>" + String(WiFi.channel()) + "</td></tr>";
    
    message += "</table>";
  }
  else {
    message = "未知的查询类型";
  }
  
  json += "\"success\":" + String(success ? "true" : "false") + ",";
  json += "\"message\":\"" + message + "\"";
  json += "}";
  
  server.send(200, "application/json", json);
}

// 处理发送短信请求
void handleSendSms() {
  if (!checkAuth()) return;
  
  String phone = server.arg("phone");
  String content = server.arg("content");
  
  phone.trim();
  content.trim();
  
  bool success = false;
  String resultMsg = "";
  
  if (phone.length() == 0) {
    resultMsg = "错误：请输入目标号码";
  } else if (content.length() == 0) {
    resultMsg = "错误：请输入短信内容";
  } else {
    logCaptureLn(String("网页端发送短信请求"));
    logCaptureLn(String("目标号码: " + phone));
    logCaptureLn(String("短信内容: " + content));
    
    success = sendSMS(phone.c_str(), content.c_str());
    resultMsg = success ? "短信发送成功！" : "短信发送失败，请检查模组状态";
  }
  
  sendResultPage(success, success ? "短信已发送" : "短信发送失败", resultMsg + " 3 秒后返回发送页面。", "/sms");
}

// 处理Ping请求
void handlePing() {
  if (!checkAuth()) return;

  if (modemCommandBusy()) {
    server.send(429, "application/json", "{\"success\":false,\"message\":\"模组正在执行其他操作，请稍后重试\"}");
    return;
  }
  
  logCaptureLn(String("网页端发起Ping请求"));
  
  // 激活PDP上下文（数据连接）
  logCaptureLn(String("激活数据连接(CGACT)..."));
  String activateResp = sendATCommand("AT+CGACT=1,1", 10000);
  logCaptureLn(String("CGACT响应: " + activateResp));
  
  // 检查激活是否成功（OK或已激活的情况）
  bool networkActivated = (activateResp.indexOf("OK") >= 0);
  if (!networkActivated) {
    logCaptureLn(String("数据连接激活失败，尝试继续执行..."));
  }
  
  delay(500);  // 等待网络稳定
  
  // 发送MPING命令，ping 8.8.8.8，超时30秒，ping 1次
  Serial1.println("AT+MPING=\"8.8.8.8\",30,1");
  
  // 等待响应
  unsigned long start = millis();
  String resp = "";
  bool gotOK = false;
  bool gotError = false;
  bool gotPingResult = false;
  String pingResultMsg = "";
  
  // 等待最多35秒（30秒超时 + 5秒余量）
  while (millis() - start < 35000) {
    while (Serial1.available()) {
      char c = Serial1.read();
      resp += c;
      logCapture(String(c));  // 调试输出
      
      // 检查是否收到OK
      if (resp.indexOf("OK") >= 0 && !gotOK) {
        gotOK = true;
      }
      
      // 检查是否收到ERROR
      if (resp.indexOf("+CME ERROR") >= 0 || resp.indexOf("ERROR") >= 0) {
        gotError = true;
        pingResultMsg = "模组返回错误";
        break;
      }
      
      // 检查是否收到Ping结果URC
      // 成功格式: +MPING: 1,8.8.8.8,32,xxx,xxx
      // 失败格式: +MPING: 2 或其他
      int mpingIdx = resp.indexOf("+MPING:");
      if (mpingIdx >= 0) {
        // 找到换行符确定完整的一行
        int lineEnd = resp.indexOf('\n', mpingIdx);
        if (lineEnd >= 0) {
          String mpingLine = resp.substring(mpingIdx, lineEnd);
          mpingLine.trim();
          logCaptureLn(String("收到MPING结果: " + mpingLine));
          
          // 解析结果
          // +MPING: <result>[,<ip>,<packet_len>,<time>,<ttl>]
          int colonIdx = mpingLine.indexOf(':');
          if (colonIdx >= 0) {
            String params = mpingLine.substring(colonIdx + 1);
            params.trim();
            
            // 获取第一个参数（result）
            int commaIdx = params.indexOf(',');
            String resultStr;
            if (commaIdx >= 0) {
              resultStr = params.substring(0, commaIdx);
            } else {
              resultStr = params;
            }
            resultStr.trim();
            int result = resultStr.toInt();
            
            gotPingResult = true;
            
            // result=0或1都表示成功（不同模组可能返回不同值）
            // 如果有完整的响应参数（IP、时间等），也视为成功
            bool pingSuccess = (result == 0 || result == 1) || (params.indexOf(',') >= 0 && params.length() > 5);
            
            if (pingSuccess) {
              // 成功，解析详细信息
              // 格式: 0/1,"8.8.8.8",16,时间,TTL
              int idx1 = params.indexOf(',');
              if (idx1 >= 0) {
                String rest = params.substring(idx1 + 1);
                // 处理IP地址（可能带引号）
                String ip;
                int idx2;
                if (rest.startsWith("\"")) {
                  // 带引号的IP
                  int quoteEnd = rest.indexOf('\"', 1);
                  if (quoteEnd >= 0) {
                    ip = rest.substring(1, quoteEnd);
                    idx2 = rest.indexOf(',', quoteEnd);
                  } else {
                    idx2 = rest.indexOf(',');
                    ip = rest.substring(0, idx2);
                  }
                } else {
                  idx2 = rest.indexOf(',');
                  ip = rest.substring(0, idx2);
                }
                
                if (idx2 >= 0) {
                  rest = rest.substring(idx2 + 1);
                  int idx3 = rest.indexOf(',');  // packet_len后
                  if (idx3 >= 0) {
                    rest = rest.substring(idx3 + 1);
                    int idx4 = rest.indexOf(',');  // time后
                    String timeStr, ttlStr;
                    if (idx4 >= 0) {
                      timeStr = rest.substring(0, idx4);
                      ttlStr = rest.substring(idx4 + 1);
                    } else {
                      timeStr = rest;
                      ttlStr = "N/A";
                    }
                    timeStr.trim();
                    ttlStr.trim();
                    pingResultMsg = "目标: " + ip + ", 延迟: " + timeStr + "ms, TTL: " + ttlStr;
                  }
                }
              }
              if (pingResultMsg.length() == 0) {
                pingResultMsg = "Ping成功";
              }
            } else {
              // 失败
              pingResultMsg = "Ping超时或目标不可达 (错误码: " + String(result) + ")";
            }
            break;
          }
        }
      }
    }
    
    if (gotError || gotPingResult) break;
    server.handleClient();
  }
  
  logCaptureLn(String("\nPing操作完成"));
  
  // 关闭数据连接以节省流量
  logCaptureLn(String("关闭PDP上下文(CGACT=0)..."));
  String deactivateResp = sendATCommand("AT+CGACT=0,1", 5000);
  logCaptureLn(String("CGACT关闭响应: " + deactivateResp));
  
  // 构建JSON响应
  String json = "{";
  if (gotPingResult && pingResultMsg.indexOf("延迟") >= 0) {
    json += "\"success\":true,";
    json += "\"message\":\"" + pingResultMsg + "\"";
  } else if (gotError) {
    json += "\"success\":false,";
    json += "\"message\":\"" + pingResultMsg + "\"";
  } else if (gotPingResult) {
    json += "\"success\":false,";
    json += "\"message\":\"" + pingResultMsg + "\"";
  } else {
    json += "\"success\":false,";
    json += "\"message\":\"操作超时，未收到Ping结果\"";
  }
  json += "}";
  
  server.send(200, "application/json", json);
}

// 处理保存配置请求
void handleSave() {
  if (!checkAuth()) return;

  bool operatorConfigChanged = false;

  // 账号管理表单：只在字段存在时更新
  if (server.hasArg("webUser")) {
    String newWebUser = server.arg("webUser");
    if (newWebUser.length() == 0) newWebUser = DEFAULT_WEB_USER;
    config.webUser = newWebUser;
  }
  if (server.hasArg("webPass")) {
    String newWebPass = server.arg("webPass");
    if (newWebPass.length() == 0) newWebPass = DEFAULT_WEB_PASS;
    config.webPass = newWebPass;
  }

  // 管理员 & 黑名单表单：只在字段存在时更新
  if (server.hasArg("adminPhone")) {
    config.adminPhone = server.arg("adminPhone");
  }
  if (server.hasArg("numberBlackList")) {
    config.numberBlackList = server.arg("numberBlackList");
  }
  if (server.hasArg("numberPrefixBlackList")) {
    config.numberPrefixBlackList = server.arg("numberPrefixBlackList");
  }
  if (server.hasArg("numberSuffixBlackList")) {
    config.numberSuffixBlackList = server.arg("numberSuffixBlackList");
  }
  if (server.hasArg("contentBlackList")) {
    config.contentBlackList = server.arg("contentBlackList");
  }

  // 选网配置：自动或按 PLMN 手动选择运营商。
  if (server.hasArg("operatorMode")) {
    uint8_t newMode = (uint8_t)server.arg("operatorMode").toInt();
    if (newMode > 1) newMode = 0;
    String newCode = server.arg("operatorCode");
    newCode.trim();

    bool validPlmn = newCode.length() == 5 || newCode.length() == 6;
    for (unsigned int i = 0; validPlmn && i < newCode.length(); i++) {
      validPlmn = isDigit(newCode.charAt(i));
    }
    if (newMode == 1 && !validPlmn) {
      sendResultPage(false, "选网配置无效", "手动选网需要填写 5 或 6 位数字 PLMN。", "/");
      return;
    }

    operatorConfigChanged = config.operatorMode != newMode ||
                            config.operatorCode != newCode;
    config.operatorMode = newMode;
    config.operatorCode = newCode;
  }

  // 推送通道配置：只在对应通道的字段存在时更新
  for (int i = 0; i < MAX_PUSH_CHANNELS; i++) {
    String idx = String(i);
    String enKey = "push" + idx + "en";
    String typeKey = "push" + idx + "type";
    String urlKey = "push" + idx + "url";
    String nameKey = "push" + idx + "name";
    String k1Key = "push" + idx + "key1";
    String k2Key = "push" + idx + "key2";
    String bodyKey = "push" + idx + "body";
    // 只要该通道的任一字段存在，就更新整个通道
    if (server.hasArg(enKey) || server.hasArg(typeKey) || server.hasArg(urlKey) ||
        server.hasArg(nameKey) || server.hasArg(k1Key) || server.hasArg(k2Key) ||
        server.hasArg(bodyKey)) {
      config.pushChannels[i].enabled = server.arg(enKey) == "on";
      config.pushChannels[i].type = (PushType)server.arg(typeKey).toInt();
      config.pushChannels[i].url = server.arg(urlKey);
      config.pushChannels[i].name = server.arg(nameKey);
      config.pushChannels[i].key1 = server.arg(k1Key);
      config.pushChannels[i].key2 = server.arg(k2Key);
      config.pushChannels[i].customBody = server.arg(bodyKey);
      if (config.pushChannels[i].name.length() == 0) {
        config.pushChannels[i].name = "通道" + String(i + 1);
      }
    }
  }
  
  saveConfig();
  configValid = isConfigValid();
  String saveMessage = operatorConfigChanged
                           ? "选网配置已保存，设备正在重启应用..."
                           : "3 秒后返回配置页面。如修改了账号或密码，请使用新凭据登录。";
  sendResultPage(true, "配置已保存", saveMessage, "/");

  if (operatorConfigChanged) {
    delay(800);
    ESP.restart();
  }
}

static String currentLocalTimestamp() {
  time_t now = time(nullptr);
  if (now < 100000) return "时间未同步";
  struct tm localTime;
  localtime_r(&now, &localTime);
  char buf[24];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &localTime);
  return String(buf);
}

// 转发历史查询与删除
void handleHistory() {
  if (!checkAuth()) return;

  if (server.method() == HTTP_POST) {
    String action = server.arg("action");
    String json = "{\"success\":false,\"message\":\"未知操作\"}";
    if (action == "delete") {
      uint32_t id = (uint32_t)strtoul(server.arg("id").c_str(), nullptr, 10);
      if (id > 0 && deleteForwardHistory(id)) {
        json = "{\"success\":true,\"message\":\"记录已删除\"}";
      } else {
        json = "{\"success\":false,\"message\":\"记录不存在\"}";
      }
    }
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", json);
    return;
  }

  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", forwardHistoryToJson());
}

// 使用当前表单参数测试单个推送通道，不保存配置
void handleTestPush() {
  if (!checkAuth()) return;

  int index = server.arg("index").toInt();
  if (index < 0 || index >= MAX_PUSH_CHANNELS) {
    server.send(400, "application/json", "{\"success\":false,\"message\":\"通道索引无效\"}");
    return;
  }

  PushChannel channel;
  channel.enabled = true;
  channel.type = (PushType)server.arg("type").toInt();
  channel.name = server.arg("name");
  channel.url = server.arg("url");
  channel.key1 = server.arg("key1");
  channel.key2 = server.arg("key2");
  channel.customBody = server.arg("body");
  if (channel.name.length() == 0) channel.name = "通道" + String(index + 1);

  if (!isPushChannelValid(channel)) {
    server.send(200, "application/json", "{\"success\":false,\"message\":\"通道配置不完整，请先填写必填项\"}");
    return;
  }

  webLongOperationActive = true;
  bool ok = sendToChannel(channel, "测试", "这是一条来自短信转发器的测试推送。",
                          currentLocalTimestamp().c_str(), true);
  webLongOperationActive = false;
  String json = "{\"success\":";
  json += ok ? "true" : "false";
  json += ",\"message\":\"";
  json += ok ? "测试推送已发送" : "测试推送失败，请查看系统日志";
  json += "\"}";
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", json);
}

// 切换推送网络：0=仅WiFi，1=仅4G数据，2=WiFi优先+4G兜底
void handlePushNetwork() {
  if (!checkAuth()) return;

  if (!server.hasArg("mode")) {
    server.send(400, "application/json", "{\"success\":false,\"message\":\"缺少网络模式\"}");
    return;
  }

  int mode = server.arg("mode").toInt();
  if (mode < PUSH_NET_WIFI || mode > PUSH_NET_WIFI_FALLBACK) {
    server.send(400, "application/json", "{\"success\":false,\"message\":\"网络模式无效\"}");
    return;
  }

  config.pushNetworkMode = (PushNetworkMode)mode;
  saveConfig();

  String message;
  if (mode == PUSH_NET_WIFI) {
    message = "已切换为仅 WiFi 推送";
  } else if (mode == PUSH_NET_CELLULAR) {
    message = "已切换为仅 4G 数据推送";
  } else {
    message = "已切换为 WiFi 优先，传输失败时使用 4G 数据";
  }

  String json = "{\"success\":true,\"mode\":" + String(mode) +
                ",\"message\":\"" + message + "\"}";
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", json);
}

// 处理日志查询请求 — 返回环形缓冲区中的日志行
void handleLog() {
  if (!checkAuth()) return;

  String json = "[";
  int total = logBufCount;
  int start = total < LOG_BUF_SIZE ? 0 : logBufIdx;
  for (int i = 0; i < total; i++) {
    int pos = (start + i) % LOG_BUF_SIZE;
    if (i > 0) json += ",";
    json += "\"" + jsonEscape(logBuffer[pos]) + "\"";
  }
  json += "]";
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.send(200, "application/json", json);
}

// 模组控制命令
void handleModem() {
  if (!checkAuth()) return;

  // 防止重入：modemInit() 内部会调 server.handleClient()，
  // 若浏览器超时重试会导致嵌套调用，最终拖垮 WiFi
  static bool busy = false;
  if (busy) {
    server.send(429, "application/json", "{\"success\":false,\"message\":\"模组正忙，请稍后重试\"}");
    return;
  }
  if (modemCommandBusy()) {
    server.send(429, "application/json", "{\"success\":false,\"message\":\"模组正在执行其他操作，请稍后重试\"}");
    return;
  }
  busy = true;

  String action = server.arg("action");
  String json = "{";
  bool success = false;
  String message = "";

  if (action == "restart") {
    // AT 软重启 — 先响应浏览器再初始化，防止浏览器超时重试
    logCaptureLn(String("网页端请求软重启模组..."));
    server.send(200, "application/json", "{\"success\":true,\"message\":\"正在软重启模组，请等待约 15 秒后刷新页面\"}");
    String resp = sendATCommand("AT+CFUN=1,1", 15000);
    success = (resp.indexOf("OK") >= 0);
    message = success ? "模组软重启成功" : "软重启失败";
    logCaptureLn(String(message + ": " + resp));
    if (success) modemInit();
    busy = false;
    return;
  }
  else if (action == "hardreset") {
    // EN 引脚断电重启（内部已调用 modemInit()）
    logCaptureLn(String("网页端请求硬重启模组..."));
    server.send(200, "application/json", "{\"success\":true,\"message\":\"正在硬重启模组，请等待约 15 秒后刷新页面\"}");
    resetModule();
    return;
  }
  else if (action == "signal") {
    logCaptureLn(String("网页端查询信号: AT+CSQ"));
    String resp = sendATCommand("AT+CSQ", 3000);
    int csqIdx = resp.indexOf("+CSQ:");
    if (csqIdx >= 0) {
      String csqLine = resp.substring(csqIdx);
      csqLine = csqLine.substring(0, csqLine.indexOf('\n'));
      csqLine.trim();
      int commaIdx = csqLine.indexOf(',');
      if (commaIdx >= 0) {
        int rssi = csqLine.substring(csqLine.indexOf(':') + 1, commaIdx).toInt();
        int ber = csqLine.substring(commaIdx + 1).toInt();
        int dbm = (rssi == 99) ? -999 : (-113 + rssi * 2);
        String quality;
        if (rssi >= 19) quality = "优秀";
        else if (rssi >= 14) quality = "良好";
        else if (rssi >= 10) quality = "一般";
        else if (rssi >= 5) quality = "较差";
        else quality = "很差";
        message = "RSRP: " + String(dbm) + " dBm (" + quality + "), RSSI: " + String(rssi) + ", BER: " + String(ber);
        success = true;
      }
    }
    if (!success) message = "无法获取信号: " + resp;
  }
  else if (action == "operator") {
    logCaptureLn(String("网页端查询运营商: AT+COPS?"));
    String resp = sendATCommand("AT+COPS?", 5000);
    int copsIdx = resp.indexOf("+COPS:");
    if (copsIdx >= 0) {
      String copsLine = resp.substring(copsIdx);
      copsLine = copsLine.substring(0, copsLine.indexOf('\n'));
      copsLine.trim();
      int q1 = copsLine.indexOf('"');
      int q2 = copsLine.indexOf('"', q1 + 1);
      if (q1 >= 0 && q2 >= 0) {
        message = copsLine.substring(q1 + 1, q2);
        success = true;
      } else {
        message = copsLine;
        success = true;
      }
    }
    if (!success) message = "无法获取运营商: " + resp;
  }
  else if (action == "imei") {
    logCaptureLn(String("网页端查询IMEI: AT+GSN"));
    String resp = sendATCommand("AT+GSN", 3000);
    resp.trim();
    int okIdx = resp.lastIndexOf("OK");
    if (okIdx > 0) resp = resp.substring(0, okIdx);
    int gsnIdx = resp.indexOf("AT+GSN");
    if (gsnIdx >= 0) resp = resp.substring(gsnIdx + 6);
    resp.trim();
    if (resp.length() > 0) {
      message = resp;
      success = true;
    } else {
      message = "无法获取 IMEI";
    }
  }
  else {
    message = "未知操作: " + action;
  }

  json += "\"success\":" + String(success ? "true" : "false") + ",";
  json += "\"message\":\"" + jsonEscape(message) + "\"";
  json += "}";
  busy = false;
  server.send(200, "application/json", json);
}

struct WifiScanEntry {
  String ssid;
  int32_t rssi;
  uint8_t auth;
  int32_t channel;
};

static String wifiStatusJson() {
  bool connected = WiFi.status() == WL_CONNECTED;
  String currentSsid = connected ? WiFi.SSID() : loadWifiSetting("ssid", WIFI_SSID);
  String savedSsid = loadWifiSetting("ssid", WIFI_SSID);

  String json = "{";
  json += "\"success\":true,\"connected\":" + String(connected ? "true" : "false");
  json += ",\"ssid\":\"" + jsonEscape(currentSsid) + "\"";
  json += ",\"ip\":\"" + jsonEscape(connected ? WiFi.localIP().toString() : String("")) + "\"";
  json += ",\"rssi\":" + String(connected ? WiFi.RSSI() : 0);
  json += ",\"savedSsid\":\"" + jsonEscape(savedSsid) + "\"";
  json += ",\"savedAuth\":" + String(loadWifiAuth());
  json += ",\"authText\":\"" + jsonEscape(wifiAuthChoiceText(loadWifiAuth())) + "\"";
  json += "}";
  return json;
}

static String wifiScanJson() {
  const int MAX_SCAN = 32;
  WifiScanEntry entries[MAX_SCAN];
  int count = 0;
  bool wasConnected = WiFi.status() == WL_CONNECTED;

  int found = WiFi.scanNetworks(false, true);
  if (found < 0) {
    WiFi.scanDelete();
    return "{\"success\":false,\"message\":\"WiFi 扫描失败\"}";
  }

  for (int i = 0; i < found; i++) {
    String ssid = WiFi.SSID(i);
    if (ssid.length() == 0) continue;

    bool merged = false;
    for (int j = 0; j < count; j++) {
      if (entries[j].ssid == ssid) {
        if (WiFi.RSSI(i) > entries[j].rssi) {
          entries[j].rssi = WiFi.RSSI(i);
          entries[j].auth = (uint8_t)WiFi.encryptionType(i);
          entries[j].channel = WiFi.channel(i);
        }
        merged = true;
        break;
      }
    }
    if (!merged && count < MAX_SCAN) {
      entries[count].ssid = ssid;
      entries[count].rssi = WiFi.RSSI(i);
      entries[count].auth = (uint8_t)WiFi.encryptionType(i);
      entries[count].channel = WiFi.channel(i);
      count++;
    }
  }
  WiFi.scanDelete();

  // 信号强的排在前面，方便直接选择。
  for (int i = 1; i < count; i++) {
    WifiScanEntry key = entries[i];
    int j = i - 1;
    while (j >= 0 && entries[j].rssi < key.rssi) {
      entries[j + 1] = entries[j];
      j--;
    }
    entries[j + 1] = key;
  }

  if (wasConnected && WiFi.status() != WL_CONNECTED) {
    WiFi.reconnect();
  }

  String json = "{\"success\":true,\"networks\":[";
  for (int i = 0; i < count; i++) {
    if (i > 0) json += ",";
    json += "{\"ssid\":\"" + jsonEscape(entries[i].ssid) + "\"";
    json += ",\"rssi\":" + String(entries[i].rssi);
    json += ",\"auth\":" + String(entries[i].auth);
    json += ",\"authText\":\"" + jsonEscape(wifiAuthScanText(entries[i].auth)) + "\"";
    json += ",\"channel\":" + String(entries[i].channel);
    json += "}";
  }
  json += "]}";
  return json;
}

static void connectWifiNetwork(const String& ssid, const String& pass, uint8_t auth) {
  logCaptureLn(String("正在连接 WiFi: ") + ssid);
  WiFi.disconnect(true);
  delay(400);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.setScanMethod(WIFI_FAST_SCAN);
  WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
  applyWifiSecurity(auth);

  if (auth == 1 || pass.length() == 0) {
    WiFi.begin(ssid.c_str());
  } else {
    WiFi.begin(ssid.c_str(), pass.c_str());
  }

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(100);
  }
  if (WiFi.status() == WL_CONNECTED) {
    logCaptureLn(String("WiFi 连接成功, IP: ") + WiFi.localIP().toString());
  } else {
    logCaptureLn(String("WiFi 连接失败，将在后台继续尝试"));
  }
}

// WiFi 扫描、保存、连接和状态查询
void handleWifi() {
  if (!checkAuth()) return;

  static bool busy = false;
  if (busy) {
    server.send(429, "application/json", "{\"success\":false,\"message\":\"WiFi 正忙，请稍后重试\"}");
    return;
  }
  busy = true;

  String action = server.arg("action");
  if (action == "status") {
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", wifiStatusJson());
    busy = false;
    return;
  }

  if (action == "scan") {
    String json = wifiScanJson();
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", json);
    busy = false;
    return;
  }

  if (action == "save" || action == "connect" || action == "restart") {
    String ssid;
    String pass;
    uint8_t auth;

    if (action == "restart") {
      ssid = loadWifiSetting("ssid", WIFI_SSID);
      pass = loadWifiSetting("pass", WIFI_PASS);
      auth = loadWifiAuth();
    } else {
      ssid = server.arg("ssid");
      ssid.trim();
      pass = server.arg("pass");
      auth = (uint8_t)server.arg("auth").toInt();

      if (ssid.length() == 0) {
        server.send(200, "application/json", "{\"success\":false,\"message\":\"请选择 WiFi 网络\"}");
        busy = false;
        return;
      }
      if (auth > 4) auth = 0;

      String savedSsid = loadWifiSetting("ssid", WIFI_SSID);
      String savedPass = loadWifiSetting("pass", WIFI_PASS);
      if (auth == 1) {
        pass = "";
      } else if (pass.length() == 0 && ssid == savedSsid) {
        pass = savedPass;
      }
      if (auth != 1 && pass.length() == 0) {
        server.send(200, "application/json", "{\"success\":false,\"message\":\"请输入 WiFi 密码\"}");
        busy = false;
        return;
      }
      saveWifiConnection(ssid, pass, auth);
    }

    if (action == "save") {
      server.send(200, "application/json", "{\"success\":true,\"message\":\"WiFi 连接信息已保存，重启后生效\"}");
      busy = false;
      return;
    }

    // 先响应浏览器，再切换连接；切换后设备 IP 可能变化。
    String message = action == "restart"
                         ? "WiFi 正在重新连接，请等待约 5 秒后刷新页面"
                         : "WiFi 连接信息已保存，正在连接；若 IP 变化，请使用新 IP 打开页面";
    server.send(200, "application/json", "{\"success\":true,\"message\":\"" + message + "\"}");
    delay(120);
    connectWifiNetwork(ssid, pass, auth);
    busy = false;
    return;
  }

  server.send(200, "application/json", "{\"success\":false,\"message\":\"未知操作\"}");
  busy = false;
}

static String apStatusJson() {
  bool enabled = loadApEnabled();
  bool running = (WiFi.getMode() & WIFI_AP) != 0;
  String ip = running ? WiFi.softAPIP().toString() : String("");
  int clients = running ? WiFi.softAPgetStationNum() : 0;
  String defaultSsid = defaultApSsid();

  String json = "{";
  json += "\"success\":true,\"enabled\":" + String(enabled ? "true" : "false");
  json += ",\"running\":" + String(running ? "true" : "false");
  json += ",\"ssid\":\"" + jsonEscape(loadApSsid()) + "\"";
  json += ",\"defaultSsid\":\"" + jsonEscape(defaultSsid) + "\"";
  json += ",\"ip\":\"" + jsonEscape(ip) + "\"";
  json += ",\"clients\":" + String(clients);
  json += ",\"passwordChanged\":" + String(loadApPasswordChanged() ? "true" : "false");
  json += ",\"encryption\":\"WPA2-AES\"";
  json += "}";
  return json;
}

// 热点配置：开关、SSID、WPA2 密码；配置保存后立即应用
void handleAp() {
  if (!checkAuth()) return;

  String action = server.arg("action");
  if (action == "status") {
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", apStatusJson());
    return;
  }

  if (action != "save") {
    if (action == "reset-prompt") {
      resetApPrompt();
      server.send(200, "application/json", "{\"success\":true,\"message\":\"首次登录提示已重置\"}");
      return;
    }
    server.send(200, "application/json", "{\"success\":false,\"message\":\"未知操作\"}");
    return;
  }

  bool enabled = server.hasArg("enabled") &&
                 (server.arg("enabled") == "on" || server.arg("enabled") == "1" ||
                  server.arg("enabled") == "true");
  String ssid = server.arg("ssid");
  ssid.trim();
  if (ssid.length() == 0) ssid = defaultApSsid();
  if (ssid.length() > 32) {
    server.send(200, "application/json", "{\"success\":false,\"message\":\"热点名称不能超过 32 字节\"}");
    return;
  }

  String pass = server.arg("pass");
  if (pass.length() == 0) pass = loadApPassword();
  if (pass.length() < 8 || pass.length() > 63) {
    server.send(200, "application/json", "{\"success\":false,\"message\":\"热点密码必须为 8-63 位\"}");
    return;
  }

  saveApConfig(enabled, ssid, pass);
  bool passwordChanged = loadApPasswordChanged();

  String json = "{\"success\":true,\"message\":\"热点配置已保存\",";
  json += "\"enabled\":";
  json += enabled ? "true" : "false";
  json += ",\"passwordChanged\":";
  json += passwordChanged ? "true" : "false";
  json += "}";

  // 先发送响应，再重启热点；如果当前是通过 AP 访问，连接会按预期断开。
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", json);
  delay(100);
  if (enabled) {
    startAccessPoint();
  } else {
    stopAccessPoint();
  }
}

// 系统控制命令
void handleSystem() {
  if (!checkAuth()) return;

  String action = server.arg("action");
  if (action == "restart") {
    // 整机重启 — 先响应浏览器再重启，防止浏览器超时重试
    logCaptureLn(String("网页端请求重启系统..."));
    server.send(200, "application/json", "{\"success\":true,\"message\":\"系统正在重启，请等待约 30 秒后刷新页面\"}");
    delay(500);  // 确保响应完整发出
    logCaptureLn(String("系统重启中..."));
    ESP.restart();
  } else {
    server.send(200, "application/json", "{\"success\":false,\"message\":\"未知操作\"}");
  }
}
