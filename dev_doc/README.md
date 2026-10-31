# 短信转发器 (SMS Forwarding) — 项目总览

## 项目简介

基于 **ESP32-C3 (MakerGO SuperMini)** 的短信转发设备。通过 4G/LTE 模组接收短信，可经由 WiFi 或 4G 数据将短信内容推送到多种即时通讯平台（微信/钉钉/飞书/Telegram 等），并在本地保存最近 10 条短信记录。提供 Web 管理界面进行配置和诊断。

## 硬件平台

| 组件 | 型号 / 规格 |
|---|---|
| MCU | ESP32-C3 (MakerGO SuperMini) |
| 4G 模组 | 通过 UART 连接，AT 指令控制 |
| 模组 EN 引脚 | GPIO 5 |
| 模组 TXD/RXD | GPIO 3 / GPIO 4 |
| LED | GPIO 8（内置 LED） |
| 串口波特率 | USB: 115200, 模组: 115200 |

## 第三方依赖库

| 库 | 版本 | 用途 |
|---|---|---|
| pdulib | 0.5.11 | PDU 格式短信编解码 |
| Arduino ESP32 Core | 3.3.10 | WiFi / HTTP / mbedTLS / NTP |

## 核心功能

1. **短信接收** — 4G 模组 PDU 模式自动上报，解析后提取发送者/时间/内容
2. **长短信合并** — 支持多段长短信（最多 10 段）自动拼合，30 秒超时保护
3. **多通道推送** — 同时支持最多 5 个推送通道，可独立配置推送到不同平台
4. **WiFi 设置** — 系统设置“WiFi 连接”卡片支持扫描（`GET /wifi?action=scan`）、仅保存（`POST /wifi?action=save`）和保存并连接（`POST /wifi?action=connect`）；SSID、加密方式、密码写入 NVS `wifi_config`，重启 WiFi 使用已保存凭据（`GET /wifi?action=restart`）；启动时锁定 2.4G 物理 BSSID/信道，适配双频合一 SSID
5. **设备热点** — 默认启用 MAC 末 4 位 + `短信转发` 的 WPA2-AES 热点，支持开关、SSID 和密码配置；首次登录提示修改默认密码
6. **选网配置** — 默认自动选网，也可填写 5-6 位 PLMN 手动选择 LTE 运营商；保存后重启应用
7. **推送网络切换** — 系统设置页支持 WiFi、4G 数据、WiFi 优先（失败自动使用 4G）三种模式；短信转发与测试推送共用当前路由
8. **短信记录** — 记录所有收到的短信（包括黑名单拦截、管理员命令、未配置推送、无可用网络），最多保留最近 10 条，SPIFFS 持久化到 `/forward_history.bin`；Web 端可查看每条记录实际尝试的推送通道及成功/失败，并逐条删除
9. **推送测试** — 每个推送通道可用当前表单参数发送测试消息，单次尝试、不保存配置，并遵守当前推送网络
10. **Web 管理** — 单页应用 (SPA)，侧边栏导航 + HTTP Basic Auth 保护；`panel-account` 仅由底部“修改密码”按钮打开，面板标题为“修改密码”
11. **系统日志** — Web 端实时查看设备串口日志，自动刷新，最多保留 120 行
12. **管理员命令** — 通过短信远程执行 SMS:发送 和 RESET 命令（仅限管理员号码）
13. **黑名单** — 支持号码精确/前缀/后缀和内容关键词过滤，多个关键词按“或”逻辑匹配

## 支持推送平台

| 类型 | 枚举值 | 说明 |
|---|---|---|
| POST JSON | `PUSH_TYPE_POST_JSON` (1) | 通用 POST JSON，字段: sender / message / timestamp |
| Bark | `PUSH_TYPE_BARK` (2) | iOS 推送，字段: title / body |
| GET 请求 | `PUSH_TYPE_GET` (3) | 参数放在 URL query string |
| 钉钉机器人 | `PUSH_TYPE_DINGTALK` (4) | 支持加签安全验证 |
| PushPlus | `PUSH_TYPE_PUSHPLUS` (5) | 微信公众号推送，支持多渠道 |
| Server酱 | `PUSH_TYPE_SERVERCHAN` (6) | 微信推送 |
| 自定义模板 | `PUSH_TYPE_CUSTOM` (7) | 用户自定义 HTTP 请求体 |
| 飞书机器人 | `PUSH_TYPE_FEISHU` (8) | 支持签名验证 |
| Gotify | `PUSH_TYPE_GOTIFY` (9) | 自建推送服务 |
| Telegram | `PUSH_TYPE_TELEGRAM` (10) | Bot API |
| 企业微信机器人 | `PUSH_TYPE_WECOM` (11) | 企微群机器人 text 消息 |

## 工程结构

```
code/
├── code.ino              # 入口: setup() + loop()
├── config_types.h        # 枚举/结构体/常量定义（无依赖）
├── globals.h / .cpp      # 全局变量 + 引脚定义 + 公共头文件
├── config.h / .cpp       # NVS 配置存取、配置校验
├── modem.h / .cpp        # 模组 AT 指令、电源控制、短信发送
├── modem_http.h / .cpp   # ML307A MHTTP/HTTPS 数据通道
├── push.h / .cpp         # 多通道推送、加密工具函数
├── history.h / .cpp      # 短信记录（所有收到的短信，最多 10 条，SPIFFS 持久化）
├── sms_process.h / .cpp  # 短信解析、长短信合并、黑名单、管理员命令
├── web_handlers.h / .cpp # HTTP 请求处理器 + 日志环形缓冲区
├── web_html.h / .cpp     # SPA HTML 页面模板（单页 11 个面板，含短信记录与 WiFi 设置）
├── wifi_config.h / .cpp  # WiFi 运行时配置（NVS wifi_config、编译期宏兜底、安全等级）
└── ap_config.h / .cpp    # 设备热点配置（NVS ap_config、AP+STA、WPA2-AES）
```

## 编译与烧录

```powershell
# 设置环境
$env:Path = "C:\Program Files\Arduino IDE\resources\app\lib\backend\resources;$env:Path"
$env:ARDUINO_DIRECTORIES_DATA = "D:\dev\arduino_pack"
$env:ARDUINO_DIRECTORIES_USER = "D:\dev\arduino_pack\user"

# 编译
arduino-cli compile --fqbn esp32:esp32:makergo_c3_supermini --build-path "D:\dev\arduino_pack\build" "D:\dev\sms_forwarding\code"

# 烧录
arduino-cli upload --fqbn esp32:esp32:makergo_c3_supermini --port COM4 --input-dir "D:\dev\arduino_pack\build" "D:\dev\sms_forwarding\code"

# 串口监视
arduino-cli monitor --port COM4 --config 115200
```

## 启动流程

1. 初始化 GPIO（LED、模组 EN 引脚）
2. 启动 USB Serial (115200) 和模组 Serial1 (115200, GPIO 3/4)
3. 模组断电重启（EN 引脚控制）
4. 初始化长短信缓存、加载 NVS 配置、初始化短信记录（挂载 SPIFFS 并读取记录文件）、校验配置有效性
5. 从 NVS `wifi_config` 读取 `ssid/pass/auth`（空值回退到 `WIFI_SSID`/`WIFI_PASS`），应用最低安全等级后连接 WiFi；20 秒超时后继续启动，不因 WiFi 不可用重启
6. 从 NVS `ap_config` 读取热点配置；默认启用 AP+STA，启动 `MAC末4位 + 短信转发` 的 WPA2-AES 热点
7. 启动 HTTP 服务器（端口 80），注册所有路由
8. 同步 NTP 时间
9. 初始化 4G 模组：AT 握手 → 配置短信 URC 上报 → 设置 PDU 模式 → 等待网络注册；4G 数据上下文在推送时按需激活

## 主循环

```
loop()
  ├── server.handleClient()     // HTTP 请求处理
  ├── 配置无效时每秒打印 IP
  ├── checkConcatTimeout()      // 长短信超时检查
  ├── Serial → Serial1 透传    // USB → 模组 AT 透传
  ├── checkSerial1URC()        // 模组 URC 解析（+CMTI/+CMT）
  ├── processPendingSms()      // 每次处理一条待读短信
  └── pollStoredSms()          // 每60秒 CMGL 补收遗漏短信
```

## 关键设计决策

- **PDU 模式** 而非 Text 模式：中文短信必须使用 PDU 编码
- **WiFi 运行时配置**：`wifi_config.h/.cpp` 负责 NVS `wifi_config` 的 `ssid`/`pass`/`auth` 读写和安全等级；`WIFI_SSID`/`WIFI_PASS` 只在 NVS 值为空时作为编译期兜底，`auth` 设置的是最低安全等级而非 AP 的精确加密模式
- **设备热点**：`ap_config.h/.cpp` 负责 NVS `ap_config` 的 `enabled`/`ssid`/`pass`/`passChanged`/`promptShown`；默认启用 AP+STA，WPA2-AES，默认 SSID 为 MAC 末 4 位 + `短信转发`，默认密码 `12345678`
- **推送网络路由**：`PushNetworkMode` 支持仅 WiFi、仅 4G、WiFi 优先；WiFi 优先只在连接建立/DNS/TLS 等传输层失败时切换 4G，HTTP 4xx、业务错误和读取超时不重发，避免重复通知
- **4G 数据按需管理**：启动时不再主动执行 `AT+CGACT=0,1`；4G 推送通过 ML307A MHTTP/HTTPS 完成，结束时尝试删除 MHTTP 实例并关闭 cid=1
- **HTTPS 证书限制**：当前 4G HTTPS 使用模组 `auth=0`（TLS 加密但不校验证书链），需使用可信推送地址；严格校验需额外写入 CA 证书
- **双 NVS 命名空间**：`sms_config` 保存账号、黑名单、推送网络和推送通道；`wifi_config` 独立保存 WiFi 的 `ssid`/`pass`/`auth`，断电不丢失
- **WiFi 切换风险**：扫描同步执行并可能短暂影响当前连接；保存并连接或重启 WiFi 后 DHCP 可能分配新 IP；WiFi 加密方式不影响 Web HTTP 与 4G HTTPS 证书策略
- **短信记录持久化**：最近 10 条收到的短信以自定义二进制格式（version 2）存入 SPIFFS `/forward_history.bin`，带版本号和 CRC32 校验，文件损坏时自动重置为空记录
- **单页应用 (SPA)**：Web UI 采用侧边栏 + 面板切换，所有功能在一个 HTML 页面中，通过 JS 切换可见面板；侧边栏不含“账号管理”，底部“修改密码”按钮打开 `panel-account`
- **日志行缓冲**：`logCapture()` 将输出追加到行缓冲区，仅 `logCaptureLn()` 提交整行到环形缓冲区，避免非换行输出被误拆成多行
- **日志环形缓冲区**：120 行容量，循环覆盖，通过 `/log` 端点以 JSON 返回，Web 端每 2 秒自动刷新
- **模块化设计**：每个 .h/.cpp 对承担单一职责，便于增减功能和问题定位
- **HTML 模板分离**：HTML 字符串常量单独存于 `web_html.cpp`，便于 UI 修改而不影响逻辑代码
