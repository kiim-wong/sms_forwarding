# 系统架构

## 模块依赖关系

| 模块 | 职责 | 主要依赖 |
|---|---|---|
| `config_types.h` | 枚举/结构体/常量 | `<Arduino.h>` |
| `globals.h/.cpp` | 全局变量、引脚定义、公共头文件 | `config_types.h` |
| `config.h/.cpp` | NVS 配置存取、配置校验 | `globals.h`、`web_handlers.h`（日志） |
| `wifi_config.h/.cpp` | WiFi 运行时配置：NVS `wifi_config` 读写、编译期兜底、安全等级选择 | `globals.h`、Preferences、WiFi |
| `modem.h/.cpp` | AT 指令、电源控制、短信发送 | `globals.h` |
| `modem_http.h/.cpp` | ML307A MHTTP/HTTPS 数据通道、PDP 生命周期 | `globals.h`、`modem.h`、`sms_process.h` |
| `push.h/.cpp` | HTTP 推送通道 | `globals.h`、`config.h`、`web_handlers.h`（日志） |
| `history.h/.cpp` | 短信记录持久化（所有收到的短信） | `globals.h`、`push.h`（`jsonEscape`）、`web_handlers.h`（日志）、SPIFFS |
| `sms_process.h/.cpp` | URC 解析、长短信合并、黑名单、管理员命令 | `globals.h`、`push.h`、`history.h`、`web_handlers.h`（日志） |
| `web_handlers.h/.cpp` | HTTP 处理器 + 日志环形缓冲区 | `globals.h`、`config.h`、`wifi_config.h`、`modem.h`、`push.h`、`history.h`、`web_html.h` |
| `web_html.h/.cpp` | SPA HTML 模板 | `<Arduino.h>` |
| `code.ino` | 入口 `setup()` + `loop()` | 所有模块 |

## 数据流总览

### 短信接收流程

```
4G模组 UART
    │
    ▼
checkSerial1URC()          [sms_process.cpp]
    │  逐行读取 Serial1
    │  +CMTI → 记录 ME 索引
    │  +CMT  → 缓存直接上报的 PDU
    │  每 60s CMGL=4 补收遗漏短信
    ▼
AT+CMGR → pdu.decodePDU()  [pdulib 库]
    │  提取 sender / timestamp / text
    │  提取长短信 concatInfo (refNumber, partNumber, totalParts)
    ▼
┌─[长短信? totalParts > 1]─┐
│                           │
▼ 是                     ▼ 否 (普通短信)
findOrCreateConcatSlot()    │
  → 缓存分段                │
  → 收齐后合并              │
  → assembleConcatSms()     │
         │                  │
         └──────┬───────────┘
                ▼
         processSmsContent()
           ├── isAdmin()? → 跳过黑名单
           ├── isInNumberBlackList()? → 记录“黑名单拦截/号码黑名单”后结束
           ├── isInContentBlackList()? → 记录“黑名单拦截/内容关键词”后结束
           ├── isAdmin()? → processAdminCommand()
           │                  ├── "SMS:号码:内容" → sendSMS()
           │                  └── "RESET" → resetModule() + ESP.restart()
           │                  （命令短信记录为“管理员命令”）
           ├── sendSMSToServer()     [push.cpp]
           │     └── sendToChannel() × N
           └── addForwardHistory()   [history.cpp]
                 记录状态与实际尝试的通道结果
```

### HTTP 请求流程

```
浏览器 (SPA 单页应用)
    │  侧边栏切换面板，所有功能在一个 HTML 页面
    │  JS 控制 panel 可见性，无需页面跳转
    ▼
server.handleClient()     [code.ino loop]
    │
    ▼
checkAuth()               [web_handlers.cpp]
    │  HTTP Basic Auth
    │  账号: config.webUser
    │  密码: config.webPass
    ▼
路由分发:
    GET  /         → handleRoot()        SPA 主页 (HTML 模板变量替换，11 个面板；账号面板不在侧边栏，底部“修改密码”打开 panel-account)
    GET  /tools    → handleRoot()        兼容旧链接，返回同一 SPA 页面
    GET  /sms      → handleRoot()        兼容旧链接，返回同一 SPA 页面
    POST /save     → handleSave()        保存配置 → saveConfig() → 重新校验；选网变更后重启
    POST /pushnet  → handlePushNetwork() 切换推送网络模式（WiFi/4G/兜底）
    GET  /history  → handleHistory()     短信记录 JSON（按新→旧排列）
    POST /history  → handleHistory()     action=delete&id=... 删除单条记录
    POST /testpush → handleTestPush()    用当前表单参数测试单个推送通道（不保存）
    POST /sendsms  → handleSendSms()     网页发送短信 → sendSMS()
    POST /ping     → handlePing()        AT+CGACT=1 → MPING → AT+CGACT=0
    GET  /query    → handleQuery()       查询 ATI/CESQ/ICCID/CEREG 等
    GET  /flight   → handleFlightMode()  AT+CFUN 查询/切换飞行模式
    GET  /at       → handleATCommand()   透传 AT 指令到模组
    GET  /log      → handleLog()         返回环形缓冲区日志 (JSON 数组)
    GET  /modem    → handleModem()       模组重启/信号/运营商/IMEI
    GET/POST /wifi → handleWifi()        WiFi 状态/扫描/保存/连接/重启（?action=...）
    GET/POST /ap   → handleAp()          热点状态/保存/应用（?action=...）
    GET  /system   → handleSystem()      整机重启 (ESP.restart)
```

### 推送路由流程

```
sendSMSToServer() / handleTestPush()
    │  读取 config.pushNetworkMode
    ▼
┌─ WiFi ──────────────► HTTPClient（仅 WiFi 可用时）
├─ 4G 数据 ───────────► modemDataBegin()
│                         └─ AT+MHTTPCREATE/MHTTPCFG/MHTTPREQUEST/MHTTPREAD
└─ WiFi 优先 ──────────► 先走 WiFi
                          ├─ 连接建立/DNS/TLS 等传输层失败 → 4G 兜底
                          └─ 4xx/业务错误/读超时 → 不重发
```

### 日志系统架构

```
业务代码调用:
    logCapture("目标号码: ");       → Serial.print() + 追加到 _logLine
    logCaptureLn(String(phone));    → Serial.println() + 提交 _logLine 到环形缓冲区
    logCaptureF("ref=%d\n", ref);    → Serial.print() + 以 \n 结尾时自动提交整行
         │
         ▼
    _logLine (行缓冲区)              // 累积 logCapture 调用直到 logCaptureLn 或 \n
         │
         ▼  logCaptureLn / logCaptureF(\n)
    _logAppend()                    // 写入环形缓冲区
         │
         ▼
    logBuffer[120]                  // 环形缓冲区，循环覆盖
         │
         ▼
    handleLog()                     // GET /log → JSON ["line1","line2",...]
         │
         ▼
    Web 日志面板                   // 每 2 秒自动轮询，终端风格深色主题
```

### 配置持久化流程

```
Web 表单提交 /save
    │
    ▼
handleSave()
    │  解析 POST 参数
    │  赋值到 config 全局结构体
    ▼
saveConfig()              [config.cpp]
    │  Preferences(NVS) 写入
    │  namespace: "sms_config"
    ▼
configValid = isConfigValid()  重新校验
```

### WiFi 配置与连接流程

```
系统设置“WiFi 连接”卡片 / 网络测试“重启 WiFi”
    │
    ├─ GET  /wifi?action=status  → wifiStatusJson()
    ├─ GET  /wifi?action=scan    → WiFi.scanNetworks(false, true)（同步扫描）
    │                               → 同名去重、按 RSSI 降序，最多 32 个
    ├─ POST /wifi?action=save    → saveWifiConnection() → NVS wifi_config
    ├─ POST /wifi?action=connect → 保存 → 先响应 → WiFi.disconnect(true)
    │                               → WiFi.begin() → 最多等待 15s
    └─ GET  /wifi?action=restart → loadWifiSetting() 读取已保存凭据 → 重连（不写 NVS）

NVS wifi_config:
  ssid (String) | pass (String，开放网络写为空) | auth (UChar，0=自动/1=开放/2=WPA-WPA2/3=WPA2/4=WPA3)
```

`wifi_config` 与 `sms_config` 分离；`WIFI_SSID`/`WIFI_PASS` 编译期宏只在 NVS 值为空时兜底。WiFi 设置只影响 ESP32 的 STA 连接（Web 管理入口和 WiFi 推送链路），不改变 Web HTTP:80，也不改变 4G MHTTP 的 HTTPS/证书策略。

启动连接时先扫描并选择匹配 SSID 的最佳物理 BSSID/信道，再调用 `WiFi.begin(ssid, pass, channel, bssid)`，以规避双频合一 SSID 的 5G 虚拟 AP；扫描失败时回退普通 `WiFi.begin()`。

### 选网流程

```
POST /save (operatorMode / operatorCode)
    ├─ 自动：配置 operatorMode=0，设备重启
    └─ 手动：校验 5-6 位 PLMN → 配置 operatorMode=1 → 设备重启

modemInit:
    auto   → AT+COPS=0
    manual → AT+COPS=1,2,"<PLMN>",7
```

### 设备热点流程

```
系统设置“热点配置”卡片
    │
    ├─ GET  /ap?action=status → loadAp*() + WiFi.softAPIP()/softAPgetStationNum()
    └─ POST /ap?action=save   → saveApConfig() → startAccessPoint()/stopAccessPoint()

NVS ap_config:
  enabled (Bool，默认 true) | ssid (默认 MAC 末 4 位 + 短信转发)
  pass (默认 12345678) | passChanged (Bool) | promptShown (Bool)

AP+STA：热点不替代 STA；关闭热点只关闭 SoftAP，不关闭 STA WiFi。
默认加密固定 WPA2-AES；首次登录在 passChanged=false 时显示修改密码提示。
```

### 短信记录持久化流程

```
短信处理完成（普通转发 / 黑名单拦截 / 管理员命令 / 未配置推送 / 无可用网络）
    │
    ▼
addForwardHistory(sender, timestamp, message, status, reason, channels, channelCount)
    │  超长字段按 UTF-8 边界截断；超过 10 条时丢弃最旧记录
    │  channels 记录本次实际尝试的推送通道及每个通道的成功/失败
    ▼
saveForwardHistory()
    │  写入 /forward_history.tmp → 成功后重命名为 /forward_history.bin
    ▼
initForwardHistory()（下次启动）
    │  校验文件头 magic/version/count/长度/CRC32
    └── 校验失败 → 清空记录并删除文件
```

## 线程/任务模型

单线程事件循环（标准 Arduino 模型），所有操作在主 loop 中串行执行：

| 操作 | 频率 | 超时保护 |
|---|---|---|
| HTTP 请求处理 | 每帧 | 常规请求非阻塞；WiFi 扫描/连接见下行 |
| WiFi 扫描 / 保存并连接 / 重启 WiFi | 按请求 | 同步阻塞；扫描时长取决于 AP 数量，连接/重启最多等待 15s |
| 长短信超时检查 | 每帧 | 30s 超时自动转发不完整消息 |
| Serial→Serial1 透传 | 每帧 | 无（单字节读） |
| Serial1 URC 检查 | 每帧 | 逐行读取，无行则立即返回 |
| 待读短信处理 | 每帧最多 1 条 | 单条 CMGR/CMGD 有超时 |
| ME 已存短信补收 | 每 60s | CMGL 5s 超时；收到 URC 时跳过 |
| 配置无效告警 | 每秒 | 仅 configValid=false 时 |

## 内存管理

- ESP32-C3 可用内存：~327KB
- 全局变量占用：~43KB
- PDU 缓冲区：4096 字节（`pdu = PDU(4096)`）
- 长短信缓存：5 组 × 10 段，每段内容动态分配（`String`）
- 短信记录：最多 10 条，单条消息上限 1024 字节，内存数组与 SPIFFS 文件同步
- 日志环形缓冲区：120 行 × String，约 12KB
- 日志行缓冲区：1 个 String（`_logLine`），用于合并 logCapture 输出
- 串口接收缓冲：4096 字节（`SERIAL_BUFFER_SIZE`），降低推送阻塞期间短信 URC 溢出风险
- HTTP 响应在函数内栈分配，调用结束自动释放

## 关键硬件引脚

| 定义 | GPIO | 功能 |
|---|---|---|
| `TXD` | 3 | 模组 UART TX |
| `RXD` | 4 | 模组 UART RX |
| `MODEM_EN_PIN` | 5 | 模组 EN 使能（LOW=关机, HIGH=开机） |
| `LED_BUILTIN` | 8 | 板载 LED（LOW=亮） |

## 错误处理策略

- **模组 AT 无响应**：重试（最多 10 次），LED 闪烁指示
- **WiFi 连接失败**：等待 20 秒后继续启动，短信接收和仅 4G 推送仍可工作
- **WiFi 扫描/切换**：扫描同步阻塞并可能短暂断开当前连接，扫描结束后尝试 `WiFi.reconnect()`；`connect`/`restart` 可能改变 DHCP IP，使旧地址失效；`connect` 先返回接受结果，15 秒内未连上仅记录日志并继续后台尝试
- **WiFi 与 HTTPS 无关**：WiFi 加密方式只设置 STA 最低安全等级，不改变 Web HTTP:80 或 4G MHTTP `auth=0` 的证书策略
- **NTP 同步失败**：记录日志，后续推送使用设备时间
- **HTTP 推送失败**：常规推送最多尝试 2 次（退避 0.5s），测试推送 1 次；4xx 配置类错误不重试；钉钉/飞书/PushPlus/Server酱/Telegram/企业微信额外校验响应体业务码
- **推送网络**：WiFi 模式不碰 4G；4G 模式通过 ML307A MHTTP/HTTPS 发送；WiFi 优先仅在连接建立/DNS/TLS 等传输层失败时切换 4G，读超时、HTTP 4xx 和业务错误不重发
- **PDU 解析失败**：记录日志，丢弃该条短信
- **长短信超时**：强制转发已收到的分段（标记缺失段）
- **短信记录文件损坏**：文件头、长度或 CRC32 校验失败时清空记录并删除文件
- **配置无效**：每秒打印设备 IP 提示用户配置
