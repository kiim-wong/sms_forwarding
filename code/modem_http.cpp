#include "modem_http.h"
#include "modem.h"
#include "sms_process.h"
#include "web_handlers.h"

struct ParsedHttpUrl {
  bool valid;
  bool https;
  String origin;
  String path;
};

struct ModemHttpLockGuard {
  bool locked;
  ModemHttpLockGuard() : locked(modemLock()) {}
  ~ModemHttpLockGuard() {
    if (locked) modemUnlock();
  }
};

static bool parseHttpUrl(const String& url, ParsedHttpUrl& parsed) {
  parsed.valid = false;
  parsed.https = false;
  parsed.origin = "";
  parsed.path = "/";

  int schemeEnd = url.indexOf("://");
  if (schemeEnd <= 0) return false;

  String scheme = url.substring(0, schemeEnd);
  scheme.toLowerCase();
  if (scheme != "http" && scheme != "https") return false;

  int hostStart = schemeEnd + 3;
  int pathStart = url.indexOf('/', hostStart);
  int queryStart = url.indexOf('?', hostStart);
  if (pathStart < 0 || (queryStart >= 0 && queryStart < pathStart)) {
    pathStart = queryStart >= 0 ? queryStart : url.length();
  }

  String host = url.substring(hostStart, pathStart);
  host.trim();
  if (host.length() == 0 || host.indexOf('"') >= 0 || host.indexOf(' ') >= 0) return false;

  String path = pathStart < (int)url.length() ? url.substring(pathStart) : String("/");
  if (path.length() == 0) path = "/";
  if (!path.startsWith("/")) path = "/" + path;
  if (path.indexOf('"') >= 0) return false;

  parsed.valid = true;
  parsed.https = scheme == "https";
  parsed.origin = scheme + "://" + host;
  parsed.path = path;
  return true;
}

static bool containsFinalError(const String& response) {
  return response.indexOf("ERROR") >= 0;
}

static int parseCreatedHttpId(const String& response) {
  int marker = response.indexOf("+MHTTPCREATE:");
  if (marker < 0) return -1;

  int pos = marker + 12;
  while (pos < (int)response.length() && !isDigit(response.charAt(pos))) pos++;
  int start = pos;
  while (pos < (int)response.length() && isDigit(response.charAt(pos))) pos++;
  if (pos == start) return -1;
  return response.substring(start, pos).toInt();
}

static int splitHttpUrcFields(const String& line, String fields[], int maxFields) {
  int marker = line.indexOf(':');
  if (marker < 0) return 0;

  int pos = marker + 1;
  int count = 0;
  while (pos <= (int)line.length() && count < maxFields) {
    while (pos < (int)line.length() &&
           (line.charAt(pos) == ' ' || line.charAt(pos) == '\t')) {
      pos++;
    }
    if (pos >= (int)line.length()) break;

    String value;
    if (line.charAt(pos) == '"') {
      pos++;
      int end = line.indexOf('"', pos);
      if (end < 0) end = line.length();
      value = line.substring(pos, end);
      pos = end < (int)line.length() ? end + 1 : end;
    } else {
      int end = line.indexOf(',', pos);
      if (end < 0) end = line.length();
      value = line.substring(pos, end);
      value.trim();
      pos = end;
    }
    fields[count++] = value;
    if (pos < (int)line.length() && line.charAt(pos) == ',') pos++;
  }
  return count;
}

static bool waitForContentPrompt(unsigned long timeoutMs) {
  String line;
  line.reserve(256);
  unsigned long start = millis();

  while (millis() - start < timeoutMs) {
    while (Serial1.available()) {
      char c = (char)Serial1.read();
      if (c == '>') return true;
      if (c == '\n') {
        line.trim();
        if (line.length() > 0 && !handleSmsUrcLine(line)) {
          if (line == "ERROR" || line.startsWith("+CME ERROR") ||
              line.startsWith("+CMS ERROR")) {
            logCaptureLn(String("[HTTP-4G] 设置请求体失败: ") + line);
            return false;
          }
        }
        line = "";
      } else if (c != '\r') {
        if (line.length() < 512) line += c;
      }
    }
    if (!webLongOperationActive) server.handleClient();
    delay(1);
  }

  logCaptureLn(String("[HTTP-4G] 等待请求体输入提示超时"));
  return false;
}

static bool waitForFinalResult(unsigned long timeoutMs, String* captured = nullptr) {
  String response;
  unsigned long start = millis();

  while (millis() - start < timeoutMs) {
    String line = readSerialLine(Serial1);
    if (line.length() > 0) {
      line.trim();
      if (handleSmsUrcLine(line)) continue;
      if (captured) {
        *captured += line;
        *captured += "\r\n";
      }
      if (line == "OK") return true;
      if (line == "ERROR" || line.startsWith("+CME ERROR") ||
          line.startsWith("+CMS ERROR")) {
        logCaptureLn(String("[HTTP-4G] 模组返回错误: ") + line);
        return false;
      }
    } else {
      if (!webLongOperationActive) server.handleClient();
      delay(1);
    }
  }
  return false;
}

static bool readHttpCachePart(int httpId, int type, int length,
                              String& output, String& error) {
  output = "";
  if (length <= 0) return true;

  int remaining = length;
  const int MAX_CHUNK = 2048;
  while (remaining > 0 && output.length() < 4096) {
    int readLength = remaining > MAX_CHUNK ? MAX_CHUNK : remaining;
    String cmd = "AT+MHTTPREAD=" + String(httpId) + "," + String(type) + "," +
                 String(readLength);
    String response = sendATCommandLocked(cmd.c_str(), 6000);
    if (response.indexOf("+MHTTPREAD:") < 0 || response.indexOf("OK") < 0) {
      error = "读取 HTTP 缓存失败";
      return false;
    }

    int marker = response.indexOf("+MHTTPREAD:");
    int pos = response.indexOf(':', marker) + 1;
    int numericFields = 0;

    // 不同 ML307 固件在带 read_len 时可能返回 3 个或 4 个数字字段：
    // <id>,<type>,<unread>[,<data_len>],<data>。按数字字段识别正文起点，
    // 不能把正文里的逗号当作字段分隔符。
    while (pos < (int)response.length() && numericFields < 4) {
      while (pos < (int)response.length() &&
             (response.charAt(pos) == ' ' || response.charAt(pos) == '\r' ||
              response.charAt(pos) == '\n' || response.charAt(pos) == '\t')) {
        pos++;
      }
      int comma = response.indexOf(',', pos);
      if (comma < 0) break;
      String token = response.substring(pos, comma);
      token.trim();
      bool numeric = token.length() > 0;
      for (unsigned int i = 0; i < token.length(); i++) {
        if (!isDigit(token.charAt(i))) {
          numeric = false;
          break;
        }
      }
      if (!numeric) break;
      numericFields++;
      pos = comma + 1;
    }

    if (numericFields < 3) {
      error = "HTTP 缓存响应格式异常";
      String sample = response.substring(0, response.length() > 240 ? 240 : response.length());
      logCaptureLn(String("[HTTP-4G] 缓存响应: ") + sample);
      return false;
    }

    String payload = response.substring(pos);
    int finalMarker = payload.lastIndexOf("\r\nOK");
    if (finalMarker >= 0) payload = payload.substring(0, finalMarker);
    if (payload.length() > (unsigned int)readLength) {
      payload = payload.substring(0, readLength);
    }
    if (payload.length() == 0) break;

    output += payload;
    remaining -= payload.length();
  }
  return output.length() > 0 || length == 0;
}

static bool waitForHttpReceive(int httpId, ModemHttpResponse& response,
                               unsigned long timeoutMs) {
  unsigned long start = millis();
  while (millis() - start < timeoutMs) {
    String line = readSerialLine(Serial1);
    if (line.length() == 0) {
      if (!webLongOperationActive) server.handleClient();
      delay(1);
      continue;
    }

    line.trim();
    if (handleSmsUrcLine(line)) continue;

    if (line == "ERROR" || line.startsWith("+CME ERROR") ||
        line.startsWith("+CMS ERROR")) {
      response.error = "4G HTTP 请求失败: " + line;
      return false;
    }

    if (line.indexOf("+MHTTPURC:") < 0 || line.indexOf("\"recv\"") < 0) {
      continue;
    }

    String fields[6];
    int count = splitHttpUrcFields(line, fields, 6);
    if (count < 5 || fields[1].toInt() != httpId) continue;

    response.gotResponse = true;
    response.statusCode = fields[2].toInt();
    int headerLength = fields[3].toInt();
    int contentLength = fields[4].toInt();

    String readError;
    if (headerLength > 0) {
      if (!readHttpCachePart(httpId, 0, headerLength, response.headers, readError)) {
        response.error = readError;
        return false;
      }
    }
    if (contentLength > 0) {
      if (!readHttpCachePart(httpId, 1, contentLength, response.body, readError)) {
        response.error = readError;
        return false;
      }
    }
    return true;
  }

  response.error = "等待 4G HTTP 响应超时";
  return false;
}

bool modemDataBegin(bool& activatedByUs) {
  activatedByUs = false;
  String state = sendATCommand("AT+CGACT?", 2500);
  if (state.indexOf("+CGACT: 1,1") >= 0) {
    // 模组或网络可能已经自动拉起 cid=1。推送结束后仍尝试关闭，避免长期占流量。
    activatedByUs = true;
    logCaptureLn(String("[NET] 4G 数据已处于激活状态，推送结束后请求关闭"));
    return true;
  }

  logCaptureLn(String("[NET] 激活 4G 数据上下文..."));
  String activate = sendATCommand("AT+CGACT=1,1", 15000);
  if (activate.indexOf("OK") < 0) {
    logCaptureLn(String("[NET] 4G 数据激活失败: ") + activate);
    return false;
  }

  bool confirmed = false;
  for (int retry = 0; retry < 20; retry++) {
    String stateNow = sendATCommand("AT+CGACT?", 2000);
    if (stateNow.indexOf("+CGACT: 1,1") >= 0) {
      confirmed = true;
      break;
    }
    delay(250);
  }
  if (!confirmed) {
    logCaptureLn(String("[NET] 4G 数据上下文未能确认激活"));
    sendATandWaitOK("AT+CGACT=0,1", 5000);
    return false;
  }

  activatedByUs = true;
  logCaptureLn(String("[NET] 4G 数据已激活，本次推送结束后请求关闭"));
  delay(300);
  return true;
}

void modemDataEnd(bool activatedByUs) {
  if (!activatedByUs) return;
  logCaptureLn(String("[NET] 关闭本次推送使用的 4G 数据连接"));
  // 某些异常路径下实例未完全释放，会继续占用 cid=1；先清空本应用可能使用的实例。
  for (int httpId = 0; httpId < 4; httpId++) {
    sendATCommand(("AT+MHTTPDEL=" + String(httpId)).c_str(), 1500);
  }
  bool deactivated = false;
  for (int attempt = 0; attempt < 3 && !deactivated; attempt++) {
    if (attempt > 0) delay(1000);
    String response = sendATCommand("AT+CGACT=0,1", 15000);
    logCaptureLn(String("[NET] CGACT关闭响应: ") + response);

    for (int check = 0; check < 6; check++) {
      String state = sendATCommand("AT+CGACT?", 2000);
      if (state.length() > 0 && state.indexOf("+CGACT: 1,1") < 0) {
        deactivated = true;
        break;
      }
      delay(500);
    }
  }
  if (!deactivated) {
    logCaptureLn(String("[!] 4G 数据上下文 1 仍可能处于激活状态（模组可能自动保持）"));
  } else {
    logCaptureLn(String("[NET] 4G 数据关闭请求已完成（模组可能自动恢复）"));
  }
}

bool modemHttpRequest(const String& url, bool isGet, const String& contentType,
                      const String& body, ModemHttpResponse& response) {
  response = {};
  response.statusCode = 0;

  ParsedHttpUrl parsed;
  if (!parseHttpUrl(url, parsed)) {
    response.error = "URL 格式无效";
    return false;
  }

  ModemHttpLockGuard lock;
  if (!lock.locked) {
    response.error = "模组正忙，无法发起 4G 推送";
    return false;
  }

  // 清理上一次异常退出可能留下的实例，确保每次请求从干净状态开始。
  sendATCommandLocked("AT+MHTTPDEL=0", 2500);
  sendATCommandLocked("AT+MHTTPDEL=1", 2500);
  sendATCommandLocked("AT+MHTTPDEL=2", 2500);
  sendATCommandLocked("AT+MHTTPDEL=3", 2500);

  String createCmd = "AT+MHTTPCREATE=\"" + parsed.origin + "\"";
  String createResponse = sendATCommandLocked(createCmd.c_str(), 10000);
  int httpId = parseCreatedHttpId(createResponse);
  if (httpId < 0) {
    response.error = "创建 4G HTTP 实例失败";
    logCaptureLn(String("[HTTP-4G] ") + response.error + ": " + createResponse);
    return false;
  }

  String setupCmd = "AT+MHTTPCFG=\"cached\"," + String(httpId) + ",1,4096";
  String setupResponse = sendATCommandLocked(setupCmd.c_str(), 3000);
  if (setupResponse.indexOf("OK") < 0) {
    response.error = "启用 4G HTTP 缓存失败";
    sendATCommandLocked(("AT+MHTTPDEL=" + String(httpId)).c_str(), 2500);
    return false;
  }

  setupCmd = "AT+MHTTPCFG=\"cid\"," + String(httpId) + ",1";
  setupResponse = sendATCommandLocked(setupCmd.c_str(), 3000);
  if (setupResponse.indexOf("OK") < 0) {
    response.error = "绑定 4G PDP 上下文失败";
    sendATCommandLocked(("AT+MHTTPDEL=" + String(httpId)).c_str(), 2500);
    return false;
  }

  setupCmd = "AT+MHTTPCFG=\"timeout\"," + String(httpId) + ",20,20,20";
  setupResponse = sendATCommandLocked(setupCmd.c_str(), 3000);
  if (setupResponse.indexOf("OK") < 0) {
    response.error = "配置 4G HTTP 超时失败";
    sendATCommandLocked(("AT+MHTTPDEL=" + String(httpId)).c_str(), 2500);
    return false;
  }

  if (parsed.https) {
    sendATCommandLocked("AT+MSSLCFG=\"auth\",0,0", 2500);
    setupCmd = "AT+MHTTPCFG=\"ssl\"," + String(httpId) + ",1,0";
  } else {
    setupCmd = "AT+MHTTPCFG=\"ssl\"," + String(httpId) + ",0";
  }
  setupResponse = sendATCommandLocked(setupCmd.c_str(), 3000);
  if (setupResponse.indexOf("OK") < 0) {
    response.error = "配置 4G HTTPS 失败";
    sendATCommandLocked(("AT+MHTTPDEL=" + String(httpId)).c_str(), 2500);
    return false;
  }

  setupCmd = "AT+MHTTPCFG=\"header\"," + String(httpId) + ",\"Connection: close\"";
  sendATCommandLocked(setupCmd.c_str(), 3000);

  if (!isGet) {
    setupCmd = "AT+MHTTPHEADER=" + String(httpId) + ",0,0,\"Content-Type: " +
               contentType + "\"";
    setupResponse = sendATCommandLocked(setupCmd.c_str(), 3000);
    if (setupResponse.indexOf("OK") < 0) {
      response.error = "设置 4G HTTP 请求头失败";
      sendATCommandLocked(("AT+MHTTPDEL=" + String(httpId)).c_str(), 2500);
      return false;
    }

    String contentCmd = "AT+MHTTPCONTENT=" + String(httpId) + ",0," +
                        String(body.length());
    Serial1.println(contentCmd);
    if (!waitForContentPrompt(6000)) {
      response.error = "4G HTTP 请求体输入超时";
      sendATCommandLocked(("AT+MHTTPDEL=" + String(httpId)).c_str(), 2500);
      return false;
    }
    Serial1.write((const uint8_t*)body.c_str(), body.length());
    if (!waitForFinalResult(10000)) {
      response.error = "4G HTTP 请求体发送失败";
      sendATCommandLocked(("AT+MHTTPDEL=" + String(httpId)).c_str(), 2500);
      return false;
    }
  }

  String requestCmd = "AT+MHTTPREQUEST=" + String(httpId) + "," +
                      (isGet ? "1" : "2") + ",0,\"" + parsed.path + "\"";
  String requestResponse = sendATCommandLocked(requestCmd.c_str(), 10000);
  if (requestResponse.indexOf("OK") < 0 || containsFinalError(requestResponse)) {
    response.error = "4G HTTP 请求发送失败";
    logCaptureLn(String("[HTTP-4G] ") + requestResponse);
    sendATCommandLocked(("AT+MHTTPDEL=" + String(httpId)).c_str(), 2500);
    return false;
  }
  response.requestSent = true;

  bool received = false;
  if (requestResponse.indexOf("\"recv\"") >= 0) {
    // 极快的响应可能在 REQUEST 的结束码之前到达。
    int recvPos = requestResponse.indexOf("+MHTTPURC:");
    if (recvPos >= 0) {
      String recvLine = requestResponse.substring(recvPos);
      int lineEnd = recvLine.indexOf('\n');
      if (lineEnd >= 0) recvLine = recvLine.substring(0, lineEnd);
      String fields[6];
      int count = splitHttpUrcFields(recvLine, fields, 6);
      if (count >= 5 && fields[1].toInt() == httpId) {
        response.gotResponse = true;
        response.statusCode = fields[2].toInt();
        int headerLength = fields[3].toInt();
        int contentLength = fields[4].toInt();
        String readError;
        received = true;
        if (headerLength > 0 &&
            !readHttpCachePart(httpId, 0, headerLength, response.headers, readError)) {
          response.error = readError;
          received = false;
        }
        if (received && contentLength > 0 &&
            !readHttpCachePart(httpId, 1, contentLength, response.body, readError)) {
          response.error = readError;
          received = false;
        }
      }
    }
  }

  if (!received) {
    received = waitForHttpReceive(httpId, response, 30000);
  }

  if (!received) {
    sendATCommandLocked(("AT+MHTTPTERM=" + String(httpId)).c_str(), 2500);
  }
  sendATCommandLocked(("AT+MHTTPDEL=" + String(httpId)).c_str(), 2500);
  return received;
}
