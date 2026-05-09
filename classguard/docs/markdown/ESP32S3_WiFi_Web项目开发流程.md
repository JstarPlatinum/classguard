# ESP32-S3 WiFi 数据上传与 Web 可视化系统开发流程

## 1. 项目定位与总体架构

本项目的目标是让 ESP32-S3 作为边缘采集节点，读取 SCD41、PMS5003、MLX90640 等传感器数据，并通过 WiFi 将数据上传到 PC 本地服务器。PC 端负责接收、存储和转发数据，浏览器端通过自建 Web 页面显示实时参数、设备状态、历史曲线和报警信息。

在通信关系上，ESP32-S3 不是传统意义上的“从设备”，而是一个主动联网的数据采集客户端；PC 是服务器端，负责监听指定端口并接收 ESP32 主动上传的数据。

```text
传感器模块
    ↓
ESP32-S3 采集、预处理、封装 JSON
    ↓ WiFi / HTTP POST
PC 本地服务器 FastAPI
    ↓
SQLite 数据库存储 + SSE 实时推送
    ↓
Web 页面实时显示数据、设备状态和历史曲线
```

第一版原型采用以下技术栈：

| 层级 | 推荐技术/库 | 作用 |
|---|---|---|
| ESP32 固件 | Espressif IDE + ESP-IDF | 作为主开发环境，适合工程化开发 ESP32-S3 固件 |
| WiFi 连接 | `esp_wifi`、`esp_netif`、`esp_event`、`nvs_flash` | 完成 WiFi STA 连接、网络事件处理和配置保存 |
| HTTP 上传 | `esp_http_client` | ESP32 向 PC 服务器发送 HTTP POST 请求 |
| JSON 封装 | `cJSON` | 将传感器数据封装为标准 JSON |
| 传感器驱动 | `driver/i2c` 或新版 `driver/i2c_master`、`driver/uart` | 驱动 SCD41、MLX90640、PMS5003 等模块 |
| 任务调度 | FreeRTOS | 分离 WiFi、采集、上传和状态监控任务 |
| PC 后端 | Python `FastAPI`、`uvicorn`、`pydantic`、`aiosqlite` | 接收数据、校验数据、存储数据、提供 API |
| 前端页面 | HTML + CSS + JavaScript | 构建本地 Web 看板 |
| 图表显示 | Apache ECharts | 显示 CO₂、PM2.5、温湿度、红外温度等历史曲线 |
| 实时通信 | SSE `EventSource` | 后端将最新数据推送到浏览器，无需手动刷新 |
| 数据存储 | SQLite | 本地测试轻量、易迁移、便于导出数据 |

---

## 2. 网页开发流程

网页端建议先于 ESP32 固件开发完成。这样可以先用 PC 脚本模拟 ESP32 上传假数据，把服务器接收、数据库保存和 Web 显示流程跑通，再接入真实硬件。开发过程会更稳定，也更容易定位问题。

### 2.1 后端服务器开发

后端建议使用 Python FastAPI。项目目录可以这样组织：

```text
server/
├── app.py                  # FastAPI 主程序
├── config.py               # 服务器配置，如 token、数据库路径、端口说明
├── database.py             # SQLite 初始化、写入、查询
├── models.py               # Pydantic 数据结构定义
├── requirements.txt        # Python 依赖库
├── data/
│   └── telemetry.db        # SQLite 数据库文件
└── static/
    ├── index.html          # Web 首页
    ├── dashboard.js        # 前端逻辑
    └── style.css           # 页面样式
```

建议安装的 Python 库如下：

```bash
pip install fastapi uvicorn[standard] pydantic aiosqlite python-dotenv
```

各库作用如下：

| 库 | 作用 |
|---|---|
| `fastapi` | 构建 HTTP API 与 SSE 接口 |
| `uvicorn[standard]` | 运行 FastAPI 服务器 |
| `pydantic` | 校验 ESP32 上传的 JSON 数据结构 |
| `aiosqlite` | 异步读写 SQLite 数据库 |
| `python-dotenv` | 将设备 token、数据库路径等配置放到 `.env` 文件中管理 |

后端至少应实现以下接口：

| 接口 | 方法 | 功能 |
|---|---|---|
| `/api/telemetry` | POST | 接收 ESP32 上传的传感器数据 |
| `/api/latest` | GET | 返回最新一条数据，用于页面初始化 |
| `/api/history` | GET | 返回历史数据，用于绘制曲线 |
| `/events` | GET | SSE 实时推送最新数据到浏览器 |
| `/` | GET | 返回 Web 看板页面 |

服务器启动时必须监听 `0.0.0.0`，否则 ESP32 不能从局域网访问 PC：

```bash
uvicorn app:app --host 0.0.0.0 --port 8000
```

不要使用：

```bash
uvicorn app:app --host 127.0.0.1 --port 8000
```

`127.0.0.1` 只允许 PC 自己访问，ESP32 和手机等局域网设备无法访问。

### 2.2 数据接收格式设计

ESP32 上传数据时建议使用 JSON。数据结构要稳定，不要直接上传用逗号拼接的字符串。推荐第一版使用如下格式：

```json
{
  "device_id": "esp32s3_node_001",
  "firmware": "v0.1.0",
  "timestamp": 0,
  "uptime_ms": 123456,
  "wifi": {
    "rssi": -48,
    "ip": "192.168.0.120"
  },
  "sensors": {
    "scd41": {
      "co2_ppm": 612,
      "temperature_c": 25.4,
      "humidity_percent": 54.2
    },
    "pms5003": {
      "pm1_0": 8,
      "pm2_5": 15,
      "pm10": 22
    },
    "mlx90640": {
      "frame_rate": 16,
      "temp_min_c": 23.1,
      "temp_max_c": 31.8,
      "temp_avg_c": 26.4
    }
  },
  "status": {
    "sensor_ok": true,
    "error_code": 0,
    "error_message": ""
  }
}
```

其中 `timestamp` 可以先由服务器写入接收时间，不强制 ESP32 自己提供真实时间。ESP32 的本地时间若未配置 NTP，可能并不准确；服务器接收时间更适合作为第一版历史曲线的时间轴。

### 2.3 设备认证设计

实际使用时同一个 WiFi 下可能存在很多设备，因此服务器不应接收所有来源的数据。第一版可以使用一个简单的设备 token。ESP32 请求时在 Header 中携带：

```http
X-Device-Token: classguard_test_token_001
```

后端收到 `/api/telemetry` 请求后检查：

```text
Header 中的 X-Device-Token 是否正确
JSON 中的 device_id 是否在允许列表内
数据字段是否符合规定格式
```

这套机制不能替代正式产品中的 HTTPS 和证书认证，但对于本地原型验证已经足够。后期如果需要远程部署，可以升级为 HTTPS、HMAC 签名或 MQTT 用户名密码机制。

### 2.4 数据库存储设计

SQLite 第一版可以使用一张主表保存原始 JSON 和常用字段。这样既保留了完整数据，又方便查询曲线。

推荐表结构：

```sql
CREATE TABLE IF NOT EXISTS telemetry (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    received_at TEXT NOT NULL,
    device_id TEXT NOT NULL,
    firmware TEXT,
    uptime_ms INTEGER,
    wifi_rssi INTEGER,
    wifi_ip TEXT,
    co2_ppm REAL,
    temperature_c REAL,
    humidity_percent REAL,
    pm1_0 REAL,
    pm2_5 REAL,
    pm10 REAL,
    mlx_temp_min_c REAL,
    mlx_temp_max_c REAL,
    mlx_temp_avg_c REAL,
    sensor_ok INTEGER,
    error_code INTEGER,
    raw_json TEXT NOT NULL
);
```

这种设计适合初期调试。常用字段被单独展开，Web 曲线查询会比较方便；完整 JSON 仍然保存在 `raw_json` 中，后期新增字段时不容易丢失信息。

### 2.5 Web 页面开发

Web 页面可以先采用原生 HTML、CSS、JavaScript，不必一开始使用 Vue 或 React。原因是本项目重点在硬件采集、通信链路和实时显示，原生页面足够完成演示，也更容易部署到 FastAPI 的 `static/` 目录中。

页面建议分为四个区域：

| 区域 | 内容 |
|---|---|
| 实时参数卡片 | CO₂、温度、湿度、PM1.0、PM2.5、PM10、红外最高温、红外平均温 |
| 设备状态栏 | 设备编号、固件版本、WiFi RSSI、设备 IP、在线状态、最近上传时间 |
| 历史曲线区 | CO₂ 曲线、PM2.5 曲线、温湿度曲线、红外温度曲线 |
| 报警与日志区 | CO₂ 超限、PM2.5 超限、传感器异常、设备离线等提示 |

前端推荐使用的库和浏览器 API：

| 工具 | 作用 |
|---|---|
| `fetch()` | 页面加载时请求 `/api/latest` 和 `/api/history` |
| `EventSource` | 连接 `/events`，接收服务器实时推送 |
| ECharts | 绘制实时曲线和历史曲线 |
| CSS Grid/Flex | 实现响应式卡片布局 |

前端逻辑建议如下：

```text
页面打开
    ↓
fetch('/api/latest') 获取最新数据
    ↓
fetch('/api/history') 获取历史数据并初始化 ECharts
    ↓
EventSource('/events') 建立实时连接
    ↓
收到新数据后更新参数卡片、设备状态和曲线
```

### 2.6 网页端初步验证

在 ESP32 尚未接入之前，建议写一个 PC 模拟上传脚本 `mock_sender.py`，每 2 秒向服务器发送一组假数据。这样可以验证：

```text
FastAPI 是否能接收 POST 数据
SQLite 是否能正常写入
Web 页面是否能显示最新参数
ECharts 曲线是否能更新
SSE 是否能实时推送
```

模拟上传脚本可以使用 Python `requests` 库：

```bash
pip install requests
```

---

## 3. ESP32 开发流程（Espressif IDE / ESP-IDF）

ESP32 端继续使用项目默认的开发方式，继续使用C开发。
新增的内容包括
| `app_main.c` | 程序入口，初始化 NVS、WiFi、传感器、任务和队列 |
| `app_config.h` | 保存 WiFi SSID、服务器地址、上传周期、设备 ID 等配置 |
| `wifi_manager` | 连接 WiFi、断线重连、获取 IP 和 RSSI |
| `sensor_manager` | 统一调度各传感器读取，生成传感器数据结构 |
| `http_uploader` | 将 JSON 数据通过 HTTP POST 上传到 PC 服务器 |
| `data_packet` | 使用 `cJSON` 生成统一 JSON 数据包 |
| `device_status` | 记录运行时间、错误码、传感器状态和上传结果 |

### 3.1 ESP-IDF 需要使用的组件/库

ESP-IDF 中建议使用以下组件：

| ESP-IDF 组件 | 作用 |
|---|---|
| `nvs_flash` | 初始化非易失性存储，WiFi 驱动依赖它 |
| `esp_netif` | 初始化 TCP/IP 网络接口 |
| `esp_event` | 处理 WiFi 连接、断开、获取 IP 等事件 |
| `esp_wifi` | WiFi STA 模式连接实验室 WiFi 或手机热点 |
| `esp_http_client` | HTTP POST 上传 JSON 数据 |
| `cJSON` | 构建和解析 JSON 数据 |
| `freertos` | 创建采集、上传、状态监控等任务 |
| `driver/i2c` 或 `driver/i2c_master` | 读取 SCD41、MLX90640 等 I2C 传感器 |
| `driver/uart` | 读取 PMS5003 串口数据 |
| `esp_timer` | 获取系统运行时间，控制周期采样 |
| `esp_log` | 输出调试日志 |

### 3.2 WiFi 配置方式

第一版可以直接在 `app_config.h` 中写入 WiFi 名称、密码和服务器地址：

```c
#define WIFI_SSID       "你的WiFi名称"
#define WIFI_PASSWORD   "你的WiFi密码"

#define DEVICE_ID       "esp32s3_node_001"
#define DEVICE_TOKEN    "classguard_test_token_001"

#define SERVER_URL      "http://192.168.0.67:8000/api/telemetry"
#define UPLOAD_PERIOD_MS 2000
```

其中 `192.168.0.67` 应替换为服务器(此处为PC) WLAN IPv4 地址。不要使用 `127.0.0.1`。

WiFi 连接模式采用 STA：

```text
ESP32-S3 连接实验室 WiFi / 手机热点 / 小路由器
PC 连接同一个 WiFi
ESP32 主动访问 PC 的局域网 IP 和端口
```

如果实验室 WiFi 有网页认证、客户端隔离或防火墙限制，建议先使用手机热点或小路由器完成原型验证。

### 3.3 FreeRTOS 任务划分

ESP32 端不建议把所有逻辑放在一个 `while (1)` 中。推荐采用任务和队列：

```text
wifi_task / WiFi 事件回调
    维护 WiFi 状态，断线后自动重连

sensor_task
    周期性读取 SCD41、PMS5003、MLX90640
    生成 SensorData 数据结构
    送入上传队列

upload_task
    从队列取出 SensorData
    调用 data_packet 生成 JSON
    使用 esp_http_client 上传到服务器
    根据 HTTP 状态码记录上传成功或失败

status_task
    周期性打印设备状态
    统计传感器异常次数、上传失败次数、WiFi RSSI
```

推荐第一版上传周期：

| 数据类型 | 建议周期 |
|---|---|
| SCD41 CO₂、温湿度 | 2 s ~ 5 s |
| PMS5003 PM 数据 | 2 s ~ 5 s |
| MLX90640 温度统计值 | 2 s ~ 5 s |
| MLX90640 完整 32×24 热图 | 后期单独设计，建议低频上传或按需上传 |

### 3.4 传感器接入顺序

为了降低调试复杂度，不建议一开始同时接入全部传感器。推荐顺序：

```text
WiFi 连接成功
    ↓
HTTP POST 上传假数据
    ↓
SCD41 接入并上传 CO₂、温度、湿度
    ↓
PMS5003 接入并上传 PM1.0、PM2.5、PM10
    ↓
MLX90640 接入并上传最高温、最低温、平均温
    ↓
再考虑完整热图或多设备扩展
```

这一顺序的依据是：SCD41 的数据字段较少，最适合作为第一颗传感器验证 I2C 与上传链路；PMS5003 需要处理 UART 帧校验，调试量略高；MLX90640 数据量较大，且对 I2C 速率、内存和计算都有更高要求，适合作为增强功能接入。

### 3.5 HTTP 上传逻辑

ESP32 上传时建议携带 Header：

```http
Content-Type: application/json
X-Device-Token: classguard_test_token_001
```

上传逻辑：

```text
检查 WiFi 是否已连接
    ↓
读取最新 SensorData
    ↓
使用 cJSON 生成 JSON 字符串
    ↓
esp_http_client 设置 URL、POST 方法、Header 和 Body
    ↓
执行 HTTP 请求
    ↓
检查 HTTP 状态码是否为 200
    ↓
释放 JSON 字符串和 HTTP client 资源
```

常见错误处理：

| 问题 | 处理方式 |
|---|---|
| WiFi 未连接 | 暂停上传，等待 WiFi 事件恢复 |
| HTTP 连接失败 | 记录错误码，下一周期重试 |
| 服务器返回 401 | 检查 `X-Device-Token` 是否正确 |
| 服务器返回 422 | 检查 JSON 字段是否与 Pydantic 模型一致 |
| 服务器无响应 | 检查 PC IP、防火墙、服务器是否监听 `0.0.0.0` |

---

## 4. 联合调试流程

联合调试阶段的核心原则是逐层验证，不要同时修改多个环节。每一层确认正常后再进入下一层。

### 4.1 网络连通性检查

PC 当前需要使用 WLAN IPv4 地址。windows可以通过bash代码 `ipconfig` 获取：

```text
无线局域网适配器 WLAN:
IPv4 地址: 192.168.0.67
默认网关: 192.168.0.1
```

则 ESP32 的服务器 URL 应为：

```text
http://192.168.0.67:8000/api/telemetry
```

不要使用：

```text
127.0.0.1        # 本机回环地址
```

验证顺序：

```text
PC 启动 FastAPI，监听 0.0.0.0:8000
    ↓
PC 浏览器打开 http://127.0.0.1:8000
    ↓
手机连接同一 WiFi，打开 http://192.168.0.67:8000
    ↓
如果手机可以打开，说明局域网访问基本正常
    ↓
ESP32 再尝试访问 http://192.168.0.67:8000/api/telemetry
```

如果手机都打不开 PC 页面，ESP32 通常也打不开。此时优先检查 Windows 防火墙、服务器监听地址和实验室 WiFi 是否开启客户端隔离。

### 4.2 PC 后端单独调试

使用 `mock_sender.py` 或 Postman 测试 `/api/telemetry`：

```text
发送合法 token + 合法 JSON
    ↓
检查服务器是否返回 200
    ↓
检查 SQLite 是否新增记录
    ↓
打开网页检查参数卡片是否更新
    ↓
检查历史曲线是否更新
```

此阶段不涉及 ESP32，目的是确认 Web 系统自身可用。

### 4.3 ESP32 上传假数据

ESP32 端先不接传感器，只上传固定假数据：

```text
co2_ppm = 600
temperature_c = 25.0
humidity_percent = 50.0
pm2_5 = 12
```

观察点：

```text
ESP32 串口日志是否显示 WiFi 已连接
ESP32 是否获取到 192.168.0.xxx 地址
HTTP POST 是否返回 200
PC 后端是否打印收到数据
Web 页面是否实时变化
```

如果 ESP32 返回连接失败，优先检查：

```text
SERVER_URL 是否写对
PC IP 是否变化
PC 服务器是否运行
Windows 防火墙是否拦截
ESP32 和 PC 是否在同一 WiFi
实验室 WiFi 是否禁止设备互访
```

### 4.4 接入真实传感器

按照 SCD41、PMS5003、MLX90640 的顺序逐个接入。每接入一个传感器，都应完成以下检查：

```text
串口日志能否打印该传感器原始数据
JSON 中是否出现该传感器字段
服务器是否成功保存该字段
网页是否正确显示该字段
历史曲线是否能显示该字段
异常情况下网页是否能显示错误状态
```

不要在一个阶段同时修改传感器驱动、JSON 格式、服务器模型和前端页面。更稳妥的做法是每次只增加一个字段，确认端到端链路正常后再增加下一个字段。

### 4.5 设备在线/离线判断

服务器端应记录每个设备最后一次上传时间：

```text
last_seen = 最近一次收到该 device_id 数据的时间
```

网页端显示状态：

| 条件 | 状态 |
|---|---|
| 当前时间 - last_seen ≤ 10 秒 | 在线 |
| 当前时间 - last_seen > 10 秒 | 离线 |
| 收到数据但 `sensor_ok = false` | 传感器异常 |
| token 错误或 device_id 未注册 | 拒绝接收 |

这样可以避免“网页还停留在旧数据，但用户误以为设备仍在实时运行”的问题。

### 4.6 推荐调试记录表

联合调试时建议记录以下内容，后期写项目文档或比赛材料时也能直接使用：

| 时间 | 测试内容 | 结果 | 问题 | 处理方式 |
|---|---|---|---|---|
| 2026-xx-xx | PC 后端接收模拟数据 | 成功 | 无 | 保留截图 |
| 2026-xx-xx | ESP32 上传假数据 | 成功 | 防火墙首次拦截 | 放行 Python |
| 2026-xx-xx | SCD41 上传 | 成功 | 无 | 记录 CO₂ 曲线 |
| 2026-xx-xx | PMS5003 上传 | 待测 | UART 帧校验 | 检查波特率和数据帧 |
| 2026-xx-xx | MLX90640 统计值上传 | 待测 | 数据量较大 | 先上传 min/max/avg |

---

## 5. 第一版最小可行原型目标

第一版不要追求功能过多，建议以“稳定闭环”为核心：

```text
ESP32-S3 成功连接 WiFi
    ↓
ESP32-S3 每 2 秒上传一次 JSON 数据
    ↓
PC FastAPI 服务器成功接收并写入 SQLite
    ↓
Web 页面实时显示 CO₂、温湿度、PM2.5、PM10、设备在线状态
    ↓
ECharts 绘制最近一段时间的历史曲线
```

MLX90640 可以先只上传：

```text
temp_min_c
temp_max_c
temp_avg_c
```

完整 32×24 红外热图属于第二阶段功能。它的数据量、刷新率和前端显示方式都更复杂，适合在基础通信链路稳定后再加入。

---

## 6. 后续扩展方向

当本地 PC 服务器验证稳定后，可以继续扩展：

| 扩展方向 | 实现方式 |
|---|---|
| 多 ESP32 节点 | 为每个节点分配不同 `device_id` 和 token |
| 云端访问 | 将 FastAPI 部署到云服务器或内网穿透平台 |
| MQTT 通信 | 使用 EMQX / Mosquitto 替代 HTTP POST，适合多设备订阅 |
| 数据导出 | 后端提供 `/api/export.csv` 接口 |
| 报警策略 | CO₂、PM2.5、温度异常阈值可在网页上配置 |
| 设备配置下发 | 后端提供 `/api/config`，ESP32 周期性拉取上传周期、阈值等参数 |
| OTA 升级 | 后期加入 ESP-IDF OTA，实现固件远程更新 |
| 完整热图显示 | 上传 MLX90640 32×24 矩阵，前端用 ECharts heatmap 显示 |

---

## 7. 推荐开发顺序总结

```text
网页与后端开发
    ↓
FastAPI 接收模拟数据
    ↓
SQLite 保存数据
    ↓
Web 页面显示参数与曲线
    ↓
ESP32 使用 ESP-IDF 连接 WiFi
    ↓
ESP32 上传假 JSON 到 PC
    ↓
接入 SCD41
    ↓
接入 PMS5003
    ↓
接入 MLX90640 统计值
    ↓
联合调试在线/离线状态、报警、历史曲线
    ↓
整理演示视频、测试记录和项目说明文档
```

重点不是一开始把所有模块同时跑起来，而是让“采集—上传—接收—存储—显示”形成稳定闭环。只要闭环稳定，后续增加传感器、曲线、报警策略或云端部署都属于可控扩展。
