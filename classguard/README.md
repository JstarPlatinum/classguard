# ClassGuard 项目目录说明

本目录是 ClassGuard 的 ESP32-S3 实际板卡固件与项目管理目录。当前工程已有 ESP-IDF 根工程文件 `CMakeLists.txt` 和入口目录 `main/`，后续代码按“板级支持 -> 外设驱动 -> 服务封装 -> 业务应用”的方式拆分，避免把所有逻辑堆在 `main.c` 中。

本结构依据 `../esp32s3_multi_module_driver_design_v2_actual_board.md` 中的 V2 实际板卡方案整理，目标硬件为 ESP32-S3-WROOM-1 最小系统板，外设包括 HUB75 RGB LED 点阵屏、MLX90640 热成像阵列、SCD41、SHT35、PMS5003 和两路 NPN 红光对射模块。

## 顶层目录

| 目录 | 用途 | 应存放内容 |
| --- | --- | --- |
| `.devcontainer/` | 开发容器配置 | ESP-IDF 容器、Dockerfile、容器启动配置，保持现有文件即可 |
| `.vscode/` | VS Code 工程配置 | ESP-IDF 插件配置、调试配置、C/C++ IntelliSense 配置 |
| `main/` | ESP-IDF 固件入口 | `app_main`、系统初始化顺序、任务创建、顶层模块启动，不放大量驱动细节 |
| `components/` | ESP-IDF 组件代码 | BSP、外设驱动、服务层、业务层、公共工具代码 |
| `config/` | 工程配置 | `sdkconfig.defaults`、功能开关、采样周期、阈值、屏幕参数、传感器偏移配置 |
| `partitions/` | 分区表 | NVS、OTA、SPIFFS/LittleFS、模型或资源分区表 |
| `feature/` | 功能原型与临时验证 | 尚未沉淀进正式组件的功能实验、模块验证草稿、比赛演示特性原型 |
| `docs/` | 设计与调试文档 | 需求、架构、接线、调试步骤、标定说明、接口协议 |
| `hardware/` | 硬件资料 | 原理图、PCB、BOM、引脚表、数据手册、硬件测试记录 |
| `tools/` | 辅助工具 | 烧录脚本、串口调试、日志导出、数据解析、PC 端验证工具 |
| `test/` | 测试代码 | 单元测试、集成测试、硬件在环测试和测试数据 |
| `project_management/` | 项目管理资料 | 任务拆分、风险清单、版本计划、发布记录 |
| `data/` | 样本和实验数据 | 传感器样本、标定数据、串口日志、现场测试记录 |
| `assets/` | 资源文件 | 模型文件、网页静态资源、图标、嵌入式文件系统资源 |

## `components/` 固件组件分层

| 目录 | 用途 | 应存放代码 |
| --- | --- | --- |
| `components/bsp/` | 板级支持包 | V2 实际板卡 GPIO 映射、I2C/UART/HUB75 总线初始化、电源控制、复位脚、安全默认电平 |
| `components/drivers/` | 外设驱动层 | 只负责硬件读写、协议解析和最小状态维护，不写业务判断 |
| `components/services/` | 服务层 | 把多个驱动组合成稳定服务，例如传感器数据管理、显示模型、事件逻辑、故障监控 |
| `components/app/` | 业务应用层 | ClassGuard 的业务流程、显示页面切换、报警策略、比赛演示逻辑 |
| `components/common/` | 公共代码 | `pin_map.h`、`app_config.h`、`data_types.h`、错误码、队列事件类型、环形缓冲、通用滤波算法 |

## `components/drivers/` 外设驱动目录

| 目录 | 对应模块 | 应存放代码 |
| --- | --- | --- |
| `components/drivers/i2c_bus/` | 双 I2C 总线 | I2C0/I2C1 初始化、I2C scanner、总线锁、超时恢复、设备探测 |
| `components/drivers/mlx90640/` | MLX90640 热成像阵列 | GPIO39/GPIO40 上的 I2C0 驱动、帧读取、校准参数加载、温度矩阵生成 |
| `components/drivers/scd41/` | SCD41 CO2 传感器 | GPIO41/GPIO42 上的 I2C1 驱动、周期测量、CO2/温湿度读取、CRC 校验 |
| `components/drivers/sht35/` | SHT35 温湿度传感器 | 单次测量、温湿度读取、CRC 校验、温湿度偏移配置入口 |
| `components/drivers/pms5003/` | PMS5003 颗粒物传感器 | UART1 GPIO21/GPIO47 收发、SET GPIO14、RESET GPIO48、帧状态机和校验和 |
| `components/drivers/hub75_display/` | HUB75 RGB 点阵屏 | GPIO4/5/6/7/15/16/17/18/8/9/10/11/12/13 的 DMA 显示驱动封装 |
| `components/drivers/beam_sensor/` | 两路 NPN 红光对射 | GPIO1/GPIO2 输入中断、边沿时间戳、去抖、低有效/高有效适配 |

## `components/services/` 服务目录

| 目录 | 用途 | 应存放代码 |
| --- | --- | --- |
| `components/services/sensor_manager/` | 传感器统一管理 | 汇总 ThermalFrame、EnvironmentFrame、PMFrame，维护最新快照和有效标志 |
| `components/services/display_model/` | 显示数据模型 | 把热成像、CO2、温湿度、PM 数据和对射事件转换成 HUB75 可显示内容 |
| `components/services/event_logic/` | 事件与业务判断 | 对射触发状态机、速度/通过事件计算、报警触发条件、显示模式切换 |
| `components/services/fault_monitor/` | 故障监控 | I2C NACK、PMS 连续错帧、传感器超时、任务心跳、看门狗和降级策略 |

## `main/`

`main/` 只作为系统入口，建议保留以下职责：

1. 初始化日志、NVS、看门狗和基础 GPIO 安全电平。
2. 初始化 `components/bsp/` 中定义的板级资源。
3. 按方案文档建议顺序启动 PMS5003、两条 I2C、MLX90640、SCD41、SHT35、对射输入和 HUB75。
4. 创建 FreeRTOS 任务，例如 `TaskDisplay`、`TaskThermal`、`TaskEnvironment`、`TaskPMS5003`、`TaskBeamEvent`、`TaskFaultMonitor`。
5. 只调用组件对外接口，不在 `main.c` 中直接解析传感器协议或刷新屏幕细节。

## `docs/`

| 目录 | 用途 | 应存放内容 |
| --- | --- | --- |
| `docs/requirements/` | 需求与约束 | 功能需求、性能目标、功耗目标、比赛演示需求、异常场景 |
| `docs/architecture/` | 软件架构 | 任务划分、队列关系、数据流、状态机、组件依赖图 |
| `docs/hardware/` | 硬件说明 | V2 引脚分配表、接线图、电源树、HUB75 排线说明、NPN 隔离接法 |
| `docs/bringup/` | 分阶段调试 | GPIO 测试、I2C scanner、PMS5003 串口验证、HUB75 彩条测试、系统联调记录 |
| `docs/calibration/` | 标定说明 | MLX90640 温度校准、SHT35 温湿度偏移、对射距离、PM 数据稳定时间和阈值 |

## `hardware/`

| 目录 | 用途 | 应存放内容 |
| --- | --- | --- |
| `hardware/board/` | 板卡资料 | 实际开发板排针图、原理图、PCB、BOM、焊接记录、GPIO 占用表 |
| `hardware/datasheets/` | 器件手册 | ESP32-S3、MLX90640、SCD41、SHT35、PMS5003、HUB75 屏、光耦、电平转换芯片资料 |
| `hardware/test_records/` | 硬件测试记录 | 上电测试、I2C 波形、电源跌落、HUB75 花屏排查、NPN 误触发记录 |

## `tools/`

| 目录 | 用途 | 应存放内容 |
| --- | --- | --- |
| `tools/scripts/` | 自动化脚本 | 构建、烧录、清理、串口日志保存、分区表生成、测试数据转换脚本 |
| `tools/pc_debug/` | PC 端调试 | 串口上位机、PMS5003 帧解析器、热成像数据查看器、HUB75 显示素材生成工具 |

## `test/`

| 目录 | 用途 | 应存放内容 |
| --- | --- | --- |
| `test/unit/` | 单元测试 | PMS5003 帧解析、CRC 校验、对射状态机、移动平均和阈值判断测试 |
| `test/integration/` | 集成测试 | 双 I2C 同时运行、传感器管理服务、显示模型、任务间队列测试 |
| `test/hil/` | 硬件在环测试 | 需要实际 ESP32-S3 板卡参与的串口自动化测试、外设连通性测试、长时间稳定性测试 |

## `project_management/`

| 目录 | 用途 | 应存放内容 |
| --- | --- | --- |
| `project_management/tasks/` | 任务管理 | 模块开发清单、负责人、优先级、完成标准、阻塞项 |
| `project_management/risks/` | 风险管理 | 启动脚占用、HUB75 电源、I2C 稳定性、PMS 启动漂移、NPN 误触发等风险和对策 |
| `project_management/releases/` | 版本管理 | 固件版本计划、发布说明、测试通过记录、演示版本冻结记录 |

## `data/` 与 `assets/`

| 目录 | 用途 | 应存放内容 |
| --- | --- | --- |
| `data/samples/` | 样本数据 | 热成像帧、环境传感器样本、PMS5003 串口帧、对射触发日志 |
| `data/calibration/` | 标定数据 | 温度偏移、湿度偏移、CO2 校准记录、对射距离和速度计算参数 |
| `data/logs/` | 运行日志 | 长时间运行日志、异常复位日志、现场演示记录 |
| `assets/models/` | 模型资源 | 后续如需视觉/热成像判断模型，可放量化模型和标签文件 |
| `assets/web/` | Web 资源 | 后续如做设备配置页，可放 HTML、CSS、JS、图标等静态文件 |

## 推荐开发顺序

1. 在 `components/common/` 中先建立 `pin_map.h`、`app_config.h`、`data_types.h`。
2. 在 `components/bsp/` 中固化 V2 实际板卡的 GPIO、I2C、UART 和 HUB75 初始化。
3. 先调通 `drivers/i2c_bus/`，再分别验证 MLX90640、SCD41、SHT35。
4. 实现 `drivers/pms5003/` 的串口帧状态机和校验逻辑。
5. 实现 `drivers/beam_sensor/` 的中断记录和去抖状态机。
6. 单独调通 `drivers/hub75_display/`，先显示纯色和彩条，再接入业务数据。
7. 在 `services/` 中组合最新数据快照、显示模型、事件逻辑和故障监控。
8. 最后在 `main/` 中按初始化顺序拉起所有任务，并把测试记录沉淀到 `docs/bringup/`。
