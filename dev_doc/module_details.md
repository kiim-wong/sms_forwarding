# 模块详解 — 逐文件源码指南

---

## code.ino — 主入口

**文件**: `code/code.ino` (约 123 行)
**角色**: Arduino 工程入口点，仅包含 `setup()` 和 `loop()`。

### setup() 初始化顺序

```
1. GPIO 初始化
   ├── pinMode(LED_BUILTIN, OUTPUT); digitalWrite(HIGH)   // LED 灭
   └── (MODEM_EN_PIN 在 modemPowerCycle 内设置)

2. 串口初始化
   ├── Serial.begin(115200); delay(200)                    // USB CDC 稳定
   ├── Serial1.setRxBufferSize(SERIAL_BUFFER_SIZE)         // begin 前扩大接收缓冲
   └── Serial1.begin(115200, SERIAL_8N1, RXD, TXD)         // 模组 UART

3. 模组上电
   ├── modemPowerCycle()                                   // EN 引脚时序
   └── Serial1.read() 清启动噪声

4. 数据加载
   ├── initConcatBuffer()                                  // 长短信缓存清零
   ├── loadConfig()                                        // NVS → config
   ├── initForwardHistory()                                // 挂载 SPIFFS，加载短信记录
   └── configValid = isConfigValid()                       // 校验（只看推送通道）

5. WiFi 连接
   ├── WiFi.mode(WIFI_STA); setSleep(false); setAutoReconnect(true)
   ├── WiFi.setScanMethod(WIFI_FAST_SCAN)                  // 快速扫描
   ├── WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL)
   ├── loadWifiSetting("ssid", WIFI_SSID) / loadWifiSetting("pass", WIFI_PASS)
   │     └── NVS namespace "wifi_config"，空值回退编译期宏
   ├── applyWifiSecurity(loadWifiAuth())                   // 设置最低安全等级
   └── WiFi.begin(ssid, pass)，20 秒超时；失败继续启动，仅 4G 推送和短信接收仍可用

6. HTTP 服务器
   ├── server.on("/", handleRoot)
   ├── server.on("/save", HTTP_POST, handleSave)
   ├── server.on("/history", handleHistory)                  # 短信记录查询/删除
   ├── server.on("/testpush", HTTP_POST, handleTestPush)     # 推送通道测试
   ├── server.on("/pushnet", HTTP_POST, handlePushNetwork)  # 推送网络切换
   ├── server.on("/tools", handleRoot)                       # 兼容旧链接
   ├── server.on("/sms", handleRoot)                         # 兼容旧链接
   ├── server.on("/sendsms", HTTP_POST, handleSendSms)
   ├── server.on("/ping", HTTP_POST, handlePing)
   ├── server.on("/query", handleQuery)
   ├── server.on("/flight", handleFlightMode)
   ├── server.on("/at", handleATCommand)
   ├── server.on("/log", handleLog)                          # 系统日志 JSON
   ├── server.on("/modem", handleModem)
   ├── server.on("/wifi", handleWifi)
   ├── server.on("/system", handleSystem)                    # 整机重启
   └── server.begin()

7. NTP 时间同步
   ├── configTzTime("CST-8", "ntp.ntsc.ac.cn", ...)
   └── 等待 time() > 100000

8. 模组初始化
   └── modemInit()                                         // AT 握手 → CPMS/CNMI → PDU 模式 → 等网络注册；数据上下文按需激活
```

### loop() 执行顺序（每帧执行）

```cpp
server.handleClient();        // 1. 处理HTTP请求
if(!configValid) { ... }      // 2. 配置无效时每秒打印IP
checkConcatTimeout();          // 3. 长短信超时合并转发
if(Serial.available())        // 4. USB->模组透传(单字节)
    Serial1.write(Serial.read());
checkSerial1URC();             // 5. 检查模组URC(短信上报)
processPendingSms();           // 6. 处理 +CMTI 队列（每次最多一条）
pollStoredSms();               // 7. 每60秒用 CMGL 补收遗漏短信
```

### 修改指南

- **新增 HTTP 路由**: 在 `web_handlers.h/.cpp` 添加处理函数，然后在 `setup()` 中 `server.on("/path", handler)`
- **调整初始化顺序**: 直接修改 `setup()` 中的代码顺序即可；模组初始化在 WiFi、HTTP 服务和 NTP 之后调用，目的是让网页先可访问
- **禁用某功能**: 注释掉对应的 `server.on()` 行即可

---

## config_types.h — 类型定义

**依赖**: 仅 `<Arduino.h>`  
**被依赖**: 所有其他模块

### PushType 枚举 (0-11)

表示推送目标平台。值 0 为"未启用"，1-11 对应 11 种推送方式。

**添加新推送类型**:
1. 在 `PushType` 枚举末尾添加新值（如 `PUSH_TYPE_NEW = 12`）
2. 在 `isPushChannelValid()` 添加对应的必填字段校验
3. 在 `sendToChannel()` 添加 `case PUSH_TYPE_NEW:` 实现
4. 在 `handleRoot()` 的 HTML 生成中添加 option
5. 在 HTML JavaScript `updateTypeHint()` 中添加类型提示

### PushChannel 结构体

通用推送通道配置。字段含义:
- `enabled` — 开关
- `type` — PushType 枚举值
- `name` — WebUI 中显示的名称
- `url` — Webhook URL（GET 类型则为目标 URL）
- `key1` — 通用参数 1（token/secret/chat_id 等）
- `key2` — 通用参数 2（channel/bot_token 等）
- `customBody` — 自定义模板的 HTTP body

### Config 结构体

所有可持久化配置的容器，通过 `saveConfig()/loadConfig()` 与 NVS 同步。

| 字段 | 类型 | 说明 |
|---|---|---|
| `adminPhone` | `String` | 管理员手机号，用于 `SMS:` / `RESET` 命令鉴权 |
| `pushChannels[MAX_PUSH_CHANNELS]` | `PushChannel` | 5 个推送通道配置 |
| `webUser` | `String` | Web 登录账号 |
| `webPass` | `String` | Web 登录密码 |
| `numberBlackList` | `String` | 精确号码黑名单（换行分隔） |
| `numberPrefixBlackList` | `String` | 号码前缀黑名单（换行分隔） |
| `numberSuffixBlackList` | `String` | 号码后缀黑名单（换行分隔） |
| `contentBlackList` | `String` | 内容关键词黑名单（换行分隔） |
| `pushNetworkMode` | `PushNetworkMode` | 推送网络：0=WiFi，1=4G，2=WiFi优先+4G兜底 |
| `operatorMode` | `uint8_t` | 选网：0=自动，1=手动 |
| `operatorCode` | `String` | 手动选网 PLMN（5-6 位 MCC+MNC） |

`PushNetworkMode` 枚举：

| 值 | 枚举 | 含义 |
|---|---|---|
| 0 | `PUSH_NET_WIFI` | 仅 WiFi |
| 1 | `PUSH_NET_CELLULAR` | 仅 4G 数据 |
| 2 | `PUSH_NET_WIFI_FALLBACK` | WiFi 优先，传输失败时 4G 兜底 |

### 常量

| 常量 | 值 | 说明 |
|---|---|---|
| `MAX_PUSH_CHANNELS` | 5 | 推送通道数量上限 |
| `MAX_CONCAT_PARTS` | 10 | 长短信最大分段数 |
| `CONCAT_TIMEOUT_MS` | 30000 | 长短信等待超时(ms) |
| `MAX_CONCAT_MESSAGES` | 5 | 同时缓存的长短信组数 |
| `DEFAULT_WEB_USER` | `"admin"` | 默认 Web 账号 |
| `DEFAULT_WEB_PASS` | `"admin123"` | 默认 Web 密码 |

---

## globals.h / globals.cpp — 全局变量

### 全局变量清单

| 变量 | 类型 | 初始化 | 用途 |
|---|---|---|---|
| `config` | `Config` | 默认构造 | 系统全部配置（运行时+持久化） |
| `preferences` | `Preferences` | 默认构造 | NVS 存储接口 |
| `pdu` | `PDU` | `PDU(4096)` | PDU 编解码器（4KB 缓冲区） |
| `server` | `WebServer` | `WebServer(80)` | HTTP 服务器（端口 80） |
| `configValid` | `bool` | `false` | 配置是否有效标志 |
| `timeSynced` | `bool` | `false` | NTP 是否已同步 |
| `lastPrintTime` | `unsigned long` | `0` | 上次打印 IP 的 millis |
| `concatBuffer` | `ConcatSms[5]` | 默认构造 | 长短信合并缓存 |

### 功能说明

- **添加新的全局变量**: 在 `globals.h` 添加 `extern` 声明，在 `globals.cpp` 添加定义
- **添加新的库依赖**: 在 `globals.h` 添加 `#include`，所有模块自动获得该依赖

---

## config.h / config.cpp — 配置管理

### NVS 存储结构

```
Namespace: "sms_config"
├── adminPhone  (String)
├── webUser     (String, 默认 "admin")
├── webPass     (String, 默认 "admin123")
├── numBlkList     (String, 精确号码，换行分隔)
├── numBlkPrefix   (String, 号码前缀，换行分隔)
├── numBlkSuffix   (String, 号码后缀，换行分隔)
├── contentBlkList (String, 内容关键词，换行分隔)
├── pushNetMode  (UChar, 0=WiFi / 1=4G / 2=WiFi优先+4G兜底，默认 0)
├── push0en     (Bool)
├── push0type   (UChar)
├── push0url    (String)
├── push0name   (String)
├── push0k1     (String)
├── push0k2     (String)
├── push0body   (String)
├── push1en ... (同上 × 4)
└── ...
```

> WiFi 连接信息不在 `sms_config` 中，而是独立存放在 namespace `wifi_config`，键为 `ssid` / `pass` / `auth`；详见 `wifi_config.h / wifi_config.cpp`。

### 修改指南

- **添加新配置项**: 在 `Config` 结构体加字段 → `loadConfig()` 加读取（设默认值） → `saveConfig()` 加写入 → `handleSave()` 加表单解析
- **修改校验逻辑**: 编辑 `isPushChannelValid()` 和 `isConfigValid()`

---

## modem.h / modem.cpp — 模组控制

### 串口通信模型

```
ESP32-C3                   4G 模组
  Serial1 (UART) ──────────── AT 端口
  GPIO 5     ──────────── EN 引脚
```

### AT 指令函数对比

| 函数 | 返回类型 | 延时处理 | 适用场景 |
|---|---|---|---|
| `sendATCommand()` | String (完整响应) | 收到 OK/ERROR 后 `delay(50)` | 需要解析响应内容 |
| `sendATCommandLocked()` | String (完整响应) | 同上，但要求调用方已持有模组锁 | MHTTP 多命令事务 |
| `sendATandWaitOK()` | bool | 无额外 delay | 快速幂等检查 |

### 模组电源控制

```
modemPowerCycle():
  EN=HIGH (默认)  模组运行
      ↓
  EN=LOW (1200ms) 模组断电
      ↓
  EN=HIGH (6000ms) 模组上电启动
```

6000ms 是关键时间，太短模组初始化未完成会导致 AT 无响应。

### sendSMS() 时序图

```
ESP32                         模组
  │                            │
  ├─ AT+CMGS=<len>\r\n ──────►│
  │                            │
  │◄───────────────────── >  │  (提示符)
  │                            │
  ├─ PDU hex data + 0x1A ────►│
  │                            │
  │◄──────────────── +CMGS:   │  (成功响应)
  │◄──────────────── OK       │
```

### 修改指南

- **更换模组型号**: 修改 `modemPowerCycle()` 的时序参数，调整 AT 指令序列
- **添加新 AT 功能**: 实现新函数，使用 `sendATCommand()` 或直接操作 `Serial1`
- **调波特率**: 修改 `Serial1.begin()` 参数 + 模组 AT 配置

---

## modem_http.h / modem_http.cpp — 4G MHTTP/HTTPS

**角色**: 为“4G 数据”推送模式提供同步 HTTP/HTTPS 客户端，封装 ML307A 的 MHTTP AT 命令和 PDP 生命周期。它不是 ESP32 的 WiFi HTTPClient，二者由 `push.cpp` 按 `PushNetworkMode` 选择。

### 关键流程

```
modemDataBegin()
  ├─ AT+CGACT? 检查 cid=1
  ├─ 未激活 → AT+CGACT=1,1 → 轮询确认 +CGACT: 1,1
  └─ 已激活 → 标记本次使用结束后尝试关闭

modemHttpRequest()
  ├─ modemLock() 独占串口事务
  ├─ AT+MHTTPDEL=0..3 清理旧实例
  ├─ AT+MHTTPCREATE="<scheme>://<host>[:port]"
  ├─ AT+MHTTPCFG="cached",<id>,1,4096
  ├─ AT+MHTTPCFG="cid",<id>,1
  ├─ AT+MHTTPCFG="timeout",<id>,20,20,20
  ├─ HTTPS: AT+MSSLCFG="auth",0,0 + AT+MHTTPCFG="ssl",<id>,1,0
  ├─ POST: AT+MHTTPHEADER Content-Type + AT+MHTTPCONTENT 数据模式写入原始 body
  ├─ AT+MHTTPREQUEST=<id>,<method>,0,"<path>"
  ├─ 等待 +MHTTPURC: "recv" → AT+MHTTPREAD 读取 header/content
  └─ AT+MHTTPDEL，释放锁

modemDataEnd()
  ├─ 清理 MHTTP 实例
  └─ AT+CGACT=0,1 并轮询确认（模组/网络可能自动重新拉起 cid=1）
```

### 实现约束

- 整个 MHTTP 事务必须持有 `modemLock()`，期间使用 `sendATCommandLocked()`；不能让网页 AT 终端、短信读取或模组控制插入 AT 命令。
- POST body 通过 `AT+MHTTPCONTENT=<id>,0,<byte_length>` 数据模式发送，不能直接拼进带引号的 AT 字符串（ML307A 字符串参数内部不能含引号）。
- 等待 `>` 提示符、`+MHTTPURC` 和 `OK` 时仍要消费短信 URC，不能清空 Serial1。
- 当前 HTTPS 为 `auth=0`：TLS 加密有效，但不校验服务端证书链；使用可信 Webhook 地址，严格校验需额外写入 CA 证书。
- 4G 推送只在选择的模式下启用；WiFi 模式不应触发任何 MHTTP/CGACT 操作。

---

## push.h / push.cpp — 推送服务

### 推送通道执行流程

```
sendSMSToServer(sender, message, timestamp)
  │
  ├─ 检查是否有启用的有效通道
  ├─ 按 config.pushNetworkMode 创建 PushRuntime
  │    ├─ WiFi：仅 HTTPClient
  │    ├─ 4G：modemDataBegin + MHTTP
  │    └─ WiFi 优先：先 WiFi，传输层失败再 4G
  │
  └─ for each valid channel:
       └─ sendToChannelOnRuntime(channel, ..., runtime)
            │
            ├─ jsonEscape() 转义 sender/message/timestamp
            │
            └─ switch(channel.type):
                 ├─ POST_JSON  → POST {sender, message, timestamp}
                 ├─ BARK       → POST {title, body}
                 ├─ GET        → GET ?sender=&message=&timestamp=
                 ├─ DINGTALK   → POST {msgtype:"text", text:{content:"..."}}
                 │               ├─ 有secret → dingtalkSign() 追加URL参数
                 ├─ PUSHPLUS   → POST {token, title, content, channel}
                 │               └─ 默认URL: http://www.pushplus.plus/send
                 ├─ SERVERCHAN → POST title=&desp= (form-urlencoded)
                 │               └─ 默认URL: https://sctapi.ftqq.com/{key1}.send
                 ├─ CUSTOM     → POST (使用 customBody 模板替换)
                 ├─ FEISHU     → POST {timestamp?, sign?, msg_type, content}
                 │               └─ 有secret → HMAC-SHA256签名
                 ├─ GOTIFY     → POST {title, message, priority}
                 │               └─ URL: {url}/message?token={key1}
                 ├─ TELEGRAM   → POST {chat_id, text}
                 │               └─ 默认URL: https://api.telegram.org/bot{key2}/sendMessage
                 └─ WECOM      → POST {"msgtype":"text","text":{"content":"..."}}
```

`sendSMSToServer()` 返回 `PushSummary`，字段为 `attempted`、`succeeded`、`networkAvailable`、`hasEnabledChannel`、`channelCount` 和 `channels`；`channels` 保存每个实际尝试通道的 `{name, success}`，供短信记录展示每个通道的成功/失败。

### HMAC 签名实现

钉钉和飞书都使用 `mbedtls_md` 库（ESP32 内置）:
```cpp
mbedtls_md_context_t ctx;
mbedtls_md_init(&ctx);
mbedtls_md_setup(&ctx, MBEDTLS_MD_SHA256, 1);
mbedtls_md_hmac_starts(&ctx, key, keyLen);
mbedtls_md_hmac_update(&ctx, data, dataLen);
mbedtls_md_hmac_finish(&ctx, hmacResult);
mbedtls_md_free(&ctx);
// hmacResult → base64::encode() → urlEncode()
```

### 修改指南

- **添加新推送通道**: 在 `PushType` 加枚举 → `isPushChannelValid()` 加校验 → `sendToChannel()` 加 case → Web UI 加选项
- **修改钉钉/飞书签名逻辑**: 编辑 `dingtalkSign()` 或 `sendToChannel()` 中 FEISHU case
- **测试推送**: `sendToChannel(..., testMode=true)` 只尝试 1 次，供 `/testpush` 使用；仍然遵守当前 `pushNetworkMode`

---

## history.h / history.cpp — 短信记录

源码内部仍沿用 `ForwardHistoryEntry`、`addForwardHistory()`、`forwardHistoryToJson()` 等命名，这里按产品语义统一称为“短信记录”。

### 职责

维护最近 10 条收到的短信记录，并在 SPIFFS 中持久化。记录范围包括普通转发、黑名单拦截、管理员命令、未配置推送和无可用网络；每条记录保存处理状态、原因，以及实际尝试的推送通道及每个通道的成功/失败。内存中按从旧到新排列，Web 查询时按从新到旧输出。

### 记录文件格式

文件路径 `/forward_history.bin`，文件格式 version 2，全部字段小端：

```
文件头 24 字节：
  magic("SFH1") u32 | version u16(=2) | headerSize u16(=24) | count u16 | reserved u16 |
  nextId u32 | payloadLength u32 | payloadCrc32 u32
记录头 16 字节：
  id u32 | senderLen u16 | timestampLen u16 | messageLen u16 | reasonLen u16 |
  status u8 | channelCount u8 | flags u8 | reserved u8
通道块：channelCount × (name[40] + success u8)
记录体：sender + timestamp + message + reason（均为 UTF-8 原始字节）
```

`payloadCrc32` 覆盖所有记录头、通道块与文本体。`flags` 的 bit0 表示短信内容被截断，其余位必须为 0。保存时先写 `/forward_history.tmp`，成功后再替换正式文件；加载时校验 magic、version 2、headerSize、count、payload 长度与 CRC32，任一失败则重置为空记录并删除文件。

### 状态枚举

| 值 | 状态 | 含义 |
|---|---|---|
| 0 | 已转发 | 实际尝试的通道全部成功 |
| 1 | 部分成功 | 有成功也有失败 |
| 2 | 转发失败 | 有尝试但全部失败 |
| 3 | 黑名单拦截 | 命中号码或内容黑名单，未推送 |
| 4 | 管理员命令 | 管理员 `SMS:` / `RESET` 命令，未推送 |
| 5 | 未配置推送 | 没有启用的有效推送通道 |
| 6 | WiFi 未连接 | 旧记录语义：WiFi 未连接，未尝试推送；新固件不再写入 |
| 7 | 无可用网络 | 当前推送模式没有可用网络，未尝试推送 |

### API

| 函数 | 说明 |
|---|---|
| `initForwardHistory()` | 挂载 SPIFFS 并加载记录文件 |
| `addForwardHistory(sender, timestamp, message, status, reason, channels, channelCount)` | 新增记录（最多 10 条，超长字段按 UTF-8 边界截断） |
| `forwardHistoryCount()` | 返回当前短信记录条数 |
| `deleteForwardHistory(id)` | 按 id 删除记录并持久化 |
| `forwardHistoryToJson()` | 输出 JSON 数组（新→旧，含 `status`/`statusText`/`reason`/`channels`/`truncated`） |

### 修改指南

- **调整保留条数**: 修改 `MAX_FORWARD_HISTORY`
- **调整短信内容上限**: 修改 `HISTORY_MESSAGE_MAX`
- **调整原因字段上限**: 修改 `HISTORY_REASON_MAX`
- **更换存储介质**: 修改 `saveForwardHistory()` / `initForwardHistory()` 中的 SPIFFS 调用

---

## sms_process.h / sms_process.cpp — 短信处理

### 长短信合并状态机

```
┌─────────────────────────────────────────────────────┐
│                  concatBuffer[0..4]                  │
│  ┌─────────┐  ┌─────────┐      ┌─────────┐         │
│  │ Slot 0  │  │ Slot 1  │ ...  │ Slot 4  │         │
│  │ inUse   │  │ inUse   │      │ inUse   │         │
│  │ refNum  │  │ refNum  │      │ refNum  │         │
│  │ total   │  │ total   │      │ total   │         │
│  │ recv'd  │  │ recv'd  │      │ recv'd  │         │
│  │ time    │  │ time    │      │ time    │         │
│  │ parts[] │  │ parts[] │      │ parts[] │         │
│  └─────────┘  └─────────┘      └─────────┘         │
└─────────────────────────────────────────────────────┘
```

### PDU 解析流程

```
checkSerial1URC() / sendATCommand() 共用行解析:
  ┌─ +CMTI: "ME",<index> ─────────────────────────────┐
  │  入索引队列 → processPendingSms() → AT+CMGR       │
  │  → PDU 解析 → AT+CMGD 删除                         │
  └────────────────────────────────────────────────────┘
  ┌─ +CMT: ... + 下一行 PDU ──────────────────────────┐
  │  缓存 PDU → PDU 解析（兼容模组直接上报）           │
  └────────────────────────────────────────────────────┘
  ┌─ 每 60s AT+CMGL=4 ───────────────────────────────┐
  │  先收集索引，按索引降序 CMGR/CMGD，防止存储重排    │
  │  误删未读短信；处理后删除，避免重复转发             │
  └────────────────────────────────────────────────────┘
```

### 黑名单匹配算法

```
原始发送者号码: "+86 138-0013-8000"
  │
  ├─ 去除空格/横线/括号，统一去掉 +86
  │   → "13800138000"
  │
  ├─ 精确号码：相等 → 命中
  ├─ 号码前缀：startsWith → 命中
  └─ 号码后缀：endsWith → 命中

短信内容: "这是贷款推广短信"
  │
  └─ 逐行检查内容关键词:
       "贷款" → 包含 → 命中
       "退订" → 不包含

多个内容关键词按“或”处理，任意一个包含即拦截。
```

### 管理员命令解析

```
短信内容: "SMS:13800138000:你好世界"
  │
  ├─ 第一个 ':'  → "SMS"
  ├─ 第二个 ':'  → "13800138000"  (目标号码)
  └─ 剩余内容    → "你好世界"      (短信内容)
  │
  └─ sendSMS("13800138000", "你好世界")

短信内容: "RESET"
  └─ resetModule() + ESP.restart()
```

### 修改指南

- **调整长短信超时**: 修改 `CONCAT_TIMEOUT_MS` 宏（config_types.h）
- **修改黑名单匹配逻辑**: 编辑 `isInNumberBlackList()` 或 `isInContentBlackList()`
- **添加新管理员命令**: 在 `processAdminCommand()` 中添加 `else if` 分支
- **更换短信库**: 修改 `checkSerial1URC()` 中的 `pdu.decodePDU()` 调用

---

## web_handlers.h / web_handlers.cpp — HTTP 处理 + 日志系统

### 路由表

| 方法 | 路径 | 处理函数 | Auth | 说明 |
|---|---|---|---|---|
| GET | `/` | `handleRoot()` | ✓ | SPA 主页（含 11 个面板） |
| GET | `/tools` | `handleRoot()` | ✓ | 旧链接兼容，返回同一 SPA 页面 |
| GET | `/sms` | `handleRoot()` | ✓ | 旧链接兼容，返回同一 SPA 页面 |
| POST | `/save` | `handleSave()` | ✓ | 保存配置；选网配置变更后重启 |
| GET | `/history` | `handleHistory()` | ✓ | 短信记录 JSON（新→旧） |
| POST | `/history` | `handleHistory()` | ✓ | `action=delete&id=...` 删除单条记录 |
| POST | `/testpush` | `handleTestPush()` | ✓ | 用当前表单参数测试单个推送通道 |
| POST | `/pushnet` | `handlePushNetwork()` | ✓ | 切换 WiFi/4G/WiFi优先推送网络 |
| POST | `/sendsms` | `handleSendSms()` | ✓ | 网页发送短信 |
| POST | `/ping` | `handlePing()` | ✓ | Ping 测试 |
| GET | `/query` | `handleQuery()` | ✓ | 模组/WiFi 信息查询 |
| GET | `/flight` | `handleFlightMode()` | ✓ | 飞行模式控制 |
| GET | `/at` | `handleATCommand()` | ✓ | AT 指令调试 |
| GET | `/log` | `handleLog()` | ✓ | 系统日志（JSON 数组） |
| GET | `/modem` | `handleModem()` | ✓ | 模组重启/信号/运营商/IMEI |
| GET/POST | `/wifi` | `handleWifi()` | ✓ | WiFi 状态/扫描/保存/连接/重启（`?action=...`） |
| GET/POST | `/ap` | `handleAp()` | ✓ | 热点状态/保存/应用（`?action=status|save`） |
| GET | `/system` | `handleSystem()` | ✓ | 整机重启 |

### WiFi 设置（handleWifi）

- `action=status`：返回 `connected`、当前/已保存 SSID、IP、RSSI、`savedAuth` 和 `authText`；密码不回显。
- `action=scan`：同步调用 `WiFi.scanNetworks(false, true)`，同名 SSID 去重并保留最强信号，按 RSSI 降序返回 `ssid` / `rssi` / `auth` / `authText` / `channel`，最多 32 个；`auth` 是 `wifi_auth_mode_t` 原始值，与保存参数 0~4 不同；扫描前已连接、扫描后断开时调用 `WiFi.reconnect()`。
- `action=save`：参数 `ssid` / `pass` / `auth` 写入 NVS `wifi_config`，只保存、不立即切换，返回“重启后生效”。
- `action=connect`：保存后先发响应，再 `WiFi.disconnect(true)` 并 `WiFi.begin()`，最多等待 15 秒；DHCP 可能分配新 IP，旧地址会失效。
- `action=restart`：读取 NVS 已保存凭据重连，不读取页面表单、不写 NVS；对应网络测试页的“重启 WiFi”按钮。

`handleWifi()` 用静态 `busy` 防止重入，忙时返回 HTTP 429。除忙状态外，参数错误和未知 action 也返回 HTTP 200，仅以 `success:false` 表示失败。扫描会同步阻塞其他 HTTP 处理并可能短暂影响当前 WiFi；WiFi 加密方式只设置 STA 最低安全等级，不改变 Web HTTP:80 或 4G HTTPS 证书策略。

### 日志环形缓冲区

```
容量: LOG_BUF_SIZE = 120 行
结构: String logBuffer[120] + 行缓冲 _logLine
写入:
  logCapture(msg)      → Serial.print() + 追加到 _logLine
  logCaptureLn(msg)    → Serial.println() + 提交 _logLine 到环形缓冲区
  logCaptureF(fmt,...) → Serial.print() + 追加到 _logLine (fmt 以 \n 结尾时自动提交)
读取:
  handlerLog()         → JSON ["行1","行2",...] (按插入顺序)
```

**行缓冲设计目的**: 避免 `logCapture("A:"); logCaptureLn(B);` 在环形缓冲区中产生两个独立条目。实际只产生一行 `"A: B"`。

### 模板变量替换

SPA 页面中的 `%PLACEHOLDER%` 在 `handleRoot()` 中通过 `html.replace()` 替换:

| 占位符 | 数据来源 |
|---|---|
| `%IP%` | `WiFi.localIP().toString()` |
| `%WIFI_SSID%` | 已连接时为当前 SSID，否则为 `loadWifiSetting("ssid", WIFI_SSID)` |
| `%WIFI_STATUS_TEXT%` | `已连接` / `未连接` |
| `%WIFI_RSSI_TEXT%` | 已连接时为 `RSSI dBm`，否则为 `--` |
| `%WIFI_SAVED_SSID%` | `loadWifiSetting("ssid", WIFI_SSID)`，WiFi 下拉框初始值 |
| `%WIFI_AUTH_MODE%` | `loadWifiAuth()`（0~4），加密方式下拉框初始值 |
| `%WEB_USER%` / `%WEB_PASS%` | `config.webUser` / `config.webPass` |
| `%ADMIN_PHONE%` | `config.adminPhone` |
| `%NUMBER_BLACK_LIST%` | `config.numberBlackList` |
| `%NUMBER_PREFIX_BLACK_LIST%` | `config.numberPrefixBlackList` |
| `%NUMBER_SUFFIX_BLACK_LIST%` | `config.numberSuffixBlackList` |
| `%CONTENT_BLACK_LIST%` | `config.contentBlackList` |
| `%HISTORY_COUNT%` | `forwardHistoryCount()` |
| `%PUSH_COUNT%` | 已启用（`enabled`）的推送通道数 |
| `%PUSH_NET_MODE%` | `config.pushNetworkMode`（0/1/2） |
| `%PUSH_CHANNELS%` | 循环生成 5 个通道的 HTML 表单 |

### 响应格式

| 端点 | Content-Type | 返回格式 |
|---|---|---|
| `/` `/tools` `/sms` `/save` | `text/html` | HTML 页面 |
| `/query` `/flight` `/at` `/ping` | `application/json` | `{"success":bool, "message":"..."}` |
| `/wifi`（`status` / `scan`） | `application/json` | 状态对象或 `{success,networks[]}`；带 `Cache-Control: no-store` |
| `/wifi`（`save` / `connect` / `restart`） | `application/json` | `{"success":bool,"message":"..."}`；`connect` / `restart` 先响应后切换 |
| `/history`（GET） | `application/json` | 短信记录数组（含状态、原因、实际尝试通道结果） |
| `/history`（POST） `/testpush` `/pushnet` | `application/json` | `{"success":bool, "message":"..."}`；`/pushnet` 额外返回 `mode` |
| `/log` | `application/json` | `["行1", "行2", ...]` |

### 修改指南

- **添加新页面**: 在 `web_handlers.cpp` 添加处理函数 → `setup()` 注册路由
- **修改页面样式**: 编辑 `web_html.cpp` 中的 HTML 模板
- **添加查询类型**: 在 `handleQuery()` 的 if-else 链中添加新 type

---

## web_html.h / web_html.cpp — HTML 模板

### 内容说明

**单页应用 (SPA)**：整个项目仅一个 HTML 常量 `htmlPage`（约 1500 行），包含 11 个面板：

| 面板 ID | 名称 | 功能 |
|---|---|---|
| `panel-overview` | 系统设置 | 显示 IP/信号/短信记录条数/推送通道数，提供 WiFi 连接卡片（扫描/保存/连接）、推送网络三态切换和热点配置（启用开关/SSID/密码，固定 WPA2-AES） |
| `panel-account` | 修改密码 | 修改 Web 登录账号和密码；不在侧边栏导航中 |
| `panel-history` | 短信记录 | 最近 10 条收到的短信，显示状态、原因、实际尝试的通道及成功/失败，支持逐条删除 |
| `panel-push` | 推送通道 | 5 个推送通道配置，每个通道可单独测试 |
| `panel-admin` | 管理员 & 黑名单 | 管理员号码 + 精确/前缀/后缀号码 + 内容关键词 |
| `panel-sendsms` | 发送短信 | Web 端发送短信 |
| `panel-diagnose` | 模组诊断 | ATI/信号/SIM/网络查询 |
| `panel-network` | 网络测试 | Ping 测试 + 重启 WiFi（使用已保存凭据） |
| `panel-modem` | 模组控制 | 飞行模式开关/模组重启 |
| `panel-atterm` | AT 终端 | AT 指令交互调试 |
| `panel-log` | 系统日志 | 实时日志查看（终端风格） |

**侧边栏**分为"配置"和"工具"两个分组，已移除“账号管理”导航项；底部“修改密码”按钮调用 `switchPanel('account')` 打开 `panel-account`，该面板标题为“修改密码”。JS 通过 `switchPanel(name)` 控制面板显示/隐藏，日志面板激活时自动开始轮询，切换离开后停止。

**视觉与响应式**：页面使用系统字体、静态渐变环境光、液体玻璃卡片和毛玻璃侧栏；所有界面图标为内联 SVG 线性图标，使用黑、白、灰 `currentColor`，不依赖 emoji 字体。宽度不超过 820px 时侧栏变为可横向滚动的底部玻璃 Dock。样式包含 `backdrop-filter` 的 `-webkit-` 前缀、无支持回退，以及 `prefers-reduced-motion`、`prefers-reduced-transparency`、`prefers-contrast` 和 `forced-colors` 适配。页面不依赖外部字体、图片或 CDN。

### 修改指南

- 直接编辑 `web_html.cpp` 中的 `R"rawliteral(...)rawliteral"` 块
- HTML 中的 `"` 在 `R"rawliteral"` 中不需要转义
- 需要动态内容的地方使用 `%PLACEHOLDER%`，在对应 handler 中 replace

---

## wifi_config.h / wifi_config.cpp — WiFi 运行时配置

**角色**: 管理 ESP32 STA 的 WiFi 连接信息。`wifi_config.h` 提供编译期兜底宏和函数声明，`wifi_config.cpp` 负责 NVS 读写、安全等级和显示文本。

```cpp
#define WIFI_SSID "<编译期兜底 SSID>"
#define WIFI_PASS "<编译期兜底密码>"
```

源码中的宏默认值是占位字符串，部署前可改成实际 WiFi 信息；更推荐通过 Web 系统设置的 WiFi 卡片保存到 NVS，运行时优先于宏。

### NVS 与加载优先级

```
namespace: "wifi_config"
├── ssid  (String)
├── pass  (String，auth=1 开放网络时为空)
└── auth  (UChar，0=自动 / 1=开放 / 2=WPA-WPA2 / 3=WPA2 / 4=WPA3)
```

运行时优先读取 `wifi_config`；键不存在、值为空或打开失败时回退到 `WIFI_SSID` / `WIFI_PASS`。该 namespace 与 `sms_config` 完全独立，不经过 `Config` 结构体或 `/save`。

### 接口

| 函数 | 说明 |
|---|---|
| `String loadWifiSetting(key, fallback)` | 只读打开 `wifi_config` 读取 String；空值返回 `fallback` |
| `uint8_t loadWifiAuth()` | 读取 `auth`，默认 0，大于 4 时按 0 |
| `void saveWifiConnection(ssid, pass, auth)` | 写入三个键；`auth=1` 时密码写空，越界 `auth` 写 0；无返回值，NVS 打开失败时静默返回 |
| `void applyWifiSecurity(auth)` | 调用 `WiFi.setMinSecurity()` 设置最低安全等级，不是强制精确加密模式 |
| `String wifiAuthChoiceText(auth)` | 下拉框文本：自动 / 开放 / WPA-WPA2 / WPA2 / WPA3 |
| `String wifiAuthScanText(auth)` | 扫描结果原始 `wifi_auth_mode_t` 的显示文本 |

**安全等级映射**: `1` → `WIFI_AUTH_OPEN`；`2` → `WIFI_AUTH_WPA_PSK`；`3` → `WIFI_AUTH_WPA2_PSK`；`4` → `WIFI_AUTH_WPA2_WPA3_PSK`；`0` 或其他 → `WIFI_AUTH_WPA2_PSK`（默认门槛）。

**注意**: 编译期宏只是没有 NVS 配置时的兜底，不应该把真实 WiFi 密码提交到公开仓库。WiFi 加密方式只影响 STA 连接，不改变 Web 管理页面的 HTTP:80，也不影响 4G MHTTP 的 HTTPS/证书策略。

---

## ap_config.h / ap_config.cpp — 设备热点

**角色**: 管理 ESP32 SoftAP 的开启状态、SSID、密码和首次登录提示状态。热点使用 AP+STA 共存，不替代 STA WiFi。

```
namespace: "ap_config"
├── enabled     (Bool，默认 true)
├── ssid        (String，默认 MAC 末 4 位 + "短信转发")
├── pass        (String，默认 12345678)
├── passChanged (Bool，密码不是默认值时 true)
└── promptShown (Bool，首次登录弹窗已显示)
```

`startAccessPoint()` 调用 `WiFi.softAP()`，固定 WPA2-AES、最多 4 个客户端；`stopAccessPoint()` 只关闭 SoftAP 并切回 STA 模式。`passChanged=false` 时页面首次登录弹出修改热点密码提示，热点卡片持续显示默认密码警告；保存非默认密码后置为 true。
