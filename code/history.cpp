#include "history.h"
#include "push.h"
#include "web_handlers.h"
#include <SPIFFS.h>
#include <stdio.h>
#include <string.h>

// 文件格式（小端，version 2）：
// 文件头 24 字节：
//   magic u32("SFH1") | version u16 | headerSize u16 | count u16 | reserved u16 |
//   nextId u32 | payloadLength u32 | payloadCrc32 u32
// 单条记录：
//   记录头 16 字节：id u32 | senderLen u16 | timestampLen u16 | messageLen u16 |
//                   reasonLen u16 | status u8 | channelCount u8 |
//                   flags u8（bit0=内容截断） | reserved u8
//   通道块：channelCount * (name[40] + success u8)
//   文本体：sender + timestamp + message + reason
// payloadCrc32 覆盖所有记录头、通道块与文本体。
static const uint32_t HISTORY_MAGIC = 0x31484653;  // "SFH1"，跨版本保持不变
static const uint16_t HISTORY_VERSION = 2;
static const char* HISTORY_FILE_PATH = "/forward_history.bin";
static const char* HISTORY_TMP_PATH = "/forward_history.tmp";

static const size_t HISTORY_FILE_HEADER_SIZE = 24;
static const size_t HISTORY_RECORD_HEADER_SIZE = 16;
static const size_t HISTORY_SENDER_MAX = 64;
static const size_t HISTORY_TIMESTAMP_MAX = 40;
static const size_t HISTORY_REASON_MAX = 48;
static const size_t HISTORY_CHANNEL_SIZE = MAX_PUSH_CHANNEL_NAME + 1;
static const size_t HISTORY_RECORD_MAX = HISTORY_RECORD_HEADER_SIZE +
                                          MAX_PUSH_CHANNELS * HISTORY_CHANNEL_SIZE +
                                          HISTORY_SENDER_MAX + HISTORY_TIMESTAMP_MAX +
                                          HISTORY_MESSAGE_MAX + HISTORY_REASON_MAX;
static const size_t HISTORY_PAYLOAD_MAX = MAX_FORWARD_HISTORY * HISTORY_RECORD_MAX;
static const size_t HISTORY_FILE_MAX = HISTORY_FILE_HEADER_SIZE + HISTORY_PAYLOAD_MAX;

static ForwardHistoryEntry historyEntries[MAX_FORWARD_HISTORY];
static int historyEntryCount = 0;
static uint32_t historyNextId = 1;
static bool historyStorageReady = false;

static void clearHistoryEntry(ForwardHistoryEntry& entry) {
  entry.id = 0;
  entry.sender = "";
  entry.timestamp = "";
  entry.message = "";
  entry.status = SMS_HISTORY_STATUS_FAILED;
  entry.reason = "";
  entry.channelCount = 0;
  for (int i = 0; i < MAX_PUSH_CHANNELS; i++) {
    memset(entry.channels[i].name, 0, sizeof(entry.channels[i].name));
    entry.channels[i].success = false;
  }
  entry.truncated = false;
}

static void resetHistory() {
  for (int i = 0; i < MAX_FORWARD_HISTORY; i++) {
    clearHistoryEntry(historyEntries[i]);
  }
  historyEntryCount = 0;
  historyNextId = 1;
}

static void putU16(uint8_t* buf, size_t offset, uint16_t value) {
  buf[offset] = (uint8_t)(value & 0xFF);
  buf[offset + 1] = (uint8_t)(value >> 8);
}

static void putU32(uint8_t* buf, size_t offset, uint32_t value) {
  buf[offset] = (uint8_t)(value & 0xFF);
  buf[offset + 1] = (uint8_t)((value >> 8) & 0xFF);
  buf[offset + 2] = (uint8_t)((value >> 16) & 0xFF);
  buf[offset + 3] = (uint8_t)((value >> 24) & 0xFF);
}

static uint16_t getU16(const uint8_t* buf, size_t offset) {
  return (uint16_t)buf[offset] | ((uint16_t)buf[offset + 1] << 8);
}

static uint32_t getU32(const uint8_t* buf, size_t offset) {
  return (uint32_t)buf[offset] |
         ((uint32_t)buf[offset + 1] << 8) |
         ((uint32_t)buf[offset + 2] << 16) |
         ((uint32_t)buf[offset + 3] << 24);
}

static uint32_t crc32Update(uint32_t crc, const uint8_t* data, size_t len) {
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; bit++) {
      crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320UL : 0);
    }
  }
  return crc;
}

// 按 UTF-8 字符边界截断，避免留下半个汉字。
static String truncateUtf8(const String& text, size_t maxBytes, bool& wasTruncated) {
  wasTruncated = false;
  if (text.length() <= maxBytes) return text;

  size_t cut = maxBytes;
  while (cut > 0 && (((uint8_t)text.charAt(cut)) & 0xC0) == 0x80) {
    cut--;
  }
  wasTruncated = true;
  return text.substring(0, cut);
}

static void copyChannelName(char* destination, const char* source) {
  memset(destination, 0, MAX_PUSH_CHANNEL_NAME);
  if (!source) return;

  size_t length = 0;
  while (length < MAX_PUSH_CHANNEL_NAME - 1 && source[length] != '\0') {
    destination[length] = source[length];
    length++;
  }
}

static void encodeRecordHeader(uint8_t* buf, const ForwardHistoryEntry& entry) {
  putU32(buf, 0, entry.id);
  putU16(buf, 4, (uint16_t)entry.sender.length());
  putU16(buf, 6, (uint16_t)entry.timestamp.length());
  putU16(buf, 8, (uint16_t)entry.message.length());
  putU16(buf, 10, (uint16_t)entry.reason.length());
  buf[12] = entry.status;
  buf[13] = entry.channelCount;
  buf[14] = entry.truncated ? 0x01 : 0x00;
  buf[15] = 0;
}

static void encodeChannel(uint8_t* buf, const PushChannelResult& channel) {
  memcpy(buf, channel.name, MAX_PUSH_CHANNEL_NAME);
  buf[MAX_PUSH_CHANNEL_NAME] = channel.success ? 1 : 0;
}

static uint32_t computePayload(uint32_t& payloadLength) {
  uint32_t crc = 0xFFFFFFFF;
  payloadLength = 0;

  for (int i = 0; i < historyEntryCount; i++) {
    const ForwardHistoryEntry& entry = historyEntries[i];
    uint8_t recordHeader[HISTORY_RECORD_HEADER_SIZE];
    encodeRecordHeader(recordHeader, entry);
    crc = crc32Update(crc, recordHeader, sizeof(recordHeader));
    payloadLength += (uint32_t)sizeof(recordHeader);

    uint8_t channelBytes[HISTORY_CHANNEL_SIZE];
    uint8_t channelCount = entry.channelCount;
    if (channelCount > MAX_PUSH_CHANNELS) channelCount = MAX_PUSH_CHANNELS;
    for (uint8_t channel = 0; channel < channelCount; channel++) {
      encodeChannel(channelBytes, entry.channels[channel]);
      crc = crc32Update(crc, channelBytes, sizeof(channelBytes));
      payloadLength += (uint32_t)sizeof(channelBytes);
    }

    crc = crc32Update(crc, (const uint8_t*)entry.sender.c_str(), entry.sender.length());
    crc = crc32Update(crc, (const uint8_t*)entry.timestamp.c_str(), entry.timestamp.length());
    crc = crc32Update(crc, (const uint8_t*)entry.message.c_str(), entry.message.length());
    crc = crc32Update(crc, (const uint8_t*)entry.reason.c_str(), entry.reason.length());
    payloadLength += (uint32_t)(entry.sender.length() + entry.timestamp.length() +
                                entry.message.length() + entry.reason.length());
  }

  return crc ^ 0xFFFFFFFF;
}

static bool writeWithCrc(File& file, const uint8_t* data, size_t length, uint32_t& crc) {
  if (length == 0) return true;
  if (file.write(data, length) != length) return false;
  crc = crc32Update(crc, data, length);
  return true;
}

static bool writeTextWithCrc(File& file, const String& text, uint32_t& crc) {
  return writeWithCrc(file, (const uint8_t*)text.c_str(), text.length(), crc);
}

static bool writeFileHeader(File& file, uint16_t count, uint32_t nextId,
                            uint32_t payloadLength, uint32_t payloadCrc) {
  uint8_t header[HISTORY_FILE_HEADER_SIZE];
  putU32(header, 0, HISTORY_MAGIC);
  putU16(header, 4, HISTORY_VERSION);
  putU16(header, 6, (uint16_t)HISTORY_FILE_HEADER_SIZE);
  putU16(header, 8, count);
  putU16(header, 10, 0);
  putU32(header, 12, nextId);
  putU32(header, 16, payloadLength);
  putU32(header, 20, payloadCrc);
  return file.write(header, sizeof(header)) == sizeof(header);
}

static bool writeHistoryEntry(File& file, const ForwardHistoryEntry& entry, uint32_t& crc) {
  uint8_t recordHeader[HISTORY_RECORD_HEADER_SIZE];
  encodeRecordHeader(recordHeader, entry);
  if (!writeWithCrc(file, recordHeader, sizeof(recordHeader), crc)) return false;

  uint8_t channelBytes[HISTORY_CHANNEL_SIZE];
  for (uint8_t channel = 0; channel < entry.channelCount; channel++) {
    encodeChannel(channelBytes, entry.channels[channel]);
    if (!writeWithCrc(file, channelBytes, sizeof(channelBytes), crc)) return false;
  }

  if (!writeTextWithCrc(file, entry.sender, crc)) return false;
  if (!writeTextWithCrc(file, entry.timestamp, crc)) return false;
  if (!writeTextWithCrc(file, entry.message, crc)) return false;
  if (!writeTextWithCrc(file, entry.reason, crc)) return false;
  return true;
}

static bool saveForwardHistory() {
  if (!historyStorageReady) return false;

  uint32_t payloadLength = 0;
  uint32_t payloadCrc = computePayload(payloadLength);
  if (payloadLength > HISTORY_PAYLOAD_MAX) {
    logCaptureLn(String("历史: 数据长度异常，取消保存"));
    return false;
  }

  SPIFFS.remove(HISTORY_TMP_PATH);
  File file = SPIFFS.open(HISTORY_TMP_PATH, "w");
  if (!file) {
    logCaptureLn(String("历史: 创建临时文件失败"));
    return false;
  }

  bool ok = writeFileHeader(file, (uint16_t)historyEntryCount, historyNextId,
                            payloadLength, payloadCrc);
  uint32_t writtenCrc = 0xFFFFFFFF;
  for (int i = 0; ok && i < historyEntryCount; i++) {
    ok = writeHistoryEntry(file, historyEntries[i], writtenCrc);
  }
  if (ok && (writtenCrc ^ 0xFFFFFFFF) != payloadCrc) {
    logCaptureLn(String("历史: CRC 计算不一致，取消保存"));
    ok = false;
  }
  file.close();

  if (!ok) {
    logCaptureLn(String("历史: 写入文件失败"));
    SPIFFS.remove(HISTORY_TMP_PATH);
    return false;
  }

  // SPIFFS 的 rename 不保证覆盖已有文件；必要时先删除正式文件再替换。
  bool replaced = SPIFFS.rename(HISTORY_TMP_PATH, HISTORY_FILE_PATH);
  if (!replaced) {
    SPIFFS.remove(HISTORY_FILE_PATH);
    replaced = SPIFFS.rename(HISTORY_TMP_PATH, HISTORY_FILE_PATH);
  }
  if (!replaced) {
    logCaptureLn(String("历史: 替换历史文件失败"));
    SPIFFS.remove(HISTORY_TMP_PATH);
    return false;
  }
  return true;
}

static void failHistoryLoad(const char* reason) {
  resetHistory();
  logCaptureLn(String("历史: ") + reason + "，已重置为空历史");
  SPIFFS.remove(HISTORY_FILE_PATH);
  if (historyStorageReady) {
    saveForwardHistory();
  }
}

static bool readWithCrc(File& file, uint8_t* data, size_t length, uint32_t& crc) {
  if (length == 0) return true;
  if (file.read(data, length) != length) return false;
  crc = crc32Update(crc, data, length);
  return true;
}

static bool readTextWithCrc(File& file, char* buffer, size_t length, uint32_t& crc) {
  if (!readWithCrc(file, (uint8_t*)buffer, length, crc)) return false;
  buffer[length] = '\0';
  return true;
}

static bool readHistoryRecord(File& file, size_t remaining, ForwardHistoryEntry& entry,
                              uint32_t& crc, size_t& recordSize) {
  recordSize = 0;
  if (remaining < HISTORY_RECORD_HEADER_SIZE) return false;

  uint8_t recordHeader[HISTORY_RECORD_HEADER_SIZE];
  if (!readWithCrc(file, recordHeader, sizeof(recordHeader), crc)) return false;

  uint32_t id = getU32(recordHeader, 0);
  size_t senderLength = getU16(recordHeader, 4);
  size_t timestampLength = getU16(recordHeader, 6);
  size_t messageLength = getU16(recordHeader, 8);
  size_t reasonLength = getU16(recordHeader, 10);
  uint8_t status = recordHeader[12];
  uint8_t channelCount = recordHeader[13];
  uint8_t flags = recordHeader[14];

  if (id == 0 ||
      senderLength > HISTORY_SENDER_MAX ||
      timestampLength > HISTORY_TIMESTAMP_MAX ||
      messageLength > HISTORY_MESSAGE_MAX ||
      reasonLength > HISTORY_REASON_MAX ||
      status > SMS_HISTORY_STATUS_NO_NETWORK ||
      channelCount > MAX_PUSH_CHANNELS ||
      (flags & 0xFE) != 0 ||
      recordHeader[15] != 0) {
    return false;
  }

  size_t channelBytes = (size_t)channelCount * HISTORY_CHANNEL_SIZE;
  size_t bodyLength = senderLength + timestampLength + messageLength + reasonLength;
  if (channelBytes > remaining - HISTORY_RECORD_HEADER_SIZE ||
      bodyLength > remaining - HISTORY_RECORD_HEADER_SIZE - channelBytes) {
    return false;
  }
  recordSize = HISTORY_RECORD_HEADER_SIZE + channelBytes + bodyLength;

  entry.id = id;
  entry.status = status;
  entry.channelCount = channelCount;
  entry.truncated = (flags & 0x01) != 0;

  for (uint8_t channel = 0; channel < channelCount; channel++) {
    uint8_t channelBytesBuffer[HISTORY_CHANNEL_SIZE];
    if (!readWithCrc(file, channelBytesBuffer, sizeof(channelBytesBuffer), crc)) return false;

    size_t nameLength = 0;
    while (nameLength < MAX_PUSH_CHANNEL_NAME &&
           channelBytesBuffer[nameLength] != '\0') {
      nameLength++;
    }
    if (nameLength >= MAX_PUSH_CHANNEL_NAME || channelBytesBuffer[MAX_PUSH_CHANNEL_NAME] > 1) {
      return false;
    }

    memset(entry.channels[channel].name, 0, sizeof(entry.channels[channel].name));
    memcpy(entry.channels[channel].name, channelBytesBuffer, nameLength);
    entry.channels[channel].success = channelBytesBuffer[MAX_PUSH_CHANNEL_NAME] != 0;
  }

  char senderBuffer[HISTORY_SENDER_MAX + 1];
  char timestampBuffer[HISTORY_TIMESTAMP_MAX + 1];
  char messageBuffer[HISTORY_MESSAGE_MAX + 1];
  char reasonBuffer[HISTORY_REASON_MAX + 1];

  if (!readTextWithCrc(file, senderBuffer, senderLength, crc) ||
      !readTextWithCrc(file, timestampBuffer, timestampLength, crc) ||
      !readTextWithCrc(file, messageBuffer, messageLength, crc) ||
      !readTextWithCrc(file, reasonBuffer, reasonLength, crc)) {
    return false;
  }

  entry.sender = String(senderBuffer, (unsigned int)senderLength);
  entry.timestamp = formatSmsTimestamp(String(timestampBuffer, (unsigned int)timestampLength));
  entry.message = String(messageBuffer, (unsigned int)messageLength);
  entry.reason = String(reasonBuffer, (unsigned int)reasonLength);
  return true;
}

static bool historyIdInUse(uint32_t id) {
  for (int i = 0; i < historyEntryCount; i++) {
    if (historyEntries[i].id == id) return true;
  }
  return false;
}

static uint32_t allocateHistoryId() {
  uint32_t id = historyNextId;
  if (id == 0) id = 1;
  while (historyIdInUse(id)) {
    id++;
    if (id == 0) id = 1;
  }
  historyNextId = id + 1;
  if (historyNextId == 0) historyNextId = 1;
  return id;
}

void initForwardHistory() {
  resetHistory();
  historyStorageReady = false;

  if (!SPIFFS.begin(true)) {
    logCaptureLn(String("历史: SPIFFS 挂载失败，历史仅在内存中保留"));
    return;
  }
  historyStorageReady = true;

  // 上次保存若停在“旧文件已删除、临时文件已写完整”的窗口，优先恢复临时文件。
  if (!SPIFFS.exists(HISTORY_FILE_PATH) && SPIFFS.exists(HISTORY_TMP_PATH)) {
    if (SPIFFS.rename(HISTORY_TMP_PATH, HISTORY_FILE_PATH)) {
      logCaptureLn(String("历史: 已恢复上次未提交的临时文件"));
    }
  }

  if (!SPIFFS.exists(HISTORY_FILE_PATH)) {
    logCaptureLn(String("历史: 文件不存在，从空历史开始"));
    return;
  }

  File file = SPIFFS.open(HISTORY_FILE_PATH, "r");
  if (!file) {
    logCaptureLn(String("历史: 文件打开失败，从空历史开始"));
    return;
  }

  size_t fileSize = file.size();
  if (fileSize < 6 || fileSize > HISTORY_FILE_MAX) {
    file.close();
    failHistoryLoad("文件长度异常");
    return;
  }

  uint8_t fileHeader[HISTORY_FILE_HEADER_SIZE] = {0};
  if (file.read(fileHeader, 6) != 6) {
    file.close();
    failHistoryLoad("文件头读取失败");
    return;
  }

  uint32_t magic = getU32(fileHeader, 0);
  uint16_t version = getU16(fileHeader, 4);
  if (version == 1) {
    file.close();
    failHistoryLoad("检测到 version 1 旧格式");
    return;
  }
  if (magic != HISTORY_MAGIC || version != HISTORY_VERSION) {
    file.close();
    failHistoryLoad("文件版本或格式不兼容");
    return;
  }
  if (fileSize < HISTORY_FILE_HEADER_SIZE) {
    file.close();
    failHistoryLoad("文件头不完整");
    return;
  }
  if (file.read(fileHeader + 6, HISTORY_FILE_HEADER_SIZE - 6) !=
      HISTORY_FILE_HEADER_SIZE - 6) {
    file.close();
    failHistoryLoad("文件头读取失败");
    return;
  }

  uint16_t headerSize = getU16(fileHeader, 6);
  uint16_t count = getU16(fileHeader, 8);
  uint16_t reserved = getU16(fileHeader, 10);
  uint32_t nextId = getU32(fileHeader, 12);
  uint32_t payloadLength = getU32(fileHeader, 16);
  uint32_t storedCrc = getU32(fileHeader, 20);

  if (headerSize != HISTORY_FILE_HEADER_SIZE ||
      reserved != 0 ||
      count > MAX_FORWARD_HISTORY ||
      payloadLength > HISTORY_PAYLOAD_MAX ||
      payloadLength != fileSize - HISTORY_FILE_HEADER_SIZE) {
    file.close();
    failHistoryLoad("文件头校验失败");
    return;
  }

  uint32_t crc = 0xFFFFFFFF;
  size_t consumed = 0;
  uint32_t maxId = 0;
  bool ok = true;

  for (uint16_t i = 0; i < count && ok; i++) {
    ForwardHistoryEntry entry;
    clearHistoryEntry(entry);
    size_t recordSize = 0;
    if (!readHistoryRecord(file, payloadLength - consumed, entry, crc, recordSize)) {
      ok = false;
      break;
    }
    consumed += recordSize;
    historyEntries[historyEntryCount] = entry;
    historyEntryCount++;
    if (entry.id > maxId) maxId = entry.id;
  }

  if (ok && (consumed != payloadLength || file.available() != 0)) ok = false;
  if (ok && (crc ^ 0xFFFFFFFF) != storedCrc) ok = false;
  file.close();

  if (!ok) {
    failHistoryLoad("文件内容校验失败");
    return;
  }

  historyNextId = nextId;
  if (historyNextId == 0 || historyNextId <= maxId) {
    historyNextId = maxId == 0xFFFFFFFFUL ? 1 : maxId + 1;
  }

  logCaptureLn(String("历史: 已加载 ") + String(historyEntryCount) + " 条记录");
}

void addForwardHistory(const char* sender, const char* timestamp, const char* message,
                       uint8_t status, const char* reason,
                       const PushChannelResult* channels, uint8_t channelCount) {
  ForwardHistoryEntry entry;
  clearHistoryEntry(entry);
  entry.id = allocateHistoryId();

  bool senderTruncated = false;
  bool timestampTruncated = false;
  bool messageTruncated = false;
  bool reasonTruncated = false;
  entry.sender = truncateUtf8(String(sender ? sender : ""), HISTORY_SENDER_MAX,
                              senderTruncated);
  entry.timestamp = truncateUtf8(formatSmsTimestamp(String(timestamp ? timestamp : "")), HISTORY_TIMESTAMP_MAX,
                                 timestampTruncated);
  entry.message = truncateUtf8(String(message ? message : ""), HISTORY_MESSAGE_MAX,
                               messageTruncated);
  entry.reason = truncateUtf8(String(reason ? reason : ""), HISTORY_REASON_MAX,
                              reasonTruncated);

  if (status <= SMS_HISTORY_STATUS_NO_NETWORK) {
    entry.status = status;
  } else {
    entry.status = SMS_HISTORY_STATUS_FAILED;
    logCaptureLn(String("历史: 非法状态值，按转发失败记录"));
  }

  uint8_t storedChannelCount = channels ? channelCount : 0;
  if (!channels && channelCount > 0) {
    logCaptureLn(String("历史: 推送通道结果缺失，按 0 个通道记录"));
  }
  if (storedChannelCount > MAX_PUSH_CHANNELS) {
    storedChannelCount = MAX_PUSH_CHANNELS;
    logCaptureLn(String("历史: 推送通道数超限，已截断"));
  }
  entry.channelCount = storedChannelCount;
  for (uint8_t channel = 0; channel < storedChannelCount; channel++) {
    copyChannelName(entry.channels[channel].name, channels[channel].name);
    entry.channels[channel].success = channels[channel].success;
  }
  entry.truncated = messageTruncated;

  if (senderTruncated) logCaptureLn(String("历史: 发送者超长，已截断"));
  if (timestampTruncated) logCaptureLn(String("历史: 时间戳超长，已截断"));
  if (messageTruncated) logCaptureLn(String("历史: 短信内容超长，已按 UTF-8 边界截断"));
  if (reasonTruncated) logCaptureLn(String("历史: 原因超长，已截断"));

  if (historyEntryCount >= MAX_FORWARD_HISTORY) {
    for (int i = 1; i < MAX_FORWARD_HISTORY; i++) {
      historyEntries[i - 1] = historyEntries[i];
    }
    historyEntries[MAX_FORWARD_HISTORY - 1] = entry;
  } else {
    historyEntries[historyEntryCount++] = entry;
  }

  saveForwardHistory();
}

int forwardHistoryCount() {
  return historyEntryCount;
}

bool deleteForwardHistory(uint32_t id) {
  for (int i = 0; i < historyEntryCount; i++) {
    if (historyEntries[i].id == id) {
      for (int j = i; j < historyEntryCount - 1; j++) {
        historyEntries[j] = historyEntries[j + 1];
      }
      clearHistoryEntry(historyEntries[historyEntryCount - 1]);
      historyEntryCount--;
      saveForwardHistory();
      logCaptureLn(String("历史: 已删除记录 id=") + String((unsigned long)id));
      return true;
    }
  }
  return false;
}

static const char* historyStatusText(uint8_t status) {
  switch (status) {
    case SMS_HISTORY_STATUS_FORWARDED: return "已转发";
    case SMS_HISTORY_STATUS_PARTIAL: return "部分成功";
    case SMS_HISTORY_STATUS_FAILED: return "转发失败";
    case SMS_HISTORY_STATUS_BLACKLISTED: return "黑名单拦截";
    case SMS_HISTORY_STATUS_ADMIN_COMMAND: return "管理员命令";
    case SMS_HISTORY_STATUS_NO_CHANNEL: return "未配置推送";
    case SMS_HISTORY_STATUS_WIFI_OFF: return "WiFi 未连接";
    case SMS_HISTORY_STATUS_NO_NETWORK: return "无可用网络";
    default: return "未知状态";
  }
}

// 补齐 push.h 中 jsonEscape 未覆盖的 C0 控制字符。
static String escapeHistoryJson(const String& value) {
  String escaped = jsonEscape(value);
  String result;
  result.reserve(escaped.length());
  for (unsigned int i = 0; i < escaped.length(); i++) {
    uint8_t c = (uint8_t)escaped.charAt(i);
    if (c < 0x20) {
      char buf[7];
      snprintf(buf, sizeof(buf), "\\u%04x", c);
      result += buf;
    } else {
      result += (char)c;
    }
  }
  return result;
}

String forwardHistoryToJson() {
  size_t estimate = 2;
  for (int i = 0; i < historyEntryCount; i++) {
    estimate += 200 + historyEntries[i].sender.length() +
                historyEntries[i].timestamp.length() +
                historyEntries[i].message.length() +
                historyEntries[i].reason.length() +
                historyEntries[i].channelCount * 64;
  }

  String json;
  json.reserve((unsigned int)estimate);
  json += '[';
  bool first = true;
  for (int i = historyEntryCount - 1; i >= 0; i--) {
    const ForwardHistoryEntry& entry = historyEntries[i];
    if (!first) json += ',';
    first = false;

    json += "{\"id\":";
    json += String((unsigned long)entry.id);
    json += ",\"sender\":\"";
    json += escapeHistoryJson(entry.sender);
    json += "\",\"timestamp\":\"";
    json += escapeHistoryJson(formatSmsTimestamp(entry.timestamp));
    json += "\",\"message\":\"";
    json += escapeHistoryJson(entry.message);
    json += "\",\"status\":";
    json += String((unsigned int)entry.status);
    json += ",\"statusText\":\"";
    json += historyStatusText(entry.status);
    json += "\",\"reason\":\"";
    json += escapeHistoryJson(entry.reason);
    json += "\",\"channels\":[";
    for (uint8_t channel = 0; channel < entry.channelCount; channel++) {
      if (channel > 0) json += ',';
      json += "{\"name\":\"";
      json += escapeHistoryJson(String(entry.channels[channel].name));
      json += "\",\"success\":";
      json += entry.channels[channel].success ? "true" : "false";
      json += '}';
    }
    json += "],\"truncated\":";
    json += entry.truncated ? "true" : "false";
    json += '}';
  }
  json += ']';
  return json;
}
