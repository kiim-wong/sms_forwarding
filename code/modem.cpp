#include "modem.h"
#include "web_handlers.h"
#include "sms_process.h"

static bool s_modemCommandBusy = false;

bool modemCommandBusy() {
  return s_modemCommandBusy;
}

bool modemLock() {
  if (s_modemCommandBusy) return false;
  s_modemCommandBusy = true;
  return true;
}

void modemUnlock() {
  s_modemCommandBusy = false;
}

// 把串口里已经到达的数据按行处理掉；遇到短信URC时交给解析器，不能直接丢弃。
static void drainModemInput(unsigned long windowMs) {
  unsigned long start = millis();
  do {
    String line = readSerialLine(Serial1);
    if (line.length() > 0) {
      line.trim();
      handleSmsUrcLine(line);
    } else {
      delay(1);
    }
  } while (millis() - start < windowMs);
}

// 调用者必须已经持有模组锁。供需要连续发多条 AT 指令的协议流程使用。
String sendATCommandLocked(const char* cmd, unsigned long timeout) {
  drainModemInput(15);
  Serial1.println(cmd);

  unsigned long start = millis();
  String resp = "";
  bool gotFinal = false;
  while (millis() - start < timeout) {
    String line = readSerialLine(Serial1);
    if (line.length() > 0) {
      line.trim();
      if (!handleSmsUrcLine(line)) {
        resp += line;
        resp += "\r\n";
        if (line == "OK" || line == "ERROR" ||
            line.startsWith("+CME ERROR") || line.startsWith("+CMS ERROR")) {
          gotFinal = true;
          break;
        }
      }
    }
    if (!webLongOperationActive) server.handleClient();
  }

  // 结束码之后可能还有一行URC（例如 +CMTI），短暂续读，避免被下一条命令
  // 的响应误当作自己的内容。
  if (gotFinal) {
    unsigned long tailStart = millis();
    while (millis() - tailStart < 50) {
      String line = readSerialLine(Serial1);
      if (line.length() > 0) {
        line.trim();
        if (!handleSmsUrcLine(line)) {
          resp += line;
          resp += "\r\n";
        }
      } else {
        delay(1);
      }
      if (!webLongOperationActive) server.handleClient();
    }
  }
  return resp;
}

// 发送AT命令并获取响应。命令等待期间到达的短信URC会进入队列，不混入AT响应。
String sendATCommand(const char* cmd, unsigned long timeout) {
  if (!modemLock()) {
    logCaptureLn("[!] 模组正忙，忽略嵌套AT指令");
    return "";
  }

  struct LockGuard {
    ~LockGuard() { modemUnlock(); }
  } guard;

  return sendATCommandLocked(cmd, timeout);
}

// 新增"模组断电重启"函数
void modemPowerCycle() {
  pinMode(MODEM_EN_PIN, OUTPUT);

  logCaptureLn(String("EN 拉低：关闭模组"));
  digitalWrite(MODEM_EN_PIN, LOW);
  delay(1200);  // 关机时间给够

  logCaptureLn(String("EN 拉高：开启模组"));
  digitalWrite(MODEM_EN_PIN, HIGH);
  delay(6000);  // 等模组完全启动再发AT（关键）
}

// 重启模组（EN引脚断电重启 + 重新初始化）
void resetModule() {
  logCaptureLn(String("正在硬重启模组（EN 断电重启）..."));
  modemPowerCycle();
  modemInit();
}

static bool configureOperator() {
  if (config.operatorMode == 0) {
    logCaptureLn(String("选网模式：自动选择"));
    return sendATandWaitOK("AT+COPS=0", 60000);
  }

  bool validCode = config.operatorCode.length() == 5 || config.operatorCode.length() == 6;
  for (unsigned int i = 0; validCode && i < config.operatorCode.length(); i++) {
    validCode = isDigit(config.operatorCode.charAt(i));
  }
  if (!validCode) {
    logCaptureLn(String("[!] 手动选网 PLMN 无效"));
    return false;
  }

  char command[40];
  snprintf(command, sizeof(command), "AT+COPS=1,2,\"%s\",7",
           config.operatorCode.c_str());
  logCaptureLn(String("手动选网 PLMN: ") + config.operatorCode);
  return sendATandWaitOK(command, 60000);
}

// 模组 AT 初始化流程（setup 中调用，resetModule 后也调用）
void modemInit() {
  // 清掉上电噪声/残留
  while (Serial1.available()) Serial1.read();

  int atRetries = 0;
  while (!sendATandWaitOK("AT", 1000)) {
    logCaptureLn(String("AT未响应，重试..."));
    blink_short();
    atRetries++;
    if (atRetries >= 5) {
      logCaptureLn(String("[!] 模组AT握手失败，停止初始化"));
      modemReady = false;
      return;
    }
  }
  logCaptureLn(String("模组AT响应正常"));

  // 判断型号
  String resp = sendATCommand("ATI", 2000);
  logCaptureLn(String("ATI响应: " + resp));
  if (resp.indexOf("OK") >= 0) {
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
  }

  bool storageOk = false;
  for (int retry = 0; retry < 3 && !storageOk; retry++) {
    storageOk = sendATandWaitOK("AT+CPMS=\"ME\",\"ME\",\"ME\"", 2000);
    if (!storageOk) {
      logCaptureLn(String("设置短信存储失败，重试..."));
      blink_short(200);
    }
  }
  if (storageOk) {
    logCaptureLn(String("短信存储设置为ME"));
  } else {
    logCaptureLn(String("[!] 无法设置短信存储，继续尝试接收短信"));
  }

  // mt=1：先存入ME，再上报 +CMTI；这比 mt=2 直接上报 +CMT 可靠，
  // 即使串口短时忙，也能通过 CMGR/CMGL 补收，不会丢短信。
  bool cnmiOk = false;
  for (int retry = 0; retry < 3 && !cnmiOk; retry++) {
    cnmiOk = sendATandWaitOK("AT+CNMI=2,1,0,0,0", 2000);
    if (!cnmiOk) {
      logCaptureLn(String("设置CNMI失败，重试..."));
      blink_short(200);
    }
  }
  if (cnmiOk) {
    logCaptureLn(String("CNMI参数设置完成"));
  } else {
    logCaptureLn(String("[!] CNMI设置失败，将依赖CMGL定时补收"));
  }

  bool cmgfOk = false;
  for (int retry = 0; retry < 3 && !cmgfOk; retry++) {
    cmgfOk = sendATandWaitOK("AT+CMGF=0", 1000);
    if (!cmgfOk) {
      logCaptureLn(String("设置PDU模式失败，重试..."));
      blink_short(200);
    }
  }
  if (cmgfOk) {
    logCaptureLn(String("PDU模式设置完成"));
  } else {
    logCaptureLn(String("[!] PDU模式设置失败，短信接收可能异常"));
  }

  bool operatorReady = configureOperator();
  if (!operatorReady) {
    logCaptureLn(String("[!] 选网命令未成功完成"));
  }

  int ceregRetry = 0;
  while (!waitCEREG() && ceregRetry < 15) {
    logCaptureLn(String("等待网络注册..."));
    ceregRetry++;
    blink_short(200);
  }
  if (ceregRetry < 15) {
    logCaptureLn(String("网络已注册"));
    modemReady = operatorReady;
  } else {
    logCaptureLn(String("[!] 网络注册超时（无SIM卡或信号差），模组功能不可用"));
    modemReady = false;
  }
}

void blink_short(unsigned long gap_time) {
  digitalWrite(LED_BUILTIN, LOW);
  delay(50);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(gap_time);
}

bool sendATandWaitOK(const char* cmd, unsigned long timeout) {
  String resp = sendATCommand(cmd, timeout);
  return resp.indexOf("OK") >= 0;
}

// 检测网络注册状态（LTE/4G）
// CEREG状态: 1=已注册本地, 5=已注册漫游
bool waitCEREG() {
  String resp = sendATCommand("AT+CEREG?", 2500);
  int pos = resp.indexOf("+CEREG:");
  if (pos < 0) return false;

  int comma = resp.indexOf(',', pos);
  if (comma < 0) return false;

  int end = comma + 1;
  while (end < (int)resp.length() && isDigit(resp.charAt(end))) {
    end++;
  }
  int stat = resp.substring(comma + 1, end).toInt();
  return stat == 1 || stat == 5;
}

// 发送短信（PDU模式）
bool sendSMS(const char* phoneNumber, const char* message) {
  if (s_modemCommandBusy) {
    logCaptureLn(String("[!] 模组正忙，无法发送短信"));
    return false;
  }
  struct BusyGuard {
    BusyGuard() { s_modemCommandBusy = true; }
    ~BusyGuard() { s_modemCommandBusy = false; }
  } guard;

  logCaptureLn(String("准备发送短信..."));
  logCapture(String("目标号码: ")); logCaptureLn(String(phoneNumber));
  logCapture(String("短信内容: ")); logCaptureLn(String(message));

  // 使用pdulib编码PDU
  pdu.setSCAnumber();  // 使用默认短信中心
  int pduLen = pdu.encodePDU(phoneNumber, message);

  if (pduLen < 0) {
    logCapture(String("PDU编码失败，错误码: "));
    logCaptureLn(String(pduLen));
    return false;
  }

  logCapture(String("PDU数据: ")); logCaptureLn(String(pdu.getSMS()));
  logCapture(String("PDU长度: ")); logCaptureLn(String(pduLen));

  // 发送AT+CMGS命令
  String cmgsCmd = "AT+CMGS=";
  cmgsCmd += pduLen;

  drainModemInput(15);
  Serial1.println(cmgsCmd);

  // 等待 > 提示符
  unsigned long start = millis();
  bool gotPrompt = false;
  while (millis() - start < 5000) {
    if (Serial1.available()) {
      char c = Serial1.read();
      logCapture(String(c));
      if (c == '>') {
        gotPrompt = true;
        break;
      }
    }
    server.handleClient();
  }

  if (!gotPrompt) {
    logCaptureLn(String("未收到>提示符"));
    return false;
  }

  // 发送PDU数据
  Serial1.print(pdu.getSMS());
  Serial1.write(0x1A);  // Ctrl+Z 结束

  // 等待响应；按行读取，避免把同时到达的 +CMTI 当成 CMGS 响应的一部分吞掉。
  start = millis();
  String resp = "";
  while (millis() - start < 30000) {
    String line = readSerialLine(Serial1);
    if (line.length() > 0) {
      line.trim();
      if (handleSmsUrcLine(line)) {
        continue;
      }
      resp += line;
      resp += "\r\n";
      logCaptureLn(String("Debug> " + line));
      if (line == "OK") {
        logCaptureLn(String("短信发送成功"));
        return true;
      }
      if (line == "ERROR" || line.startsWith("+CME ERROR") || line.startsWith("+CMS ERROR")) {
        logCaptureLn(String("短信发送失败"));
        return false;
      }
    }
    server.handleClient();
  }
  logCaptureLn(String("短信发送超时"));
  return false;
}
