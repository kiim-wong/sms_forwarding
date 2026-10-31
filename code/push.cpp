#include "push.h"
#include "web_handlers.h"
#include "config.h"
#include "web_handlers.h"
#include "modem_http.h"
#include <HTTPClient.h>
#include <mbedtls/md.h>
#include <base64.h>
#include <sys/time.h>

// URL编码辅助函数
String urlEncode(const String& str) {
  String encoded = "";
  char c;
  char code0;
  char code1;
  for (unsigned int i = 0; i < str.length(); i++) {
    c = str.charAt(i);
    if (c == ' ') {
      encoded += '+';
    } else if (isalnum(c)) {
      encoded += c;
    } else {
      code1 = (c & 0xf) + '0';
      if ((c & 0xf) > 9) code1 = (c & 0xf) - 10 + 'A';
      c = (c >> 4) & 0xf;
      code0 = c + '0';
      if (c > 9) code0 = c - 10 + 'A';
      encoded += '%';
      encoded += code0;
      encoded += code1;
    }
  }
  return encoded;
}

// 钉钉签名函数（时间戳为UTC毫秒级）
String dingtalkSign(const String& secret, int64_t timestamp) {
  String stringToSign = String(timestamp) + "\n" + secret;
  
  uint8_t hmacResult[32];
  mbedtls_md_context_t ctx;
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
  mbedtls_md_hmac_starts(&ctx, (const unsigned char*)secret.c_str(), secret.length());
  mbedtls_md_hmac_update(&ctx, (const unsigned char*)stringToSign.c_str(), stringToSign.length());
  mbedtls_md_hmac_finish(&ctx, hmacResult);
  mbedtls_md_free(&ctx);
  
  String base64Encoded = base64::encode(hmacResult, 32);
  return urlEncode(base64Encoded);
}

// 获取当前UTC毫秒级时间戳（用于钉钉签名）
int64_t getUtcMillis() {
  struct timeval tv;
  if (gettimeofday(&tv, NULL) == 0) {
    return (int64_t)tv.tv_sec * 1000LL + tv.tv_usec / 1000;
  }
  // 如果获取失败，使用time()函数
  return (int64_t)time(nullptr) * 1000LL;
}

// JSON转义函数
String jsonEscape(const String& str) {
  String result = "";
  result.reserve(str.length() + 16);
  for (unsigned int i = 0; i < str.length(); i++) {
    char c = str.charAt(i);
    if (c == '"') result += "\\\"";
    else if (c == '\\') result += "\\\\";
    else if (c == '\n') result += "\\n";
    else if (c == '\r') result += "\\r";
    else if (c == '\t') result += "\\t";
    else result += c;
  }
  return result;
}

// 将 pdulib 的 YYMMDDHHMMSS(+时区) 时间戳转换为人类可读格式。
// 例：26103118103632 -> 2026-10-31 18:10:36
String formatSmsTimestamp(const String& rawTimestamp) {
  String raw = rawTimestamp;
  raw.trim();
  if (raw.length() == 0) return "时间未知";

  // 已经是格式化时间（例如推送测试使用的 currentLocalTimestamp()）则直接返回。
  if (raw.indexOf('-') >= 0 || raw.indexOf(':') >= 0) return raw;

  String digits;
  digits.reserve(raw.length());
  for (unsigned int i = 0; i < raw.length(); i++) {
    char c = raw.charAt(i);
    if (c >= '0' && c <= '9') digits += c;
  }
  if (digits.length() < 12) return raw;

  int year = 2000 + digits.substring(0, 2).toInt();
  int month = digits.substring(2, 4).toInt();
  int day = digits.substring(4, 6).toInt();
  int hour = digits.substring(6, 8).toInt();
  int minute = digits.substring(8, 10).toInt();
  int second = digits.substring(10, 12).toInt();

  if (month < 1 || month > 12 || day < 1 || day > 31 ||
      hour > 23 || minute > 59 || second > 59) {
    return raw;
  }

  char buf[24];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
           year, month, day, hour, minute, second);
  return String(buf);
}

// 判断业务响应体是否表示成功
// 部分平台无论成败 HTTP 都返回 200，错误信息在响应体中，需要额外校验
static bool isBodySuccess(const PushChannel& channel, const String& resp) {
  switch (channel.type) {
    case PUSH_TYPE_DINGTALK:   return resp.indexOf("\"errcode\":0") >= 0;
    case PUSH_TYPE_FEISHU:     return resp.indexOf("\"code\":0") >= 0;
    case PUSH_TYPE_PUSHPLUS:   return resp.indexOf("\"code\":200") >= 0;
    case PUSH_TYPE_SERVERCHAN: return resp.indexOf("\"code\":0") >= 0;
    case PUSH_TYPE_TELEGRAM:   return resp.indexOf("\"ok\":true") >= 0;
    case PUSH_TYPE_WECOM:      return resp.indexOf("\"errcode\":0") >= 0;
    default:                   return true; // 其余平台以 HTTP 状态码为准
  }
}

struct PushRuntime {
  PushNetworkMode mode;
  bool wifiUsable = false;
  bool cellularReady = false;
  bool cellularActivatedByUs = false;
  bool networkAvailable = false;

  explicit PushRuntime(PushNetworkMode selectedMode) : mode(selectedMode) {}
  ~PushRuntime() {
    if (cellularReady) modemDataEnd(cellularActivatedByUs);
  }
};

struct HttpAttemptResult {
  bool success = false;
  bool gotHttpResponse = false;
  bool transportFailure = false;
  bool fallbackEligible = false;
  bool retryable = true;
  int httpCode = 0;
  String response;
  String error;
};

static bool ensureCellular(PushRuntime& runtime) {
  if (runtime.cellularReady) return true;

  bool activatedByUs = false;
  if (!modemDataBegin(activatedByUs)) {
    runtime.cellularReady = false;
    return false;
  }
  runtime.cellularReady = true;
  runtime.cellularActivatedByUs = activatedByUs;
  runtime.networkAvailable = true;
  return true;
}

static bool preparePushRoute(PushRuntime& runtime) {
  if (runtime.mode == PUSH_NET_WIFI) {
    runtime.wifiUsable = WiFi.status() == WL_CONNECTED;
    runtime.networkAvailable = runtime.wifiUsable;
    if (!runtime.wifiUsable) logCaptureLn(String("[NET] WiFi 未连接，WiFi 推送不可用"));
    return runtime.wifiUsable;
  }

  if (runtime.mode == PUSH_NET_CELLULAR) {
    if (!ensureCellular(runtime)) {
      logCaptureLn(String("[NET] 4G 数据不可用，推送已取消"));
      return false;
    }
    return true;
  }

  runtime.wifiUsable = WiFi.status() == WL_CONNECTED;
  if (runtime.wifiUsable) {
    runtime.networkAvailable = true;
    logCaptureLn(String("[NET] 推送网络：WiFi 优先"));
    return true;
  }

  logCaptureLn(String("[NET] WiFi 未连接，推送切换到 4G 数据"));
  return ensureCellular(runtime);
}

static HttpAttemptResult executeWifiRequest(const PushChannel& channel, const String& url,
                                            bool isGet, const String& contentType,
                                            const String& body) {
  HttpAttemptResult result;
  HTTPClient http;
  http.setTimeout(8000);
  if (!http.begin(url)) {
    result.transportFailure = true;
    result.fallbackEligible = true;
    result.error = "HTTP 初始化失败";
    return result;
  }

  if (!isGet) http.addHeader("Content-Type", contentType);
  int httpCode = isGet ? http.GET() : http.POST(body);
  result.httpCode = httpCode;

  if (httpCode <= 0) {
    result.transportFailure = true;
    // 读超时不能兜底：请求可能已经送达服务器，再发一次会造成重复通知。
    result.fallbackEligible = httpCode == HTTPC_ERROR_CONNECTION_REFUSED ||
                              httpCode == HTTPC_ERROR_SEND_HEADER_FAILED ||
                              httpCode == HTTPC_ERROR_NOT_CONNECTED ||
                              httpCode == HTTPC_ERROR_NO_STREAM ||
                              httpCode == HTTPC_ERROR_NO_HTTP_SERVER;
    result.retryable = httpCode != HTTPC_ERROR_READ_TIMEOUT;
    result.error = http.errorToString(httpCode);
    http.end();
    return result;
  }

  result.gotHttpResponse = true;
  result.response = http.getString();
  bool httpOk = httpCode >= 200 && httpCode < 300;
  result.success = httpOk && isBodySuccess(channel, result.response);
  http.end();
  return result;
}

static HttpAttemptResult executeCellularRequest(const PushChannel& channel, const String& url,
                                                bool isGet, const String& contentType,
                                                const String& body) {
  HttpAttemptResult result;
  ModemHttpResponse modemResponse;
  if (!modemHttpRequest(url, isGet, contentType, body, modemResponse)) {
    result.transportFailure = true;
    result.retryable = !modemResponse.requestSent;
    result.error = modemResponse.error;
    return result;
  }

  result.gotHttpResponse = true;
  result.httpCode = modemResponse.statusCode;
  result.response = modemResponse.body;
  bool httpOk = result.httpCode >= 200 && result.httpCode < 300;
  result.success = httpOk && isBodySuccess(channel, result.response);
  return result;
}

// 带重试地执行单个通道的 HTTP 请求。WiFi 优先模式下，只有传输层失败
// 才会切换到 4G；收到 4xx/5xx 或业务错误不会重发，避免重复通知。
static bool executeChannelRequest(const PushChannel& channel, const String& url,
                                  bool isGet, const String& contentType, const String& body,
                                  const String& channelName, int maxAttempts,
                                  PushRuntime& runtime) {
  const int MAX_ATTEMPTS = maxAttempts > 0 ? maxAttempts : 1;
  for (int attempt = 1; attempt <= MAX_ATTEMPTS; attempt++) {
    if (attempt > 1) {
      delay(500 * (attempt - 1));
      logCaptureF("[%s] 重试 (%d/%d)...\n", channelName.c_str(), attempt, MAX_ATTEMPTS);
    }

    bool useCellular = runtime.mode == PUSH_NET_CELLULAR;
    if (runtime.mode == PUSH_NET_WIFI_FALLBACK) {
      if (runtime.wifiUsable && WiFi.status() != WL_CONNECTED) {
        logCaptureLn(String("[NET] WiFi 已断开，切换到 4G 数据"));
        runtime.wifiUsable = false;
      }
      useCellular = !runtime.wifiUsable;
    }

    if (runtime.mode == PUSH_NET_WIFI && WiFi.status() != WL_CONNECTED) {
      runtime.networkAvailable = false;
      logCaptureLn(String("[NET] WiFi 未连接，取消推送"));
      return false;
    }

    HttpAttemptResult result;
    if (!useCellular) {
      logCaptureF("[%s] 通过 WiFi 推送...\n", channelName.c_str());
      result = executeWifiRequest(channel, url, isGet, contentType, body);
      if (result.fallbackEligible && runtime.mode == PUSH_NET_WIFI_FALLBACK) {
        logCaptureF("[%s] WiFi 传输失败（%s），切换 4G 数据\n",
                    channelName.c_str(), result.error.c_str());
        runtime.wifiUsable = false;
        useCellular = true;
      }
    }

    if (useCellular) {
      if (!ensureCellular(runtime)) {
        logCaptureLn(String("[NET] 4G 数据不可用"));
      } else {
        logCaptureF("[%s] 通过 4G 数据推送...\n", channelName.c_str());
        result = executeCellularRequest(channel, url, isGet, contentType, body);
      }
    }

    if (result.transportFailure) {
      logCaptureF("[%s] HTTP请求失败: %s\n", channelName.c_str(), result.error.c_str());
      if (!result.retryable) {
        logCaptureLn(String("[" + channelName + "] 请求可能已送达，跳过重试以防重复通知"));
        return false;
      }
    } else if (result.gotHttpResponse) {
      logCaptureF("[%s] 响应码: %d\n", channelName.c_str(), result.httpCode);
      if (result.response.length() > 0) logCaptureLn(String("响应: " + result.response));
      if (result.success) {
        logCaptureF("[%s] 推送成功（第 %d/%d 次尝试）\n",
                    channelName.c_str(), attempt, MAX_ATTEMPTS);
        return true;
      }
      if (result.httpCode >= 400 && result.httpCode < 500 && result.httpCode != 429) {
        logCaptureF("[%s] 客户端错误 %d，跳过重试\n", channelName.c_str(), result.httpCode);
        return false;
      }
      logCaptureLn(String("[" + channelName + "] 本次推送未确认成功"));
    } else {
      logCaptureLn(String("[" + channelName + "] 本次推送没有收到 HTTP 响应"));
    }
  }

  logCaptureLn(String("[" + channelName + "] 多次重试后仍失败"));
  return false;
}

// 发送单个推送通道（统一构建请求参数，由 executeChannelRequest 执行并自动重试）
static bool sendToChannelOnRuntime(const PushChannel& channel, const char* sender,
                                   const char* message, const char* timestamp,
                                   bool testMode, PushRuntime& runtime) {
  if (!channel.enabled) return false;

  // 对于某些推送方式，URL可以为空（使用默认URL）
  bool needUrl = (channel.type == PUSH_TYPE_POST_JSON || channel.type == PUSH_TYPE_BARK ||
                  channel.type == PUSH_TYPE_GET || channel.type == PUSH_TYPE_DINGTALK ||
                  channel.type == PUSH_TYPE_CUSTOM || channel.type == PUSH_TYPE_WECOM);
  if (needUrl && channel.url.length() == 0) return false;

  String channelName = channel.name.length() > 0 ? channel.name : ("通道" + String(channel.type));
  logCaptureLn(String("发送到推送通道: " + channelName));

  String senderEscaped = jsonEscape(String(sender));
  String messageEscaped = jsonEscape(String(message));
  String timestampEscaped = jsonEscape(String(timestamp));

  String reqUrl = "";
  String reqBody = "";
  String reqContentType = "application/json";
  bool useGet = false;

  switch (channel.type) {
    case PUSH_TYPE_POST_JSON: {
      // 标准POST JSON格式
      reqUrl = channel.url;
      reqBody = "{";
      reqBody += "\"sender\":\"" + senderEscaped + "\",";
      reqBody += "\"message\":\"" + messageEscaped + "\",";
      reqBody += "\"timestamp\":\"" + timestampEscaped + "\"";
      reqBody += "}";
      logCaptureLn(String("POST JSON: " + reqBody));
      break;
    }

    case PUSH_TYPE_BARK: {
      // Bark推送格式
      reqUrl = channel.url;
      reqBody = "{";
      reqBody += "\"title\":\"" + senderEscaped + "\",";
      reqBody += "\"body\":\"" + messageEscaped + "\"";
      reqBody += "}";
      logCaptureLn(String("BARK JSON: " + reqBody));
      break;
    }

    case PUSH_TYPE_GET: {
      // GET请求，参数放URL里
      String getUrl = channel.url;
      if (getUrl.indexOf('?') == -1) {
        getUrl += "?";
      } else {
        getUrl += "&";
      }
      getUrl += "sender=" + urlEncode(String(sender));
      getUrl += "&message=" + urlEncode(String(message));
      getUrl += "&timestamp=" + urlEncode(String(timestamp));
      logCaptureLn(String("GET URL: " + getUrl));
      reqUrl = getUrl;
      useGet = true;
      break;
    }

    case PUSH_TYPE_DINGTALK: {
      // 钉钉机器人（如果配置了secret，需要添加签名）
      String webhookUrl = channel.url;
      if (channel.key1.length() > 0) {
        // 获取UTC毫秒级时间戳（钉钉要求）
        int64_t ts = getUtcMillis();
        String sign = dingtalkSign(channel.key1, ts);
        if (webhookUrl.indexOf('?') == -1) {
          webhookUrl += "?";
        } else {
          webhookUrl += "&";
        }
        // 使用字符串拼接避免int64_t转换问题
        char tsBuf[21];
        snprintf(tsBuf, sizeof(tsBuf), "%lld", ts);
        webhookUrl += "timestamp=" + String(tsBuf) + "&sign=" + sign;
      }
      reqUrl = webhookUrl;
      reqBody = "{\"msgtype\":\"text\",\"text\":{\"content\":\"";
      reqBody += "[SMS] 短信通知\\n发送者: " + senderEscaped + "\\n内容: " + messageEscaped + "\\n时间: " + timestampEscaped;
      reqBody += "\"}}";
      logCaptureLn(String("钉钉: " + reqBody));
      break;
    }

    case PUSH_TYPE_PUSHPLUS: {
      // PushPlus
      reqUrl = channel.url.length() > 0 ? channel.url : "http://www.pushplus.plus/send";
      // 发送渠道
      String channelValue = "wechat";
      if (channel.key2.length() > 0) {
          // 仅支持微信公众号（wechat）、浏览器插件（extension）和 PushPlus App（app）三种渠道
          if (channel.key2 == "wechat" || channel.key2 == "extension" || channel.key2 == "app") {
              channelValue = channel.key2;
          } else {
              logCaptureLn(String("Invalid PushPlus channel '" + channel.key2 + "'. Using default 'wechat'."));
          }
      }
      reqBody = "{";
      reqBody += "\"token\":\"" + channel.key1 + "\",";
      reqBody += "\"title\":\"短信来自: " + senderEscaped + "\",";
      reqBody += "\"content\":\"<b>发送者:</b> " + senderEscaped + "<br><b>时间:</b> " + timestampEscaped + "<br><b>内容:</b><br>" + messageEscaped + "\",";
      reqBody += "\"channel\":\"" + channelValue + "\"";
      reqBody += "}";
      logCaptureLn(String("PushPlus: " + reqBody));
      break;
    }

    case PUSH_TYPE_SERVERCHAN: {
      // Server酱
      reqUrl = channel.url.length() > 0 ? channel.url : ("https://sctapi.ftqq.com/" + channel.key1 + ".send");
      reqContentType = "application/x-www-form-urlencoded";
      reqBody = "title=" + urlEncode("短信来自: " + String(sender));
      reqBody += "&desp=" + urlEncode("**发送者:** " + String(sender) + "\n\n**时间:** " + String(timestamp) + "\n\n**内容:**\n\n" + String(message));
      logCaptureLn(String("Server酱: " + reqBody));
      break;
    }

    case PUSH_TYPE_CUSTOM: {
      // 自定义模板
      if (channel.customBody.length() == 0) {
        logCaptureLn(String("自定义模板为空，跳过"));
        return false;
      }
      reqUrl = channel.url;
      reqBody = channel.customBody;
      reqBody.replace("{sender}", senderEscaped);
      reqBody.replace("{message}", messageEscaped);
      reqBody.replace("{timestamp}", timestampEscaped);
      logCaptureLn(String("自定义: " + reqBody));
      break;
    }

    case PUSH_TYPE_FEISHU: {
      // 飞书机器人
      String jsonData = "{";

      // 如果配置了secret，需要添加签名（飞书使用秒级时间戳）
      if (channel.key1.length() > 0) {
        int64_t ts = time(nullptr);
        // 飞书签名: base64(HMAC-SHA256(key=timestamp + "\n" + secret, msg=""))
        String stringToSign = String(ts) + "\n" + channel.key1;
        uint8_t hmacResult[32];
        mbedtls_md_context_t ctx;
        mbedtls_md_init(&ctx);
        mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
        mbedtls_md_hmac_starts(&ctx, (const unsigned char*)stringToSign.c_str(), stringToSign.length());
        mbedtls_md_hmac_finish(&ctx, hmacResult);
        mbedtls_md_free(&ctx);
        String sign = base64::encode(hmacResult, 32);

        jsonData += "\"timestamp\":\"" + String(ts) + "\",";
        jsonData += "\"sign\":\"" + sign + "\",";
      }

      // 飞书消息体
      jsonData += "\"msg_type\":\"text\",";
      jsonData += "\"content\":{\"text\":\"";
      jsonData += "[SMS] 短信通知\\n发送者: " + senderEscaped + "\\n内容: " + messageEscaped + "\\n时间: " + timestampEscaped;
      jsonData += "\"}}";

      reqUrl = channel.url;
      reqBody = jsonData;
      logCaptureLn(String("飞书: " + reqBody));
      break;
    }

    case PUSH_TYPE_GOTIFY: {
      // Gotify 推送
      String gotifyUrl = channel.url;
      // 确保URL以/结尾
      if (!gotifyUrl.endsWith("/")) gotifyUrl += "/";
      gotifyUrl += "message?token=" + channel.key1;

      reqUrl = gotifyUrl;
      reqBody = "{";
      reqBody += "\"title\":\"短信来自: " + senderEscaped + "\",";
      reqBody += "\"message\":\"" + messageEscaped + "\\n\\n时间: " + timestampEscaped + "\",";
      reqBody += "\"priority\":5";
      reqBody += "}";
      logCaptureLn(String("Gotify: " + reqBody));
      break;
    }

    case PUSH_TYPE_TELEGRAM: {
      // Telegram Bot 推送（key1: Chat ID, key2: Bot Token）
      String tgBaseUrl = channel.url.length() > 0 ? channel.url : "https://api.telegram.org";
      if (tgBaseUrl.endsWith("/")) tgBaseUrl.remove(tgBaseUrl.length() - 1);

      reqUrl = tgBaseUrl + "/bot" + channel.key2 + "/sendMessage";

      String text = "[SMS] 短信通知\n发送者: " + senderEscaped + "\n内容: " + messageEscaped + "\n时间: " + timestampEscaped;
      reqBody = "{";
      reqBody += "\"chat_id\":\"" + channel.key1 + "\",";
      reqBody += "\"text\":\"" + text + "\"";
      reqBody += "}";

      logCaptureLn(String("Telegram: " + reqBody));
      break;
    }

    case PUSH_TYPE_WECOM: {
      // 企业微信群机器人：固定 text 消息格式
      reqUrl = channel.url;
      reqBody = "{\"msgtype\":\"text\",\"text\":{\"content\":\"";
      reqBody += "[SMS] 短信通知\\n发送者: " + senderEscaped +
                 "\\n内容: " + messageEscaped +
                 "\\n时间: " + timestampEscaped;
      reqBody += "\"}}";
      logCaptureLn(String("企业微信: " + reqBody));
      break;
    }

    default:
      logCaptureLn(String("未知推送类型"));
      return false;
  }

  return executeChannelRequest(channel, reqUrl, useGet, reqContentType, reqBody,
                               channelName, testMode ? 1 : 2, runtime);
}

// 单通道测试和外部调用入口：每次调用创建独立的网络路由上下文。
bool sendToChannel(const PushChannel& channel, const char* sender, const char* message,
                   const char* timestamp, bool testMode) {
  PushRuntime runtime(config.pushNetworkMode);
  if (!preparePushRoute(runtime)) return false;
  return sendToChannelOnRuntime(channel, sender, message, timestamp, testMode, runtime);
}

// 发送短信到所有启用的推送通道
PushSummary sendSMSToServer(const char* sender, const char* message, const char* timestamp) {
  PushSummary summary = {};

  for (int i = 0; i < MAX_PUSH_CHANNELS; i++) {
    if (isPushChannelValid(config.pushChannels[i])) {
      summary.hasEnabledChannel = true;
      break;
    }
  }

  if (!summary.hasEnabledChannel) {
    logCaptureLn(String("没有启用的推送通道"));
    return summary;
  }

  PushRuntime runtime(config.pushNetworkMode);
  if (!preparePushRoute(runtime)) {
    summary.networkAvailable = false;
    return summary;
  }
  summary.networkAvailable = true;

  String readableTimestamp = formatSmsTimestamp(String(timestamp));
  logCaptureLn(String("\n=== 开始多通道推送 ==="));
  for (int i = 0; i < MAX_PUSH_CHANNELS; i++) {
    if (isPushChannelValid(config.pushChannels[i])) {
      PushChannelResult& result = summary.channels[summary.channelCount];
      String channelName = config.pushChannels[i].name.length() > 0
                               ? config.pushChannels[i].name
                               : ("通道" + String(i + 1));
      snprintf(result.name, sizeof(result.name), "%s", channelName.c_str());
      result.success = sendToChannelOnRuntime(config.pushChannels[i], sender, message,
                                              readableTimestamp.c_str(), false, runtime);
      summary.channelCount++;
      summary.attempted++;
      if (result.success) summary.succeeded++;
      delay(100); // 短暂延迟避免请求过快
    }
  }
  // 只要实际尝试过通道，就说明开始时存在可用路由；后续个别请求失败
  // 应记录为“转发失败/部分成功”，不能误判成“无可用网络”。
  summary.networkAvailable = summary.attempted > 0 || runtime.networkAvailable;
  logCaptureLn(String("=== 多通道推送完成 ===\n"));
  return summary;
}
