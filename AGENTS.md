# AGENTS.md

低成本短信转发器：ESP32-C3 + ML307R/ML307A 4G 模组（AT 固件），可通过 WiFi 或 4G 数据将收到的短信推送到多种平台，并在本地保存短信记录（所有收到的短信，含拦截与未转发情况）。Arduino 框架，代码与文档均为中文。

## 构建与烧录

使用 `arduino-cli`（路径随机器不同，需自行确认），板型 FQBN: `esp32:esp32:makergo_c3_supermini`，串口按真实情况来。

```powershell
$env:Path = "C:\Program Files\Arduino IDE\resources\app\lib\backend\resources;$env:Path"
$env:ARDUINO_DIRECTORIES_DATA = "D:\dev\arduino_pack"
$env:ARDUINO_DIRECTORIES_USER = "D:\dev\arduino_pack\user"

# 编译（首次约 3-5 分钟，不要提前中断）
arduino-cli compile --fqbn esp32:esp32:makergo_c3_supermini --build-path "D:\dev\arduino_pack\build" "<项目路径>\code"

# 烧录
arduino-cli upload --fqbn esp32:esp32:makergo_c3_supermini --port COM4 --input-dir "D:\dev\arduino_pack\build" "<项目路径>\code"

# 串口日志（115200）
arduino-cli monitor --port COM4 --config 115200
```

注意：
- 所有 arduino-cli 相关目录**不能含中文路径**，否则编译失败。
- 依赖库：pdulib 0.5.11、ESP32 Core 3.3.10。
- `.github/agent/编译烧录实测.md` 是已验证的可靠流程，优先参考。
- 本机实际项目路径为 `C:\Users\chenx\Desktop\git-repos\sms_forwarding`（非 ASCII 路径仅限源码位置；build 目录必须放在纯英文路径）。

## 工程结构

```
code/
├── code.ino              # 入口: setup() + loop()
├── config_types.h        # 枚举/结构体/常量（最底层，无依赖）
├── globals.h/.cpp        # 全局变量 + 引脚定义 + 公共头文件
├── config.h/.cpp         # NVS 配置存取、有效性校验
├── modem.h/.cpp          # AT 指令、电源控制、短信发送
├── modem_http.h/.cpp     # ML307A MHTTP/HTTPS 数据通道
├── push.h/.cpp           # 多通道推送、加密/编码工具
├── history.h/.cpp        # 短信记录（所有收到的短信，最多 10 条，SPIFFS 持久化）
├── sms_process.h/.cpp    # URC 解析、长短信合并、黑名单、管理员命令
├── web_handlers.h/.cpp   # HTTP 处理器 + 日志环形缓冲区
├── web_html.h/.cpp       # SPA HTML 模板（模板占位符 %KEY% 替换）
├── task_types.h          # 任务/结果结构体定义
├── wifi_config.h/.cpp    # WiFi 运行时配置（NVS wifi_config；编译期宏兜底 + 安全等级）
└── ap_config.h/.cpp      # 设备热点配置（NVS ap_config；AP+STA、WPA2-AES）
dev_doc/                  # 架构/API/模块详细文档（改代码前先读）
.github/agent/            # 编译烧录流程文档
```

依赖方向：config_types.h → globals.h → 各模块 → code.ino。每个 .h/.cpp 对单一职责。

## 关键约定

- **日志输出必须用 `logCapture()` / `logCaptureLn()` / `logCaptureF()`**（web_handlers.cpp），不要直接用 Serial.print——这些函数同时写入串口和 Web 端可见的 120 行环形缓冲区。`logCapture` 不换行，`logCaptureLn` 提交整行。
- **短信记录语义**：`history.h/.cpp` 记录所有收到的短信（包括黑名单拦截、管理员命令、未配置推送、无可用网络；旧记录可能保留 `WiFi 未连接` 状态 6），每条保存状态、原因及实际尝试的推送通道结果；不要只记录进入普通转发流程的短信。
- **配置持久化分两个 NVS namespace**：`sms_config` 保存 `Config` 全局结构体（账号、黑名单、推送网络、推送通道）；`wifi_config` 只保存 WiFi 连接的 `ssid` / `pass` / `auth`，由 `wifi_config.cpp` 直接读写，不进入 `Config`/`handleSave()`。新增 WiFi 字段时同步修改 `wifi_config.h/.cpp`、`handleWifi()` 和 WiFi 卡片。
- **热点配置**：`ap_config.h/.cpp` 独立读写 NVS `ap_config`（`enabled`/`ssid`/`pass`/`passChanged`/`promptShown`）。默认启用，SSID 为 MAC 末 4 位 + `短信转发`，密码 `12345678`，WPA2-AES；首次登录由 `apPasswordChanged=false` 触发一次修改密码提示（`promptShown` 记录已提示），修改密码后置 `passChanged=true`。热点使用 AP+STA，不能关闭 STA 的网页入口逻辑。
- **推送网络三模式**：`PushNetworkMode` 的 `PUSH_NET_WIFI`、`PUSH_NET_CELLULAR`、`PUSH_NET_WIFI_FALLBACK` 分别对应仅 WiFi、仅 4G、WiFi 优先；`sendSMSToServer()` 与 `/testpush` 必须共用同一套路由逻辑，4G 请求走 `modem_http.cpp` 的 MHTTP 实现。
- **WiFi 设置与路由**：系统设置“WiFi 连接”卡片；扫描 `GET /wifi?action=scan`，仅保存 `POST /wifi?action=save`，保存并连接 `POST /wifi?action=connect`，重连 `GET /wifi?action=restart`（只用 NVS 已保存凭据）。`auth`：0 自动、1 开放、2 WPA-WPA2、3 WPA2、4 WPA3；`applyWifiSecurity()` 设置的是最低安全等级，不是 AP 的精确加密模式，`0` 的默认门槛是 WPA2。
- **选网配置**：`Config.operatorMode`（0 自动/1 手动）与 `operatorCode`（5 或 6 位 PLMN）存入 `sms_config`；`configureOperator()` 在模组初始化时执行 `AT+COPS=0` 或 `AT+COPS=1,2,"PLMN",7`。保存选网配置后 `handleSave()` 会重启设备应用。
- **2.4G WiFi 锁定**：启动时用 `WiFi.scanNetworks()` 查找与已保存 SSID 匹配的最佳物理 BSSID/信道，再用 `WiFi.begin(ssid, pass, channel, bssid)` 连接，解决双频合一 SSID 落到 5G 虚拟 AP 的问题。
- **WiFi 配置的已知风险**：扫描是同步阻塞且可能让当前 WiFi 短暂断开/重连；`connect`/`restart` 后 DHCP 可能分配新 IP，旧地址会失效；`connect` 失败时凭据仍已写入 NVS，错误凭据可能导致下次重启后无法通过 WiFi 访问网页。WiFi 加密方式与 Web HTTP、4G MHTTP HTTPS 证书策略无关。
- **新增推送通道**：在 PushType 枚举加类型 → push.cpp 的 sendToChannel() 加 case → config.cpp 的 isPushChannelValid() 加校验 → web_html.cpp 加 UI 选项。
- 短信用 **PDU 模式**（中文短信必需），解析用 pdulib。
- 主循环是单线程 Arduino 模型，常规 HTTP 与 URC 处理为非阻塞；WiFi 扫描/连接会同步阻塞，模组初始化等慢操作已有异步化结构（见 task_types.h），避免在 loop 路径中再引入长阻塞。
- 启动流程不再主动执行 `AT+CGACT=0,1`，避免网络注册前关闭数据上下文产生“设置 CGACT 失败”噪声；4G 推送按需激活，结束尝试清理 MHTTP 实例并关闭 cid=1。
- WiFi 启动时从 NVS `wifi_config` 读取 `ssid/pass/auth`（空值回退 `WIFI_SSID`/`WIFI_PASS`），调用 `applyWifiSecurity()` 后连接；20 秒超时不能重启设备，继续启动，保证仅 4G 推送和短信接收可用。
- 短信接收使用 `AT+CNMI=2,1`（ME 存储 + `+CMTI`），并保留 `AT+CMGL=4` 定时补收；AT 命令等待期间不得清空 Serial1 或吞掉短信 URC。
- Web UI 是单页应用（web_html.cpp 中字符串常量 + `%KEY%` 占位符），修改界面只动 web_html.cpp。侧边栏不再有“账号管理”入口，底部“修改密码”按钮调用 `switchPanel('account')` 打开 `panel-account`，面板标题为“修改密码”。

## 其他说明

- 默认 Web 账号 admin/admin123；管理员短信命令支持 `SMS:号码:内容` 和 `RESET`。
- 项目明确不做：多卡控制、通话、拨号、开放接口——勿实现此类需求。
