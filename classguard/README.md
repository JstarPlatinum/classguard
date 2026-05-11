# ClassGuard 项目目录说明

本目录是 ClassGuard 的 ESP32-S3 固件、硬件资料、测试工程、Web 数据看板原型与项目管理资料集合。当前 README 按现有文件结构整理，只列出已有内容的目录；空目录、构建产物目录和缓存目录不纳入说明，例如 `build/`、`.venv/`、`__pycache__/`。

## 顶层文件

| 文件 | 用途 |
| --- | --- |
| `CMakeLists.txt` | ESP-IDF 顶层工程入口，声明项目名称并包含 `main/` 与组件构建。 |
| `partitions.csv` | 当前固件使用的分区表。 |
| `sdkconfig` | 当前 ESP-IDF 工程配置。 |
| `sdkconfig.old` | 旧版 ESP-IDF 工程配置备份。 |
| `.gitignore` | Git 忽略规则。 |
| `README.md` | 本项目目录说明文档。 |

## 顶层目录

| 目录 | 当前内容 |
| --- | --- |
| `.devcontainer/` | VS Code Dev Container 配置，包括 `devcontainer.json` 和 `Dockerfile`。 |
| `.vscode/` | VS Code / ESP-IDF 开发配置，包括 IntelliSense、调试和工作区设置。 |
| `main/` | 当前主固件应用入口与业务服务代码。 |
| `components/` | ESP-IDF 可复用组件，包含 BSP、公共定义、外设驱动和占有率检测服务。 |
| `docs/` | 设计文档、数据格式说明、问题记录和算法说明。 |
| `feature/` | 功能原型，目前包含 Web 数据看板服务。 |
| `hardware/` | 硬件资料，目前包含板卡图片。 |
| `project_management/` | 发布冻结说明、测试版本发布记录和校验文件。 |
| `test/` | 传感器数据测试代码与 Wi-Fi 固定数据上传测试工程。 |
| `tools/` | PC 端辅助工具，目前包含 MLX90640 热成像查看器。 |

## `main/` 主固件应用

`main/` 是当前 ESP-IDF 主工程入口，包含系统入口、数据结构、传感器服务、热成像服务、空气质量评估和遥测上传逻辑。

| 文件 | 用途 |
| --- | --- |
| `main.c` | 固件主入口，负责系统初始化、任务启动和主流程组织。 |
| `CMakeLists.txt` | `main/` 目录的 ESP-IDF 构建配置。 |
| `app_data.c` / `app_data.h` | 应用层共享数据结构与数据访问接口。 |
| `sensor_service.c` / `sensor_service.h` | 环境与颗粒物等传感器服务封装。 |
| `thermal_service.c` / `thermal_service.h` | MLX90640 热成像采集与处理服务。 |
| `air_quality_evaluator.c` / `air_quality_evaluator.h` | 空气质量综合评估逻辑。 |
| `telemetry_upload.c` / `telemetry_upload.h` | 设备遥测数据上传逻辑。 |
| `wifi_upload_config.h` | Wi-Fi 与上传相关配置。 |

## `components/` ESP-IDF 组件

`components/` 中的代码按板级支持、公共定义、外设驱动和服务逻辑拆分，供主工程复用。

| 目录 | 当前内容 |
| --- | --- |
| `components/bsp/` | ClassGuard 板级支持包，包含 `classguard_bsp.c`、组件构建文件和公开头文件。 |
| `components/bsp/include/` | BSP 对外接口 `classguard_bsp.h`。 |
| `components/common/` | 公共组件构建配置。 |
| `components/common/include/` | 全局配置、数据类型和引脚定义：`app_config.h`、`data_types.h`、`pin_map.h`。 |
| `components/drivers/` | 外设驱动集合及驱动层 `CMakeLists.txt`。 |
| `components/drivers/beam_sensor/` | 两路对射传感器驱动。 |
| `components/drivers/hub75_display/` | HUB75 RGB 点阵屏显示驱动。 |
| `components/drivers/i2c_bus/` | I2C 总线封装。 |
| `components/drivers/mlx90640/` | MLX90640 热成像阵列驱动，包含 Melexis API、I2C 适配层、封装接口和授权说明。 |
| `components/drivers/pms5003/` | PMS5003 颗粒物传感器驱动。 |
| `components/drivers/scd41/` | SCD41 CO2 传感器驱动。 |
| `components/drivers/sht35/` | SHT35 温湿度传感器驱动。 |
| `components/services/` | 服务层组件集合及构建配置。 |
| `components/services/occupancy_detector/` | 基于热成像等数据的占有率检测服务。 |

## `docs/` 文档

| 目录 | 当前内容 |
| --- | --- |
| `docs/markdown/` | 项目设计方案、Wi-Fi Web 开发流程、MLX90640 占有率算法说明和空气质量评估方案。 |
| `docs/datatype/` | 正式传感器遥测数据格式说明。 |
| `docs/issues/` | 已记录的问题与排查材料。 |

主要文档包括：

- `esp32s3_multi_module_driver_design_v2_actual_board.md`：ESP32-S3 多模块驱动设计方案。
- `ESP32S3_WiFi_Web项目开发流程.md`：Wi-Fi 与 Web 功能开发流程。
- `mlx90640_occupancy_algorithm_v2.md`、`mlx90640_occupancy_algorithm_flow.md`：MLX90640 占有率算法文档。
- `空气质量综合评估报告与代码使用流程方案_v1.1_占有率修正版.md`：空气质量评估与代码使用流程方案。
- `formal_sensor_telemetry_v0_1.md`：传感器遥测数据格式定义。

## `feature/` 功能原型

当前 `feature/` 中包含 Web 数据看板原型：

| 目录或文件 | 用途 |
| --- | --- |
| `feature/web_dashboard/server/app.py` | Web 服务入口。 |
| `feature/web_dashboard/server/config.py` | 服务配置。 |
| `feature/web_dashboard/server/database.py` | 数据库访问逻辑。 |
| `feature/web_dashboard/server/models.py` | 数据模型。 |
| `feature/web_dashboard/server/mock_sender.py` | 本地模拟数据发送脚本。 |
| `feature/web_dashboard/server/requirements.txt` | Python 依赖。 |
| `feature/web_dashboard/server/README.md` | Web 看板服务说明。 |
| `feature/web_dashboard/server/static/` | 前端静态页面、样式与脚本：`index.html`、`style.css`、`dashboard.js`。 |
| `feature/web_dashboard/server/data/telemetry.db` | Web 看板本地遥测数据库。 |

## `test/` 测试工程

| 目录 | 当前内容 |
| --- | --- |
| `test/sensor_v1_data_test/` | 传感器 V1 数据测试代码，包含 MLX90640 与环境 I2C 测试源文件和头文件。 |
| `test/wifi_upload_fixed_data_test/` | 独立 ESP-IDF 测试工程，用于验证固定数据 Wi-Fi 上传流程。 |
| `test/wifi_upload_fixed_data_test/main/` | 测试工程入口、构建配置和 Wi-Fi 上传配置头文件。 |

`test/wifi_upload_fixed_data_test/` 下包含独立的 `CMakeLists.txt` 和 `README.md`，可按该目录说明单独构建和测试。

## `tools/` 辅助工具

| 目录 | 当前内容 |
| --- | --- |
| `tools/mlx90640_viewer/` | PC 端 MLX90640 热成像查看工具，包含 `mlx90640_viewer.py` 和 `requirements.txt`。 |

## `hardware/` 硬件资料

| 目录 | 当前内容 |
| --- | --- |
| `hardware/board/` | 板卡相关资料，目前包含 `boardimage.png`。 |

## `project_management/` 项目管理资料

| 目录 | 当前内容 |
| --- | --- |
| `project_management/releases/` | 发布与冻结记录，包含 `mlx90640_v1_freeze.md`。 |
| `project_management/releases/test_v1/` | 测试版本发布说明和 SHA256 校验文件。 |

## 当前代码组织建议

1. 新增硬件底层能力时优先放入 `components/drivers/` 或 `components/bsp/`，并通过 `include/` 暴露稳定接口。
2. 跨传感器的数据整合、占有率判断、空气质量评估等逻辑优先放在服务层或 `main/` 中已有服务文件，避免直接堆进 `main.c`。
3. 独立验证流程放入 `test/`，PC 端调试和查看工具放入 `tools/`。
4. 尚未并入主固件、但已经可运行的演示功能放入 `feature/`，并保留本目录内的说明文档。
5. 设计方案、数据格式、问题记录和发布记录分别沉淀到 `docs/` 与 `project_management/`，方便后续追踪。
