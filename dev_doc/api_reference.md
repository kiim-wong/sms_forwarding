# API 参考手册 — 完整函数索引

---

## 模块: config.cpp — 配置管理

### `void saveConfig()`
**用途**: 将 `config` 全局结构体所有字段写入 NVS。

**NVS Namespace**: `"sms_config"`  
**写入模式**: `false`（读写模式）

**写入的 Key 列表**:
| NVS Key | 类型 | 对应字段 |
|---|---|---|
| `adminPhone` | String | `config.adminPhone` |
| `webUser` | String | `config.webUser` |
| `webPass` | String | `config.webPass` |
| `numBlkList` | String | `config.numberBlackList` |
| `numBlkPrefix` | String | `config.numberPrefixBlackList` |
| `numBlkSuffix` | String | `config.numberSuffixBlackList` |
| `contentBlkList` | String | `config.contentBlackList` |
| `pushNetMode` | UChar | `config.pushNetworkMode`（0=WiFi，1=4G，2=WiFi优先+4G兜底） |
| `operatorMode` | UChar | `config.operatorMode`（0=自动选网，1=手动选网） |
| `operatorCode` | String | `config.operatorCode`（5-6 位 PLMN） |
| `push{i}en` | Bool | 通道 i 是否启用 |
| `push{i}type` | UChar | 通道 i 推送类型 |
| `push{i}url` | String | 通道 i URL |
| `push{i}name` | String | 通道 i 名称 |
| `push{i}k1` | String | 通道 i 参数1 |
| `push{i}k2` | String | 通道 i 参数2 |
| `push{i}body` | String | 通道 i 自定义模板 |

---

### `void loadConfig()`
**用途**: 从 NVS 加载所有配置到 `config` 结构体。

**默认值**:
- `webUser`: `"admin"` (`DEFAULT_WEB_USER`)
- `webPass`: `"admin123"` (`DEFAULT_WEB_PASS`)
- 通道名称: `"通道1"` ~ `"通道5"`
- 通道类型: `PUSH_TYPE_POST_JSON` (1)
- `pushNetworkMode`: `PUSH_NET_WIFI` (0)

---

### `bool isPushChannelValid(const PushChannel& ch)`
**用途**: 检查单个推送通道配置是否完整可用。

**校验规则**:

| 推送类型 | 必填字段 |
|---|---|
| POST_JSON / BARK / GET / DINGTALK / FEISHU / CUSTOM / WECOM | `url` 非空 |
| PUSHPLUS / SERVERCHAN | `key1` 非空 |
| GOTIFY | `url` 非空 **且** `key1` 非空 |
| TELEGRAM | `key1` 非空 **且** `key2` 非空 |

**前提**: `ch.enabled == true`，否则直接返回 false。

---

### `bool isConfigValid()`
**用途**: 检查系统是否至少配置了一个可用的推送通道。

**返回 true 条件**: 至少一个推送通道通过 `isPushChannelValid()` 校验。

---

### `String getDeviceUrl()`
**返回**: `"http://{WiFi.localIP}/"`

---

## 模块: wifi_config.h / wifi_config.cpp — WiFi 运行时配置

`wifi_config.h` 定义编译期兜底宏并声明接口；`wifi_config.cpp` 直接读写 NVS namespace `"wifi_config"`，与 `sms_config` 完全分离。运行时优先使用 NVS 中的 WiFi 连接信息，编译期宏只在 NVS 值为空时兜底。

**NVS 键**:

| NVS Key | 类型 | 说明 |
|---|---|---|
| `ssid` | String | WiFi SSID |
| `pass` | String | WiFi 密码；`auth=1`（开放网络）时写入空字符串 |
| `auth` | UChar | 0=自动，1=开放，2=WPA-WPA2，3=WPA2，4=WPA3；越界按 0 处理 |

### `String loadWifiSetting(const char* key, const char* fallback)`
**用途**: 以只读方式打开 `wifi_config`，读取指定 String 键。

**返回**: NVS 值非空时返回该值；键不存在、值为空或 `Preferences.begin()` 失败时返回 `fallback`。空字符串因此会被视为“未配置”。

---

### `uint8_t loadWifiAuth()`
**用途**: 读取 `auth`，默认 0；值大于 4 时返回 0。

---

### `void saveWifiConnection(const String& ssid, const String& pass, uint8_t auth)`
**用途**: 以读写方式打开 `wifi_config`，写入 `ssid`、`pass`、`auth`。

- `auth == 1` 时 `pass` 写为空字符串；
- `auth > 4` 时按 0 写入；
- 函数返回 `void`，`Preferences.begin()` 失败时静默返回，调用方无法从返回值判断是否真正写入。

---

### `void applyWifiSecurity(uint8_t auth)`
**用途**: 调用 `WiFi.setMinSecurity()` 设置最低可接受安全等级，不是强制 AP 使用某一精确加密模式。

| auth | 设置的最低安全等级 |
|---|---|
| 1 | `WIFI_AUTH_OPEN` |
| 2 | `WIFI_AUTH_WPA_PSK` |
| 3 | `WIFI_AUTH_WPA2_PSK` |
| 4 | `WIFI_AUTH_WPA2_WPA3_PSK` |
| 0 或其他 | `WIFI_AUTH_WPA2_PSK`（默认门槛） |

---

### `String wifiAuthChoiceText(uint8_t auth)`
返回 WiFi 卡片下拉框使用的中文文本：1=开放（无密码）、2=WPA / WPA2、3=WPA2、4=WPA3、其他=自动。

### `String wifiAuthScanText(uint8_t auth)`
把扫描结果中的 ESP32 `wifi_auth_mode_t` 原始值转换为显示文本：0=开放、1=WEP、2=WPA、3=WPA2、4=WPA / WPA2、5=WPA2 企业、6=WPA3、7=WPA2 / WPA3，其他=未知。

---

## 模块: ap_config.h / ap_config.cpp — 设备热点

**NVS Namespace**: `"ap_config"`，与 `sms_config`、`wifi_config` 分离。

| NVS Key | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `enabled` | Bool | `true` | 是否启用 SoftAP |
| `ssid` | String | MAC 末 4 位 + `短信转发` | 热点 SSID |
| `pass` | String | `12345678` | 热点密码（8-63 位） |
| `passChanged` | Bool | `false` | 密码不是默认值时 true |
| `promptShown` | Bool | `false` | 首次登录弹窗是否已显示 |

核心函数：

- `defaultApSsid()`：从 `WiFi.macAddress()` 生成默认 SSID；
- `loadApEnabled()` / `loadApSsid()` / `loadApPassword()` / `loadApPasswordChanged()` / `loadApPromptShown()`：读取热点配置；
- `saveApConfig(enabled, ssid, pass)`：写入 NVS，并根据密码是否等于默认值更新 `passChanged`；
- `startAccessPoint()`：`WiFi.mode(WIFI_AP_STA)` + `WiFi.softAP(..., 1, 0, 4)`，WPA2-AES；
- `stopAccessPoint()`：关闭 SoftAP 并回到 `WIFI_STA`；
- `markApPromptShown()`：记录首次登录提示已显示。

---

## 模块: modem.cpp — 模组控制

### `String sendATCommand(const char* cmd, unsigned long timeout)`
**参数**:
- `cmd`: AT 指令字符串（不含 `\r\n`，函数自动追加）
- `timeout`: 等待超时（毫秒）

**行为**:
1. 先读取串口已有整行，识别并缓存 `+CMTI` / `+CMT` 短信 URC（不清空）
2. 发送 `cmd\r\n`
3. 按行收集响应，遇到 OK/ERROR/CME/CMS ERROR 结束
4. 收到结束码后额外读取约 50ms，避免紧随其后的 URC 污染下一条命令
5. 命令执行期间通过忙标志阻止嵌套 AT 命令

**返回**: 模组响应字符串（含 OK/ERROR；短信 URC 不混入响应）

---

### `bool modemLock()` / `void modemUnlock()`
**用途**: 获取/释放模组 AT 串行锁。

`sendATCommand()` 自动加锁；MHTTP 等需要连续发送多条命令的事务由调用方先 `modemLock()`，事务期间调用 `sendATCommandLocked()`，结束后 `modemUnlock()`。

### `String sendATCommandLocked(const char* cmd, unsigned long timeout)`
**用途**: 已持锁版本的 AT 命令发送函数，仅允许在 `modemLock()` 成功后调用；行为与 `sendATCommand()` 的行解析、短信 URC 消费和尾部续读一致。

---

### `void modemPowerCycle()`
**行为**:
1. 配置 `MODEM_EN_PIN` 为输出
2. 拉低 1200ms → 关闭模组
3. 拉高 6000ms → 开启模组并等待启动

**注意**: 重启后由 `modemInit()` 统一处理串口残留和 URC，不要在业务代码里直接清空 Serial1。

---

### `void resetModule()`
**行为**:
1. 调用 `modemPowerCycle()`
2. 调用 `modemInit()` 重新执行完整的 AT 初始化

---

### `bool sendATandWaitOK(const char* cmd, unsigned long timeout)`
**与 sendATCommand 区别**: 内部复用 `sendATCommand()`，仅返回响应中是否包含 `OK`，用于初始化流程中的幂等检查。

---

### `bool waitCEREG()`
**用途**: 轮询 `AT+CEREG?` 直到网络注册成功。

**注册状态码**:
- `1`: 已注册本地网络 → true
- `5`: 已注册漫游 → true
- `0,2,3,4`: 未注册 → false

**超时**: 单次轮询 2000ms（调用方在外层循环调用）

---

### `void blink_short(unsigned long gap_time = 500)`
**行为**: LED 亮 50ms → 灭 → 等待 `gap_time` ms。

---

### `bool sendSMS(const char* phoneNumber, const char* message)`
**流程**:
1. `pdu.setSCAnumber()` 使用默认短信中心
2. `pdu.encodePDU(phoneNumber, message)` 编码
3. 发送 `AT+CMGS=<pduLen>`
4. 等待 `>` 提示符（5 秒超时）
5. 发送 PDU 数据 + `Ctrl+Z` (0x1A)
6. 等待 OK/ERROR（30 秒超时）

**返回**: true=成功, false=PDU编码失败/无提示符/ERROR/超时

---

## 模块: modem_http.cpp — 4G MHTTP/HTTPS

### `struct ModemHttpResponse`
字段：`bool gotResponse`、`int statusCode`、`String headers`、`String body`、`String error`。`gotResponse=false` 表示未取得 HTTP 响应，具体原因在 `error`。

### `bool modemDataBegin(bool& activatedByUs)`
**用途**: 为一次 4G 推送准备 PDP 上下文（cid=1）。

- 查询 `AT+CGACT?`；已激活时也标记 `activatedByUs=true`，以便本次推送结束后尝试关闭；
- 未激活时执行 `AT+CGACT=1,1`，并轮询确认 `+CGACT: 1,1`；
- 返回 true 表示 4G 数据可用。

### `void modemDataEnd(bool activatedByUs)`
**用途**: 结束 4G 推送后清理。

- 删除 `0..3` 号 MHTTP 实例；
- 对 cid=1 执行 `AT+CGACT=0,1` 并轮询确认；
- 某些模组/网络会自动重新拉起 cid=1，因此主要目的是释放本次 MHTTP 会话；日志会报告关闭结果。

### `bool modemHttpRequest(const String& url, bool isGet, const String& contentType, const String& body, ModemHttpResponse& response)`
**流程**:
1. 解析 `http://` 或 `https://` URL，分离 origin 与 path/query；
2. 获取模组事务锁；
3. 清理旧 MHTTP 实例并执行 `AT+MHTTPCREATE`；
4. 配置缓存、cid=1、超时、HTTPS（`ssl=1`，SSL id 0）和 `Content-Type`；
5. POST 时使用 `AT+MHTTPCONTENT` 数据模式写入原始 JSON/表单体，避免 ML307A 字符串参数不能内含引号的问题；
6. 执行 `AT+MHTTPREQUEST`，等待 `+MHTTPURC: "recv"`，用 `AT+MHTTPREAD` 读取响应；
7. 删除实例并释放锁。

当前 HTTPS 通过 `AT+MSSLCFG="auth",0,0` 使用无证书认证（TLS 加密但不校验证书链）。需要服务端证书校验时必须先写入 CA 证书并调整 SSL 配置。

---

## 模块: push.cpp — 推送服务

### `PushSummary sendSMSToServer(const char* sender, const char* message, const char* timestamp)`
**返回**: `PushSummary { uint8_t attempted; uint8_t succeeded; bool networkAvailable; bool hasEnabledChannel; uint8_t channelCount; PushChannelResult channels[MAX_PUSH_CHANNELS]; }`

`PushChannelResult` 为 `{ char name[40]; bool success; }`，记录单个通道的名称与本次推送是否成功。

**行为**:
1. 检查是否有启用的有效通道；
2. 按 `config.pushNetworkMode` 准备路由（WiFi、4G 或 WiFi 优先）；
3. 用 `formatSmsTimestamp()` 将 pdulib 的 `YYMMDDHHMMSS(+时区)` 转为 `YYYY-MM-DD HH:MM:SS`；
4. 遍历所有通道，对每个有效通道执行同一路由上下文中的 `sendToChannelOnRuntime()`；
5. 通道间 delay(100ms)；
6. 返回尝试数、成功数、`networkAvailable`、`hasEnabledChannel`，以及每个实际尝试通道的名称与成功/失败结果（供 `addForwardHistory()` 记录）。

`networkAvailable=false` 表示当前模式没有可用网络（例如 Wi-Fi 模式但 Wi-Fi 未连接，或 4G 激活失败），此时不会逐个尝试通道。

---

### `bool sendToChannel(const PushChannel& channel, const char* sender, const char* message, const char* timestamp, bool testMode = false)`
**核心推送函数**。根据 `channel.type` 构建对应的 HTTP 请求参数（URL/ContentType/Body），统一交给 `executeChannelRequest()` 发送（内置重试）。

**网络路由**: 每次调用创建 `PushRuntime`，读取 `config.pushNetworkMode`。WiFi 模式只使用 `HTTPClient`；4G 模式只使用 ML307A MHTTP；WiFi 优先模式先走 WiFi，仅在传输层失败时切换 4G。

**测试模式**: `testMode = true` 时只尝试 1 次，不重试；测试推送仍遵守当前推送网络模式，`/testpush` 不会绕过路由。

**签名相关**:
- 钉钉: `HMAC-SHA256(timestamp+"\n"+secret)` → Base64 → URLEncode → 追加到 URL
- 飞书: `HMAC-SHA256(timestamp+"\n"+secret)` → Base64 → 放入 JSON body

**占位符**: 自定义模板（PUSH_TYPE_CUSTOM）支持 `{sender}` `{message}` `{timestamp}` 占位符替换。

---

### `bool executeChannelRequest(...)` — 通道请求执行器（带重试）
**重试策略**: 参数 `maxAttempts` 控制最大尝试次数；常规推送为 2 次，测试推送为 1 次。失败后递增退避（第 2 次前 0.5s）。

| 结果 | 处理 |
|---|---|
| 连接失败/超时（`httpCode <= 0`） | 记录错误 → 按策略重试；WiFi 优先仅对非读超时的传输层错误切换 4G |
| HTTP 2xx 且响应体业务码正确 | 成功返回 true |
| HTTP 4xx（除 429） | 配置问题，立即放弃不重试 |
| HTTP 5xx / 429 / 业务码不符 | 重试 |
| 达到最大尝试次数仍失败 | 返回 false，日志提示已丢弃 |

**WiFi 优先兜底边界**: 仅 `http.begin()` 失败、连接建立/DNS/TLS 等传输层错误允许切换 4G；`HTTPC_ERROR_READ_TIMEOUT`、HTTP 4xx、平台业务错误不自动重发，避免服务器已收到请求后重复推送。

### `bool isBodySuccess(const PushChannel& channel, const String& resp)` — 响应体业务码校验
部分平台无论成败 HTTP 都返回 200，需检查响应体：

| 平台 | 成功标志 |
|---|---|
| 钉钉 | `"errcode":0` |
| 飞书 | `"code":0` |
| PushPlus | `"code":200` |
| Server酱 | `"code":0` |
| Telegram | `"ok":true` |
| 企业微信机器人 | `"errcode":0` |
| 其余平台 | 不检查，以 HTTP 状态码为准 |

---

### `String urlEncode(const String& str)`
标准 URL 编码（空格 → `+`, 非字母数字 → `%XX`）

---

### `String jsonEscape(const String& str)`
JSON 字符串转义：`"` → `\"`, `\` → `\\`, `\n` → `\\n`, `\r` → `\\r`, `\t` → `\\t`

### `String formatSmsTimestamp(const String& rawTimestamp)`
把 pdulib 的 `YYMMDDHHMMSS(+时区)` 原始时间转换为人类可读格式，例如 `26103118103632` → `2026-10-31 18:10:36`。如果输入已经是 `YYYY-MM-DD HH:MM:SS` 则原样返回；历史记录加载、新增和 JSON 输出也会调用该函数，兼容旧数据。

---

### `String dingtalkSign(const String& secret, int64_t timestamp)`
HMAC-SHA256(timestamp + "\n" + secret, secret) → Base64 → URLEncode

---

### `int64_t getUtcMillis()`
通过 `gettimeofday()` 获取 UTC 毫秒级时间戳，失败则回退 `time(nullptr) * 1000`

---

## 模块: history.cpp — 短信记录

源码内部仍沿用 `ForwardHistoryEntry`、`addForwardHistory()`、`forwardHistoryToJson()` 等命名，本文按产品语义统一称为“短信记录”。

### 常量与结构体

| 名称 | 值 | 说明 |
|---|---|---|
| `MAX_FORWARD_HISTORY` | 10 | 最多保留的短信记录数 |
| `HISTORY_MESSAGE_MAX` | 1024 | 单条短信内容最大字节数 |
| `HISTORY_SENDER_MAX` | 64 | 发送者字段最大字节数（内部常量） |
| `HISTORY_TIMESTAMP_MAX` | 40 | 时间戳字段最大字节数（内部常量） |
| `HISTORY_REASON_MAX` | 48 | 原因字段最大字节数（内部常量） |

`ForwardHistoryEntry` 字段：`id`、`sender`、`timestamp`、`message`、`status`、`reason`、`channelCount`、`channels[MAX_PUSH_CHANNELS]`、`truncated`。其中 `channels` 的每个元素为 `{ name[40], success }`。

状态枚举 `SmsHistoryStatus`：

| 值 | 状态 | 含义 |
|---|---|---|
| 0 | 已转发 | 实际尝试的通道全部成功 |
| 1 | 部分成功 | 有成功也有失败 |
| 2 | 转发失败 | 有尝试但全部失败 |
| 3 | 黑名单拦截 | 命中号码或内容黑名单，未推送 |
| 4 | 管理员命令 | 管理员 `SMS:` / `RESET` 命令，未推送 |
| 5 | 未配置推送 | 没有启用的有效推送通道 |
| 6 | WiFi 未连接 | 旧记录语义：WiFi 未连接，未尝试推送；新固件不再写入此状态 |
| 7 | 无可用网络 | 当前推送模式没有可用网络（WiFi 未连接或 4G 激活失败），未尝试推送 |

### 记录文件格式

持久化路径为 SPIFFS `/forward_history.bin`，文件格式 version 2，全部字段小端：

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

`payloadCrc32` 覆盖所有记录头、通道块与文本体。`flags` 的 bit0 表示短信内容被截断，其余位必须为 0。保存时先写 `/forward_history.tmp`，成功后再替换为正式文件。

---

### `void initForwardHistory()`
**用途**: 启动时初始化短信记录模块。

**行为**:
1. 清空内存中的记录数组与计数器；
2. `SPIFFS.begin(true)` 挂载文件系统（失败则记录仅保留在内存中）；
3. 读取 `/forward_history.bin`，校验 magic、version 2、文件头长度、条数、payload 长度与 CRC32；检测到 version 1 旧格式时按不兼容处理；
4. 校验失败时清空记录并删除损坏文件。

---

### `void addForwardHistory(const char* sender, const char* timestamp, const char* message, uint8_t status, const char* reason, const PushChannelResult* channels, uint8_t channelCount)`
**用途**: 新增一条短信记录并立即持久化。

**行为**:
- `sender`、`timestamp`、`message`、`reason` 分别按 64 / 40 / 1024 / 48 字节上限截断，截断时保持 UTF-8 字符边界；
- `status` 写入记录状态；非法状态按“转发失败”记录；
- `channels` 保存本次实际尝试的通道名称与成功/失败结果，`channelCount` 最多 5 个；
- 记录数达到 10 条后丢弃最旧记录；
- `truncated` 仅表示短信正文被截断。

---

### `int forwardHistoryCount()`
**返回**: 当前短信记录条数。

---

### `bool deleteForwardHistory(uint32_t id)`
**用途**: 按 `id` 删除单条记录并持久化。

**返回**: true=删除成功，false=记录不存在。

---

### `String forwardHistoryToJson()`
**返回**: JSON 数组，按从新到旧排列。每项字段：

```json
{"id":2,"sender":"+8613800138000","timestamp":"2026-01-01 12:00:00","message":"...","status":1,"statusText":"部分成功","reason":"","channels":[{"name":"通道1","success":true},{"name":"通道2","success":false}],"truncated":false}
```

`channels` 只包含本次实际尝试的通道；黑名单拦截、管理员命令、未配置推送、无可用网络等未推送的记录为空数组。JSON 不包含 `channelCount` 字段，通道数由 `channels` 数组长度体现。普通转发记录的 `reason` 为空字符串，黑名单拦截为“号码黑名单”或“内容关键词”，管理员命令为“管理员命令”。

---

## 模块: sms_process.cpp — 短信处理

### `void initConcatBuffer()`
将 `concatBuffer[MAX_CONCAT_MESSAGES]` 全部标记为未使用，清空所有分段。

---

### `int findOrCreateConcatSlot(int refNumber, const char* sender, int totalParts)`
**查找逻辑**:
1. 遍历缓冲区，找已用且 refNumber+sender 匹配的槽位 → 直接返回
2. 遍历缓冲区，找未使用槽位 → 初始化并返回
3. 无空闲槽位 → 覆盖最老的槽位（按 firstPartTime 比较）

---

### `String assembleConcatSms(int slot)`
按顺序拼接 `concatBuffer[slot].parts[0..totalParts-1]`，缺失分段标记 `[缺失分段N]`

---

### `void clearConcatSlot(int slot)`
将该槽位所有字段复位。

---

### `void checkConcatTimeout()`
遍历所有使用中的槽位，若 `millis() - firstPartTime >= 30000ms`，强制合并转发不完整消息，并清空槽位。

---

### `String readSerialLine(HardwareSerial& port)`
逐字节读取串口，遇 `\n` 返回行，`\r` 跳过，超长行保护（超过 SERIAL_BUFFER_SIZE 归零），无完整行返回空串。

**注意**: 使用 `static` 缓冲区，仅适合单线程调用。

---

### `bool isHexString(const String& str)`
检查字符串是否全为十六进制字符 `[0-9A-Fa-f]`。

---

### `bool isInNumberBlackList(const char* sender)`
**匹配逻辑**: 号码会先去掉空格、横线、括号和 `+86` 前缀，再逐行检查三个列表：
- `config.numberBlackList`：完整号码相等；
- `config.numberPrefixBlackList`：号码前缀相等；
- `config.numberSuffixBlackList`：号码后缀相等。

任意一个列表命中即返回 `true`。

---

### `bool isInContentBlackList(const char* text)`
逐行读取 `config.contentBlackList`，忽略大小写检查短信正文是否包含任意关键词。多个关键词之间是“或”关系，命中任意一个即返回 `true`。

---

### `bool isAdmin(const char* sender)`
比较发送者与 `config.adminPhone`，忽略 `+86` 前缀差异。

---

### `void processAdminCommand(const char* sender, const char* text)`
**支持命令**:
| 命令格式 | 行为 |
|---|---|
| `SMS:号码:内容` | 调用 `sendSMS()` 代发短信，结果写入系统日志 |
| `RESET` | `resetModule()` → `ESP.restart()` |

---

### `void processSmsContent(const char* sender, const char* text, const char* timestamp)`
**处理顺序**:
1. `isAdmin()` → 管理员号码跳过黑名单
2. 非管理员执行 `isInNumberBlackList()` → 命中则记录“黑名单拦截”，`reason` 为“号码黑名单”，然后结束
3. 非管理员执行 `isInContentBlackList()` → 命中则记录“黑名单拦截”，`reason` 为“内容关键词”，然后结束
4. 管理员命令检测 → 记录“管理员命令”（`reason` 为“管理员命令”）→ `processAdminCommand()` → 不再发普通通知
5. `sendSMSToServer()` — 推送所有启用的通道，返回 `PushSummary`
6. `addForwardHistory()` — 记录发送者、时间戳、内容、状态、原因及本次实际尝试的通道结果；状态按未配置推送、无可用网络、全部失败、部分成功、全部成功依次判定

---

### `void checkSerial1URC()`
逐行读取 Serial1 并交给 `handleSmsUrcLine()`：
- `+CMTI: "ME",<index>`：入索引队列，由 `processPendingSms()` 使用 `AT+CMGR` 读取；
- `+CMT:`：进入 WAIT_PDU，下一行 PDU hex 缓存后解析；
- 其他行：输出到调试日志。

`pollStoredSms()` 每 60 秒发送 `AT+CMGL=4`，先收集索引，再按索引降序 `CMGR/CMGD` 补收，避免删除导致索引重排时误删未读短信。

---

## 模块: web_handlers.cpp — HTTP 处理

### HTTP 路由一览

| 方法 | 路径 | 处理函数 | 说明 |
|---|---|---|---|
| GET | `/` `/tools` `/sms` | `handleRoot()` | SPA 主页，含短信记录与推送测试入口 |
| POST | `/save` | `handleSave()` | 保存配置并重新校验 |
| GET | `/history` | `handleHistory()` | 返回短信记录 JSON |
| POST | `/history` | `handleHistory()` | `action=delete&id=...` 删除记录 |
| POST | `/testpush` | `handleTestPush()` | 用当前表单参数测试单个推送通道 |
| POST | `/pushnet` | `handlePushNetwork()` | 切换 WiFi/4G/WiFi优先推送网络 |
| POST | `/sendsms` | `handleSendSms()` | 网页发送短信 |
| POST | `/ping` | `handlePing()` | Ping 测试 |
| GET | `/query` | `handleQuery()` | 模组/WiFi 信息查询 |
| GET | `/flight` | `handleFlightMode()` | 飞行模式控制 |
| GET | `/at` | `handleATCommand()` | AT 指令调试 |
| GET | `/log` | `handleLog()` | 系统日志 JSON 数组 |
| GET | `/modem` | `handleModem()` | 模组控制 |
| GET/POST | `/wifi` | `handleWifi()` | WiFi 状态/扫描/保存/连接/重启（`?action=...`） |
| GET/POST | `/ap` | `handleAp()` | 热点状态/保存/应用（`?action=status|save`） |
| GET | `/system` | `handleSystem()` | 整机重启 |

### 日志系统 API

### `void logCapture(const String& msg)`
### `void logCapture(const char* msg)`
**用途**: 输出日志，但不换行。内容追加到行缓冲区 `_logLine`，不会立即写入环形缓冲区。同时输出到 `Serial`。

**使用场景**: 与 `logCaptureLn()` 配合，构建一行完整日志：
```cpp
logCapture("目标号码: ");
logCaptureLn(String(phoneNumber));
// 环形缓冲区中只有一行: "目标号码: 13800138000"
```

---

### `void logCaptureLn(const String& msg)`
### `void logCaptureLn(const char* msg)`
**用途**: 输出日志并换行。将 `_logLine` + msg 提交到环形缓冲区，然后清空行缓冲区。同时输出到 `Serial.println`。

**注意**: 环形缓冲区每调用一次 `logCaptureLn` 即可产生一行日志，因此 `logCapture()` 和 `logCaptureLn()` 的日志会在环形缓冲区中合并为同一行。

---

### `void logCaptureF(const char* fmt, ...)`
**用途**: 格式化日志输出（类似 `printf`）。支持 `%d`/`%s`/`%f` 等格式。如果格式化字符串以 `\n` 结尾，自动提交整行到缓冲区。

---

### `void handleLog()`
**用途**: HTTP 端点 `GET /log`，返回环形缓冲区中所有日志行。

**返回格式**: `application/json`
```json
["行1", "行2", "行3", ...]
```
**鉴权**: 需要 HTTP Basic Auth。最多返回 120 行（环形缓冲区容量）。

---

### `bool checkAuth()`
HTTP Basic Authentication，账号密码来自 `config.webUser` / `config.webPass`。

---

### `void handleRoot()`
返回 SPA 主页 HTML。通过 `html.replace("%KEY%", value)` 替换模板占位符，动态生成 5 个推送通道的表单。页面包含 11 个可切换面板（系统设置/修改密码/短信记录/推送通道/管理员 & 黑名单/发送短信/模组诊断/网络测试/模组控制/AT终端/系统日志），通过 JS 切换显示。侧边栏不再包含“账号管理”导航项；底部“修改密码”按钮调用 `switchPanel('account')` 打开 `panel-account`，该面板标题为“修改密码”。

**模板占位符**:

| 占位符 | 数据来源 |
|---|---|
| `%IP%` | `WiFi.localIP().toString()` |
| `%WIFI_SSID%` | 已连接时为当前 SSID，否则为已保存 SSID（NVS `wifi_config` / `WIFI_SSID` 兜底） |
| `%WIFI_STATUS_TEXT%` | `已连接` / `未连接` |
| `%WIFI_RSSI_TEXT%` | 已连接时为 `RSSI dBm`，否则为 `--` |
| `%WIFI_SAVED_SSID%` | `loadWifiSetting("ssid", WIFI_SSID)`，用于 WiFi 下拉框初始值 |
| `%WIFI_AUTH_MODE%` | `loadWifiAuth()`（0~4），用于初始化加密方式下拉框 |
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

---

### `void handleToolsPage()`
兼容旧链接，直接委托给 `handleRoot()` 返回同一 SPA 页面。

---

### `void handleSave()`
解析 POST 表单 → 写入 `config` → `saveConfig()` → 重新校验 `configValid` → 返回成功页面（3 秒跳转）。

**注意**: `/save` 不处理 WiFi 字段；WiFi 连接信息只通过 `/wifi?action=save|connect` 写入独立的 `wifi_config` namespace。

---

### `void handleHistory()`
**路由**: `GET /history`、`POST /history`

- `GET`：返回 `application/json`，内容为 `forwardHistoryToJson()` 生成的数组（按新→旧排列）；每项包含 `id`、`sender`、`timestamp`、`message`、`status`、`statusText`、`reason`、`channels`（`name` + `success`）、`truncated`；
- `POST`：读取 `action` 与 `id` 参数；`action=delete` 时调用 `deleteForwardHistory(id)`，返回 `{"success":bool,"message":"..."}`。

**鉴权**: 需要 HTTP Basic Auth。响应带 `Cache-Control: no-store`。

---

### `void handleTestPush()`
**路由**: `POST /testpush`

用当前表单参数测试单个推送通道，不保存配置。

**参数**: `index`（0~4）、`type`、`name`、`url`、`key1`、`key2`、`body`。

**行为**:
1. 校验通道索引与 `isPushChannelValid()`；
2. 用 `sender="测试"`、`message="这是一条来自短信转发器的测试推送。"` 调用 `sendToChannel(..., testMode=true)`；
3. 测试推送遵守 `config.pushNetworkMode`，4G 模式可能激活数据上下文并在结束后清理；
4. 返回 `{"success":bool,"message":"..."}`；测试模式只尝试一次。

**鉴权**: 需要 HTTP Basic Auth。

---

### `void handlePushNetwork()`
**路由**: `POST /pushnet`

**参数**: `mode`（0=仅 WiFi，1=仅 4G 数据，2=WiFi 优先+4G 兜底）。

**行为**: 校验模式范围 → 写入 `config.pushNetworkMode` → `saveConfig()` 持久化到 NVS 的 `pushNetMode` → 返回：

```json
{"success":true,"mode":2,"message":"已切换为 WiFi 优先，传输失败时使用 4G 数据"}
```

缺少参数或模式非法时返回 HTTP 400 与 `success:false`。该接口只切换推送路由，不改变 WiFi 连接、APN 或短信接收配置。

**鉴权**: 需要 HTTP Basic Auth。

---

### `void handleWifi()`
**路由**: `GET/POST /wifi?action=...`

注册方式为 `server.on("/wifi", handleWifi)`，未限制 HTTP 方法；Web UI 对 `scan` / `restart` 使用 GET，对 `save` / `connect` 使用 POST，`status` 按 GET 语义调用。所有 action 都需要 HTTP Basic Auth。

**请求示例**:

- `GET /wifi?action=status`
- `GET /wifi?action=scan`
- `POST /wifi?action=save`（`ssid=...&pass=...&auth=3`）
- `POST /wifi?action=connect`（`ssid=...&pass=...&auth=3`）
- `GET /wifi?action=restart`

WiFi 卡片通过 `application/x-www-form-urlencoded` 的 POST body 提交 `ssid` / `pass` / `auth`；`server.arg()` 也接受同名 query 参数。

**并发保护**: 静态 `busy` 标志覆盖整个处理过程。上一请求未结束时返回 HTTP 429：

```json
{"success":false,"message":"WiFi 正忙，请稍后重试"}
```

**action 一览**:

| action | 参数 | 行为 | 成功返回 |
|---|---|---|---|
| `status` | 无 | 返回连接状态、当前/已保存 SSID、IP、RSSI、已保存 `auth` 和显示文本；密码不回显 | `{"success":true,"connected":bool,"ssid":"...","ip":"...","rssi":-55,"savedSsid":"...","savedAuth":3,"authText":"WPA2"}` |
| `scan` | 无 | 同步调用 `WiFi.scanNetworks(false, true)` 扫描附近网络；同名 SSID 合并为信号最强的一条，按 RSSI 降序，最多 32 个；扫描前已连接、扫描后断开时调用 `WiFi.reconnect()` | `{"success":true,"networks":[{"ssid":"...","rssi":-55,"auth":3,"authText":"WPA2","channel":6}]}` |
| `save` | `ssid`、`pass`、`auth` | 校验后调用 `saveWifiConnection()` 写入 NVS `wifi_config`；不立即切换连接 | `{"success":true,"message":"WiFi 连接信息已保存，重启后生效"}` |
| `connect` | `ssid`、`pass`、`auth` | 先保存 NVS，再返回响应；随后 `WiFi.disconnect(true)` 并用新凭据连接，最多等待 15 秒 | `{"success":true,"message":"WiFi 连接信息已保存，正在连接；若 IP 变化，请使用新 IP 打开页面"}` |
| `restart` | 无 | 从 NVS 读取已保存的 `ssid`/`pass`/`auth`（无 NVS 时回退编译期宏），重新连接；不读取表单、不写 NVS（实际最多等待 15 秒，返回文案约 5 秒） | `{"success":true,"message":"WiFi 正在重新连接，请等待约 5 秒后刷新页面"}` |
| 其他 | 无 | 未知操作 | `{"success":false,"message":"未知操作"}` |

**注意**: `scan.networks[].auth` 是 ESP32 `wifi_auth_mode_t` 原始值（例如 0=OPEN、3=WPA2、6=WPA3、7=WPA2/WPA3），与保存参数的 0~4 不是同一套编码；WiFi 卡片通过 JS 将扫描值映射到下拉框选项。

`scan` 扫描失败（`WiFi.scanNetworks()` 返回负值）时返回 `{"success":false,"message":"WiFi 扫描失败"}`；扫描成功但没有可见网络时返回 `{"success":true,"networks":[]}`。

**`save` / `connect` 参数校验**:

1. `ssid` 会先 `trim()`；为空时返回 `{"success":false,"message":"请选择 WiFi 网络"}`；
2. `auth` 由 `toInt()` 读取，大于 4 时按 0 处理；
3. `auth == 1` 时强制清空 `pass`；
4. 非开放网络且 `pass` 为空、`ssid` 与已保存 SSID 相同时，沿用 NVS 中的旧密码；
5. 非开放网络且仍无密码时返回 `{"success":false,"message":"请输入 WiFi 密码"}`。

**返回与状态码**:

- `status` / `scan` 设置 `Cache-Control: no-store`，返回 `application/json`；
- `save` / `connect` / `restart` 均返回 `application/json`；
- 除 `busy` 的 HTTP 429 外，参数错误和未知 action 也返回 HTTP 200，仅以 `success:false` 表示失败；
- `connect` / `restart` 的响应在切换连接前发出，`success:true` 只表示请求已接受，不保证新网络已经连上；`connect` 15 秒内未连上时只写日志并继续后台尝试，凭据已经写入 NVS。

**风险说明**:

- **扫描短暂影响 WiFi**：扫描是同步阻塞操作，其他 HTTP 请求会等待；已连接时可能短暂断开，扫描结束后固件尝试重连；
- **切换 IP**：`connect` / `restart` 后 DHCP 可能分配新地址，旧页面或旧 URL 会失效；错误凭据会导致设备无法通过 WiFi 打开管理页，但短信接收和 4G 推送仍可工作；
- **与 HTTPS 无关**：WiFi 的 `auth` 只影响 ESP32 STA 的最低安全等级，不改变 Web 管理页面的 HTTP:80，也不改变 4G MHTTP 的 HTTPS/证书策略（当前 4G HTTPS 仍为 `auth=0`，TLS 加密但不校验证书链）。

---

### `void handleQuery()`
根据 `?type=` 参数执行不同查询：

| type | AT 指令 | 返回内容 |
|---|---|---|
| `ati` | `ATI` | 制造商/型号/固件版本 |
| `signal` | `AT+CESQ` | RSRP/RSRQ 信号强度 |
| `siminfo` | `AT+CIMI` `AT+ICCID` `AT+CNUM` | IMSI/ICCID/本机号码 |
| `network` | `AT+CEREG?` `AT+COPS?` `AT+CGACT?` `AT+CGDCONT?` | 注册/运营商/数据/APN |
| `wifi` | (WiFi 对象) | SSID/RSSI/IP/网关/DNS/MAC/BSSID/信道 |

---

### `void handleFlightMode()`
根据 `?action=` 控制飞行模式：

| action | AT 指令 | 说明 |
|---|---|---|
| `query` | `AT+CFUN?` | 查询当前 CFUN 值 |
| `toggle` | 查询 + `AT+CFUN=1/4` | 1↔4 切换 |
| `on` | `AT+CFUN=4` | 强制开启 |
| `off` | `AT+CFUN=1` | 强制关闭 |

---

### `void handleATCommand()`
通过 `?cmd=` 透传 AT 指令到 `sendATCommand()`，返回 JSON `{success, message}`。

---

### `void handleSendSms()`
解析 POST 表单 phone/content → 调用 `sendSMS()` → 返回 HTML 结果页（3 秒跳转）。

---

### `void handlePing()`
**流程**:
1. `AT+CGACT=1,1` 激活数据连接
2. `AT+MPING="8.8.8.8",30,1` ping 一次（30 秒超时）
3. 解析 `+MPING:` URC 响应，提取 IP/延迟/TTL
4. `AT+CGACT=0,1` 关闭数据连接
5. 返回 JSON `{success, message}`

**注意**: 整个操作最长约 35 秒。

---

### `void handleAp()`
**路由**: `GET/POST /ap?action=...`

**action 一览**:

| action | 参数 | 行为 | 成功返回 |
|---|---|---|---|
| `status` | 无 | 返回 `enabled`、`running`、SSID、默认 SSID、AP IP、客户端数、`passwordChanged`、固定加密 `WPA2-AES`；不回显密码 | `{"success":true,"enabled":true,"running":true,"ssid":"6680短信转发","ip":"192.168.4.1","clients":0,"passwordChanged":false,"encryption":"WPA2-AES"}` |
| `save` | `enabled`（`on`/空）、`ssid`、`pass` | 校验后写入 NVS `ap_config`；先响应，再调用 `startAccessPoint()` 或 `stopAccessPoint()` 应用 | `{"success":true,"message":"热点配置已保存","enabled":true,"passwordChanged":false}` |

**默认值与校验**:

- 默认启用，SSID 为 MAC 末 4 位 + `短信转发`，密码 `12345678`，固定 WPA2-AES；
- SSID 为空时使用默认 SSID，最长 32 字节；
- 密码为空时沿用当前密码；长度必须为 8-63 位；
- `passChanged` 在密码不等于默认密码时为 true；首次登录通过该标志触发修改密码提示。

**存储**: NVS namespace `ap_config`，键 `enabled` / `ssid` / `pass` / `passChanged` / `promptShown`，与 `sms_config`、`wifi_config` 分离。`promptShown` 保证首次登录提示只弹一次；`passChanged=false` 时页面持续显示默认密码警告。

---

### `void handleSystem()`
系统控制命令，路由 `GET /system`。

| action | 行为 |
|---|---|
| `restart` | 先返回 JSON 成功响应 → `delay(500)` 确保发出 → `ESP.restart()` 整机重启 |
| 其他 | 返回 `{"success":false,"message":"未知操作"}` |

**注意**: 重启后设备需重新走完整启动流程（WiFi + 模组初始化），约 1 分钟恢复。
