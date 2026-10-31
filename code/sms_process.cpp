#include "sms_process.h"
#include "web_handlers.h"
#include "modem.h"
#include "history.h"
#include "web_handlers.h"
#include "push.h"
#include "web_handlers.h"

// 初始化长短信缓存
void initConcatBuffer() {
  for (int i = 0; i < MAX_CONCAT_MESSAGES; i++) {
    concatBuffer[i].inUse = false;
    concatBuffer[i].receivedParts = 0;
    for (int j = 0; j < MAX_CONCAT_PARTS; j++) {
      concatBuffer[i].parts[j].valid = false;
      concatBuffer[i].parts[j].text = "";
    }
  }
}

// 查找或创建长短信缓存槽位
int findOrCreateConcatSlot(int refNumber, const char* sender, int totalParts) {
  // 先查找是否已存在
  for (int i = 0; i < MAX_CONCAT_MESSAGES; i++) {
    if (concatBuffer[i].inUse && 
        concatBuffer[i].refNumber == refNumber &&
        concatBuffer[i].sender.equals(sender)) {
      return i;
    }
  }
  
  // 查找空闲槽位
  for (int i = 0; i < MAX_CONCAT_MESSAGES; i++) {
    if (!concatBuffer[i].inUse) {
      concatBuffer[i].inUse = true;
      concatBuffer[i].refNumber = refNumber;
      concatBuffer[i].sender = String(sender);
      concatBuffer[i].totalParts = totalParts;
      concatBuffer[i].receivedParts = 0;
      concatBuffer[i].firstPartTime = millis();
      for (int j = 0; j < MAX_CONCAT_PARTS; j++) {
        concatBuffer[i].parts[j].valid = false;
        concatBuffer[i].parts[j].text = "";
      }
      return i;
    }
  }
  
  // 没有空闲槽位，查找最老的槽位覆盖
  int oldestSlot = 0;
  unsigned long oldestTime = concatBuffer[0].firstPartTime;
  for (int i = 1; i < MAX_CONCAT_MESSAGES; i++) {
    if (concatBuffer[i].firstPartTime < oldestTime) {
      oldestTime = concatBuffer[i].firstPartTime;
      oldestSlot = i;
    }
  }
  
  // 覆盖最老的槽位
  logCaptureLn(String("[!] 长短信缓存已满，覆盖最老的槽位"));
  concatBuffer[oldestSlot].inUse = true;
  concatBuffer[oldestSlot].refNumber = refNumber;
  concatBuffer[oldestSlot].sender = String(sender);
  concatBuffer[oldestSlot].totalParts = totalParts;
  concatBuffer[oldestSlot].receivedParts = 0;
  concatBuffer[oldestSlot].firstPartTime = millis();
  for (int j = 0; j < MAX_CONCAT_PARTS; j++) {
    concatBuffer[oldestSlot].parts[j].valid = false;
    concatBuffer[oldestSlot].parts[j].text = "";
  }
  return oldestSlot;
}

// 合并长短信各分段
String assembleConcatSms(int slot) {
  String result = "";
  for (int i = 0; i < concatBuffer[slot].totalParts; i++) {
    if (concatBuffer[slot].parts[i].valid) {
      result += concatBuffer[slot].parts[i].text;
    } else {
      result += "[缺失分段" + String(i + 1) + "]";
    }
  }
  return result;
}

// 清空长短信槽位
void clearConcatSlot(int slot) {
  concatBuffer[slot].inUse = false;
  concatBuffer[slot].receivedParts = 0;
  concatBuffer[slot].sender = "";
  concatBuffer[slot].timestamp = "";
  for (int j = 0; j < MAX_CONCAT_PARTS; j++) {
    concatBuffer[slot].parts[j].valid = false;
    concatBuffer[slot].parts[j].text = "";
  }
}

// 检查长短信超时并转发
void checkConcatTimeout() {
  unsigned long now = millis();
  for (int i = 0; i < MAX_CONCAT_MESSAGES; i++) {
    if (concatBuffer[i].inUse) {
      if (now - concatBuffer[i].firstPartTime >= CONCAT_TIMEOUT_MS) {
        logCaptureLn(String("[TIMEOUT] 长短信超时，强制转发不完整消息"));
        logCaptureF("  参考号: %d, 已收到: %d/%d\n", 
                      concatBuffer[i].refNumber,
                      concatBuffer[i].receivedParts,
                      concatBuffer[i].totalParts);
        
        // 合并已收到的分段
        String fullText = assembleConcatSms(i);
        
        // 处理短信内容
        processSmsContent(concatBuffer[i].sender.c_str(), 
                         fullText.c_str(), 
                         concatBuffer[i].timestamp.c_str());
        
        // 清空槽位
        clearConcatSlot(i);
      }
    }
  }
}

// 读取串口一行（含回车换行），返回行字符串，无新行时返回空
String readSerialLine(HardwareSerial& port) {
  static char lineBuf[SERIAL_BUFFER_SIZE];
  static int linePos = 0;

  while (port.available()) {
    char c = port.read();
    if (c == '\n') {
      lineBuf[linePos] = 0;
      String res = String(lineBuf);
      linePos = 0;
      return res;
    } else if (c != '\r') {  // 跳过\r
      if (linePos < SERIAL_BUFFER_SIZE - 1)
        lineBuf[linePos++] = c;
      else
        linePos = 0;  //超长报错保护，重头计
    }
  }
  return "";
}

// 检查字符串是否为有效的十六进制PDU数据
bool isHexString(const String& str) {
  if (str.length() == 0) return false;
  for (unsigned int i = 0; i < str.length(); i++) {
    char c = str.charAt(i);
    if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'))) {
      return false;
    }
  }
  return true;
}

// 去掉号码中的分隔符，并统一 +86 前缀
static String normalizeSenderNumber(const String& value) {
  String normalized;
  normalized.reserve(value.length());
  for (unsigned int i = 0; i < value.length(); i++) {
    char c = value.charAt(i);
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n' ||
        c == '-' || c == '(' || c == ')') {
      continue;
    }
    normalized += c;
  }
  if (normalized.startsWith("+86")) {
    normalized.remove(0, 3);
  } else if (normalized.startsWith("0086") && normalized.length() > 13) {
    normalized.remove(0, 4);
  } else if (normalized.startsWith("86") && normalized.length() > 11) {
    normalized.remove(0, 2);
  }
  return normalized;
}

// 按行匹配号码规则，支持精确、前缀、后缀
static bool matchesNumberRules(const String& normalizedSender, const String& rules,
                               bool prefixMatch, bool suffixMatch) {
  if (normalizedSender.length() == 0 || rules.length() == 0) return false;

  int listLen = (int)rules.length();
  int start = 0;
  while (start <= listLen) {
    int end = rules.indexOf('\n', start);
    if (end == -1) end = listLen;

    String rule = rules.substring(start, end);
    rule.trim();
    String normalizedRule = normalizeSenderNumber(rule);

    if (normalizedRule.length() > 0) {
      if (prefixMatch) {
        if (normalizedSender.startsWith(normalizedRule)) return true;
      } else if (suffixMatch) {
        if (normalizedSender.endsWith(normalizedRule)) return true;
      } else if (normalizedSender.equals(normalizedRule)) {
        return true;
      }
    }

    start = end + 1;
  }

  return false;
}

// 检查发送者是否在精确、前缀或后缀号码黑名单中
bool isInNumberBlackList(const char* sender) {
  if (!sender || !*sender) return false;
  String normalizedSender = normalizeSenderNumber(String(sender));
  return matchesNumberRules(normalizedSender, config.numberBlackList, false, false) ||
         matchesNumberRules(normalizedSender, config.numberPrefixBlackList, true, false) ||
         matchesNumberRules(normalizedSender, config.numberSuffixBlackList, false, true);
}

// 检查内容是否包含任意一个黑名单关键词（多个关键词为“或”关系）
bool isInContentBlackList(const char* text) {
  if (!text || !*text || config.contentBlackList.length() == 0) return false;

  String content = String(text);
  content.toLowerCase();

  int listLen = (int)config.contentBlackList.length();
  int start = 0;
  while (start <= listLen) {
    int end = config.contentBlackList.indexOf('\n', start);
    if (end == -1) end = listLen;

    String keyword = config.contentBlackList.substring(start, end);
    keyword.trim();
    if (keyword.length() > 0) {
      keyword.toLowerCase();
      if (content.indexOf(keyword) >= 0) return true;
    }

    start = end + 1;
  }

  return false;
}

// 检查发送者是否为管理员
bool isAdmin(const char* sender) {
  if (config.adminPhone.length() == 0) return false;

  String senderStr = normalizeSenderNumber(String(sender));
  String adminStr = normalizeSenderNumber(config.adminPhone);
  return senderStr.length() > 0 && senderStr.equals(adminStr);
}

// 处理管理员命令
void processAdminCommand(const char* sender, const char* text) {
  String cmd = String(text);
  cmd.trim();
  
  logCaptureLn(String("处理管理员命令: " + cmd));
  
  // 处理 SMS:号码:内容 命令
  if (cmd.startsWith("SMS:")) {
    int firstColon = cmd.indexOf(':');
    int secondColon = cmd.indexOf(':', firstColon + 1);
    
    if (secondColon > firstColon + 1) {
      String targetPhone = cmd.substring(firstColon + 1, secondColon);
      String smsContent = cmd.substring(secondColon + 1);
      
      targetPhone.trim();
      smsContent.trim();
      
      logCaptureLn(String("目标号码: " + targetPhone));
      logCaptureLn(String("短信内容: " + smsContent));
      
      bool success = sendSMS(targetPhone.c_str(), smsContent.c_str());
      
      logCaptureLn(String("管理员短信命令执行") + (success ? "成功" : "失败"));
    } else {
      logCaptureLn(String("SMS命令格式错误"));
    }
  }
  // 处理 RESET 命令
  else if (cmd.equals("RESET")) {
    logCaptureLn(String("执行RESET命令"));
    
    // 重启模组
    resetModule();
    
    // 重启ESP32
    logCaptureLn(String("正在重启ESP32..."));
    delay(1000);
    ESP.restart();
  }
  else {
    logCaptureLn(String("未知命令: " + cmd));
  }
}

// 处理最终的短信内容（管理员命令检查和转发）
void processSmsContent(const char* sender, const char* text, const char* timestamp) {
  String readableTimestamp = formatSmsTimestamp(String(timestamp));
  logCaptureLn(String("=== 处理短信内容 ==="));
  logCaptureLn(String("发送者: " + String(sender)));
  logCaptureLn(String("时间戳: " + readableTimestamp));
  logCaptureLn(String("内容: " + String(text)));
  logCaptureLn(String("===================="));

  // 管理员号码优先于黑名单，保证远程命令不被过滤
  bool adminSender = isAdmin(sender);
  if (!adminSender) {
    if (isInNumberBlackList(sender)) {
      logCaptureLn(String("发送者命中号码黑名单，忽略该短信"));
      addForwardHistory(sender, readableTimestamp.c_str(), text, SMS_HISTORY_STATUS_BLACKLISTED,
                        "号码黑名单", nullptr, 0);
      return;
    }
    if (isInContentBlackList(text)) {
      logCaptureLn(String("短信内容命中关键词黑名单，忽略该短信"));
      addForwardHistory(sender, readableTimestamp.c_str(), text, SMS_HISTORY_STATUS_BLACKLISTED,
                        "内容关键词", nullptr, 0);
      return;
    }
  }

  // 检查是否为管理员命令
  if (adminSender) {
    logCaptureLn(String("收到管理员短信，检查命令..."));
    String smsText = String(text);
    smsText.trim();

    // 检查是否为命令格式
    if (smsText.startsWith("SMS:") || smsText.equals("RESET")) {
      addForwardHistory(sender, readableTimestamp.c_str(), text, SMS_HISTORY_STATUS_ADMIN_COMMAND,
                        "管理员命令", nullptr, 0);
      processAdminCommand(sender, text);
      // 命令已处理，不再进入普通转发
      return;
    }
  }

  // 发送通知http（推送到所有启用的通道）
  PushSummary pushResult = sendSMSToServer(sender, text, readableTimestamp.c_str());
  uint8_t historyStatus = SMS_HISTORY_STATUS_FORWARDED;
  if (!pushResult.hasEnabledChannel) {
    historyStatus = SMS_HISTORY_STATUS_NO_CHANNEL;
  } else if (!pushResult.networkAvailable) {
    historyStatus = SMS_HISTORY_STATUS_NO_NETWORK;
  } else if (pushResult.attempted == 0) {
    historyStatus = SMS_HISTORY_STATUS_FAILED;
  } else if (pushResult.succeeded == 0) {
    historyStatus = SMS_HISTORY_STATUS_FAILED;
  } else if (pushResult.succeeded < pushResult.attempted) {
    historyStatus = SMS_HISTORY_STATUS_PARTIAL;
  }
  addForwardHistory(sender, readableTimestamp.c_str(), text, historyStatus, "",
                    pushResult.channels, pushResult.channelCount);
}

// ---------------- SMS URC / 已存短信队列 ----------------
// ML307A 在 CNMI=2,1 下会先存 ME，再上报 +CMTI。队列只存索引，
// 真正读取时使用 AT+CMGR；即使串口忙，也能在稍后补收。
#define PENDING_SMS_INDEX_CAPACITY 16
#define PENDING_SMS_PDU_CAPACITY 4

static int pendingIndexes[PENDING_SMS_INDEX_CAPACITY];
static int pendingIndexHead = 0;
static int pendingIndexCount = 0;

static String pendingPdus[PENDING_SMS_PDU_CAPACITY];
static int pendingPduHead = 0;
static int pendingPduCount = 0;

static enum SmsUrcState {
  SMS_URC_IDLE,
  SMS_URC_WAIT_PDU
} smsUrcState = SMS_URC_IDLE;

static void enqueueSmsIndex(int index) {
  if (index <= 0) return;
  if (pendingIndexCount >= PENDING_SMS_INDEX_CAPACITY) {
    logCaptureLn(String("[!] 待读取短信索引队列已满，稍后由CMGL补收"));
    return;
  }
  int slot = (pendingIndexHead + pendingIndexCount) % PENDING_SMS_INDEX_CAPACITY;
  pendingIndexes[slot] = index;
  pendingIndexCount++;
}

static int dequeueSmsIndex() {
  if (pendingIndexCount == 0) return -1;
  int index = pendingIndexes[pendingIndexHead];
  pendingIndexHead = (pendingIndexHead + 1) % PENDING_SMS_INDEX_CAPACITY;
  pendingIndexCount--;
  return index;
}

static void enqueueSmsPdu(const String& pduLine) {
  if (pendingPduCount >= PENDING_SMS_PDU_CAPACITY) {
    logCaptureLn(String("[!] 直接短信PDU队列已满，无法缓存"));
    return;
  }
  int slot = (pendingPduHead + pendingPduCount) % PENDING_SMS_PDU_CAPACITY;
  pendingPdus[slot] = pduLine;
  pendingPduCount++;
}

static String dequeueSmsPdu() {
  if (pendingPduCount == 0) return "";
  String pduLine = pendingPdus[pendingPduHead];
  pendingPdus[pendingPduHead] = "";
  pendingPduHead = (pendingPduHead + 1) % PENDING_SMS_PDU_CAPACITY;
  pendingPduCount--;
  return pduLine;
}

// 返回 true 表示这一行属于短信URC，已经消费；false 表示是普通AT响应。
bool handleSmsUrcLine(const String& rawLine) {
  String line = rawLine;
  line.trim();
  if (line.length() == 0) return false;

  if (smsUrcState == SMS_URC_WAIT_PDU) {
    if (isHexString(line)) {
      logCaptureLn(String("[SMS] 收到直接上报PDU，长度: " + String(line.length())));
      enqueueSmsPdu(line);
      smsUrcState = SMS_URC_IDLE;
      return true;
    }
    logCaptureLn(String("[!] +CMT后未收到有效PDU，返回IDLE"));
    smsUrcState = SMS_URC_IDLE;
  }

  if (line.startsWith("+CMT:")) {
    logCaptureLn(String("检测到+CMT，等待PDU数据..."));
    smsUrcState = SMS_URC_WAIT_PDU;
    return true;
  }

  if (line.startsWith("+CMTI:")) {
    int comma = line.lastIndexOf(',');
    if (comma >= 0) {
      int index = line.substring(comma + 1).toInt();
      if (index > 0) {
        logCaptureLn(String("[SMS] 收到新短信通知，ME索引: " + String(index)));
        enqueueSmsIndex(index);
      } else {
        logCaptureLn(String("[!] 无法解析+CMTI索引: " + line));
      }
    }
    return true;
  }

  return false;
}

// 解析一条PDU并交给短信处理流程。返回 true 表示PDU解析成功。
static bool processPduLine(const String& pduLine) {
  logCaptureLn(String("收到PDU数据: " + pduLine));
  logCaptureLn(String("PDU长度: " + String(pduLine.length()) + " 字符"));

  if (!pdu.decodePDU(pduLine.c_str())) {
    logCaptureLn(String("[ERR] PDU解析失败！"));
    return false;
  }

  logCaptureLn(String("[OK] PDU解析成功"));
  logCaptureLn(String("=== 短信内容 ==="));
  logCaptureLn(String("发送者: " + String(pdu.getSender())));
  logCaptureLn(String("时间戳: " + formatSmsTimestamp(String(pdu.getTimeStamp()))));
  logCaptureLn(String("内容: " + String(pdu.getText())));

  int* concatInfo = pdu.getConcatInfo();
  int refNumber = concatInfo[0];
  int partNumber = concatInfo[1];
  int totalParts = concatInfo[2];

  logCaptureF("长短信信息: 参考号=%d, 当前=%d, 总计=%d\n", refNumber, partNumber, totalParts);
  logCaptureLn(String("==============="));

  if (totalParts > 1 && partNumber > 0) {
    logCaptureF("[SMS] 收到长短信分段 %d/%d\n", partNumber, totalParts);
    int slot = findOrCreateConcatSlot(refNumber, pdu.getSender(), totalParts);
    int partIndex = partNumber - 1;
    if (partIndex >= 0 && partIndex < MAX_CONCAT_PARTS) {
      if (!concatBuffer[slot].parts[partIndex].valid) {
        concatBuffer[slot].parts[partIndex].valid = true;
        concatBuffer[slot].parts[partIndex].text = String(pdu.getText());
        concatBuffer[slot].receivedParts++;
        if (concatBuffer[slot].receivedParts == 1) {
          concatBuffer[slot].timestamp = String(pdu.getTimeStamp());
        }
        logCaptureF("  已缓存分段 %d，当前已收到 %d/%d\n",
                    partNumber, concatBuffer[slot].receivedParts, totalParts);
      } else {
        logCaptureF("  [!] 分段 %d 已存在，跳过\n", partNumber);
      }
    }

    if (concatBuffer[slot].receivedParts >= totalParts) {
      logCaptureLn(String("[OK] 长短信已收齐，开始合并转发"));
      String fullText = assembleConcatSms(slot);
      processSmsContent(concatBuffer[slot].sender.c_str(),
                        fullText.c_str(),
                        concatBuffer[slot].timestamp.c_str());
      clearConcatSlot(slot);
    }
  } else {
    processSmsContent(pdu.getSender(), pdu.getText(), pdu.getTimeStamp());
  }

  return true;
}

static void deleteStoredSms(int index) {
  String cmd = "AT+CMGD=" + String(index);
  if (!sendATandWaitOK(cmd.c_str(), 2000)) {
    logCaptureLn(String("[!] 删除已处理短信失败，索引: " + String(index)));
  }
}

// 读取 ME 中指定索引的短信并处理；成功后删除，避免重复转发。
static bool readSmsFromStorage(int index) {
  String cmd = "AT+CMGR=" + String(index);
  String resp = sendATCommand(cmd.c_str(), 5000);
  if (resp.indexOf("+CMGR:") < 0) {
    logCaptureLn(String("[!] 读取短信失败，索引: " + String(index)));
    return false;
  }

  int headerPos = resp.indexOf("+CMGR:");
  int headerEnd = resp.indexOf('\n', headerPos);
  if (headerEnd < 0) return false;

  String header = resp.substring(headerPos, headerEnd);
  header.trim();
  int comma1 = header.indexOf(',');
  if (comma1 < 0) return false;
  int stat = header.substring(header.indexOf(':') + 1, comma1).toInt();
  if (stat != 0 && stat != 1) {
    logCaptureLn(String("[!] 跳过非接收短信，索引: " + String(index) +
                       "，状态: " + String(stat)));
    return false;
  }

  int pduStart = headerEnd + 1;
  int pduEnd = resp.indexOf('\n', pduStart);
  String pduLine = pduEnd < 0 ? resp.substring(pduStart) : resp.substring(pduStart, pduEnd);
  pduLine.trim();

  if (pduLine.length() % 2 != 0 || !isHexString(pduLine)) {
    logCaptureLn(String("[!] CMGR返回内容不是有效PDU，索引: " + String(index)));
    return false;
  }

  if (!processPduLine(pduLine)) return false;
  deleteStoredSms(index);
  return true;
}

// 处理一条排队中的短信；每次只处理一条，避免阻塞主循环。
void processPendingSms() {
  if (modemCommandBusy()) return;

  if (pendingPduCount > 0) {
    String pduLine = dequeueSmsPdu();
    if (pduLine.length() > 0) processPduLine(pduLine);
    return;
  }

  if (pendingIndexCount > 0) {
    int index = dequeueSmsIndex();
    if (index > 0) readSmsFromStorage(index);
  }
}

// 定时扫描 ME 中所有短信，补收 URC 丢失、初始化前已到达或重启前遗留的短信。
void pollStoredSms() {
  if (modemCommandBusy()) return;
  if (pendingIndexCount > 0 || pendingPduCount > 0) return;

  String resp = sendATCommand("AT+CMGL=4", 5000);
  if (resp.indexOf("OK") < 0) {
    logCaptureLn(String("[!] 扫描已存短信失败"));
    return;
  }

  // 只收集索引，再按索引从大到小读取/删除。这样即使存储区在删除后
  // 发生重排，也不会因为先删小索引而误删后面的短信。
  const int maxPoll = 16;
  int indexes[maxPoll];
  int count = 0;
  int pos = 0;
  while ((pos = resp.indexOf("+CMGL:", pos)) >= 0) {
    int headerEnd = resp.indexOf('\n', pos);
    if (headerEnd < 0) break;

    String header = resp.substring(pos, headerEnd);
    header.trim();
    int comma1 = header.indexOf(',');
    int comma2 = header.indexOf(',', comma1 + 1);
    if (comma1 < 0 || comma2 < 0) {
      pos = headerEnd + 1;
      continue;
    }

    int index = header.substring(6, comma1).toInt();
    int stat = header.substring(comma1 + 1, comma2).toInt();

    int pduStart = headerEnd + 1;
    int pduEnd = resp.indexOf('\n', pduStart);
    if (pduEnd < 0) pduEnd = resp.length();
    String pduLine = resp.substring(pduStart, pduEnd);
    pduLine.trim();
    pos = pduEnd + 1;

    // 只处理收到的短信（0=未读，1=已读），跳过已发送/草稿。
    if (stat != 0 && stat != 1) continue;
    if (!isHexString(pduLine)) continue;
    if (count < maxPoll) {
      indexes[count++] = index;
    }
  }

  // 降序插入排序；先处理高索引，删除不会影响尚未处理的小索引。
  for (int i = 1; i < count; i++) {
    int key = indexes[i];
    int j = i - 1;
    while (j >= 0 && indexes[j] < key) {
      indexes[j + 1] = indexes[j];
      j--;
    }
    indexes[j + 1] = key;
  }

  int found = 0;
  for (int i = 0; i < count; i++) {
    if (readSmsFromStorage(indexes[i])) found++;
  }

  if (found > 0) {
    logCaptureLn(String("[SMS] 已补收 " + String(found) + " 条短信"));
  }
}

// 主循环调用：只处理普通串口行；URC 交给统一状态机。
void checkSerial1URC() {
  String line = readSerialLine(Serial1);
  if (line.length() == 0) return;
  line.trim();
  if (!handleSmsUrcLine(line)) {
    logCaptureLn(String("Debug> " + line));
  }
}
