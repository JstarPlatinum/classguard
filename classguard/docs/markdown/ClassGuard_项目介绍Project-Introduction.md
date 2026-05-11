# ClassGuard 教室空气守门员项目详细介绍 / Detailed Introduction to the ClassGuard Classroom Air Guardian Project

## 一、项目概述 / I. Project Overview

我们团队设计并实现的 ClassGuard“教室空气守门员”，是一个面向教室、自习室、实验室、培训空间等人员密集室内场景的低成本、隐私友好型环境风险提示系统。  
ClassGuard, the “Classroom Air Guardian” designed and implemented by our team, is a low-cost and privacy-friendly environmental risk notification system for densely occupied indoor spaces such as classrooms, study rooms, laboratories, and training rooms.

它不是一个只显示温度或 PM2.5 的普通空气盒子，而是希望把“教室空气是否需要管理”这件原本看不见、说不清的问题，转化为可以采集、可以研判、可以展示、可以行动的环境管理闭环。  
It is not a conventional air-quality box that only displays temperature or PM2.5; instead, it aims to transform the previously invisible and ambiguous question of “whether classroom air needs management” into an environmental management loop that can be measured, assessed, displayed, and acted upon.

项目的核心思路是：通过 ESP32-S3 采集 CO2、温湿度、颗粒物和热成像占用率等多源数据，在本地完成综合评分与风险判断，再通过 LED 提示端和 Web 看板把结果展示给老师、学生和管理者。  
The core idea of the project is to use the ESP32-S3 to collect multi-source data, including CO₂, temperature and humidity, particulate matter, and thermal-imaging-based occupancy rate, perform integrated scoring and risk assessment locally, and then present the results to teachers, students, and managers through an LED notification terminal and a Web dashboard.

当环境状态变差时，系统不是简单报警，而是给出“建议通风”“提前通风”“开启净化”“降低空间负载”等可执行建议；当采取开窗、开门、风扇或净化器等干预措施后，系统继续监测数据变化，让用户看到干预是否有效。  
When the environmental condition deteriorates, the system does not merely trigger an alarm; it provides actionable suggestions such as “ventilation recommended,” “ventilate in advance,” “turn on air purification,” or “reduce space load.” After interventions such as opening windows or doors, using fans, or turning on air purifiers, the system continues to monitor data changes so that users can see whether the intervention is effective.

## 二、项目背景与设计初衷 / II. Project Background and Design Motivation

我们学校坐落于中国东北，在这种高纬度寒冷地区冬季教室往往存在各类通风困境：冬天漫长的低温使长时间自然开窗通风难以持续，而部分大学阶梯教室、自习室面积大、建筑老旧，缺少稳定的主动机械换风条件。  
Our university is located in Northeast China, where classrooms in high-latitude cold regions often face multiple ventilation challenges in winter: the long-lasting low temperatures make prolonged natural window ventilation difficult to sustain, while some university lecture halls and study rooms are large, located in older buildings, and lack stable active mechanical ventilation conditions.

长时间密闭会导致 CO₂ 浓度升高、教室内各种人体呼出的气溶胶增多导致空气质量下降。  
Prolonged enclosure can lead to increased CO₂ concentration and a higher accumulation of human-exhaled aerosols in the classroom, resulting in degraded air quality.

此时学生容易出现困倦、注意力下降等问题；而在人员密集且通风不足的环境中，呼吸道疾病传播风险也可能随之增加。  
Under such conditions, students are more likely to feel drowsy and experience reduced concentration; in densely occupied and poorly ventilated spaces, the risk of respiratory disease transmission may also increase.

并且对于有慢性呼吸道疾病，对环境空气质量特别敏感的人群来说，往往察觉到空气质量不佳或者感到不舒服时往往为时已晚，这点本人深有体会。  
For people with chronic respiratory diseases or those who are particularly sensitive to environmental air quality, it is often already too late by the time they notice poor air quality or feel discomfort, which is something I have personally experienced.

并且在真实教室里，空气质量问题往往并不直观。  
In real classrooms, air-quality problems are often not directly visible.

一个教室是否已经通风不足，学生和老师通常只能依靠体感判断：觉得闷、困、热、潮，或者等到有人主动提醒才开窗。  
Students and teachers usually judge whether a classroom is insufficiently ventilated only through physical sensations—feeling stuffy, sleepy, hot, or humid—or by waiting until someone actively reminds them to open the windows.

但 CO2 的升高、人员密度的变化、颗粒物的积累和温湿度偏离舒适区，都会在无形中影响学习状态、呼吸舒适度和室内环境健康。  
However, rising CO₂ levels, changes in occupancy density, particulate matter accumulation, and temperature or humidity drifting away from the comfort zone can all subtly affect learning performance, respiratory comfort, and indoor environmental health.

我最初构思这个项目时，想解决的不是单一传感器读数问题，而是一个更完整的问题：如何让教室具备“自我提醒”的能力。  
When I first conceived this project, I was not trying to solve the problem of a single sensor reading, but a more complete question: how to give classrooms the ability to “remind themselves.”

也就是说，系统不仅要知道当前 CO2 是多少、PM2.5 是多少、教室人员密度如何，还要能把这些数据合在一起，判断现在是否需要采取行动，或者对未来的、趋势做出预判，来提醒用户采取措施：例如提醒开窗通风；或是向呼吸道敏感人群发出警告，建议佩戴口罩等减低自身感染风险的措施等扽，并用简单易读的方式表达出来，而不是仅告知各类数据指标，这样的指标并不警醒，并不能很好的提示用户可能存在的风险。  
In other words, the system should not only know the current CO₂ level, PM2.5 concentration, and classroom occupancy density, but also integrate these data to determine whether action is needed now, or predict future trends and remind users to take measures, such as opening windows for ventilation, warning respiratory-sensitive individuals, or recommending protective actions such as wearing masks to reduce personal infection risk. These results should be expressed in a simple and readable way rather than merely presenting separate data indicators, because raw indicators alone are not sufficiently alerting and do not effectively communicate potential risks to users.

在项目方案中，我把系统总结为一个闭环：监测 -> 研判 -> 提示 -> 通风/净化/提醒敏感人群防护 -> 再监测验证。  
In the project design, I summarize the system as a closed loop: monitoring -> assessment -> notification -> ventilation/purification/protection reminders for sensitive groups -> continued monitoring and verification.

这个闭环也是我后续代码实现的主线。  
This closed loop also serves as the main thread for my subsequent code implementation.

当前项目已经完成了多类传感器驱动、共享数据结构、热成像占用率检测、空气质量综合评价、Wi-Fi 遥测上传、本地 Web 看板和 LED 显示驱动基础，已经从最初的概念方案推进到了可以联调和演示的数据系统原型。  
At present, the project has completed the foundations of multiple sensor drivers, shared data structures, thermal-imaging-based occupancy detection, integrated air-quality evaluation, Wi-Fi telemetry upload, a local Web dashboard, and LED display drivers. It has progressed from the initial concept into a data-system prototype that can be jointly debugged and demonstrated.

## 三、系统整体架构 / III. Overall System Architecture

我将 ClassGuard 拆成三个层次：感知层、研判层和展示层。  
I divide ClassGuard into three layers: the sensing layer, the assessment layer, and the presentation layer.

感知层负责采集真实环境数据。  
The sensing layer is responsible for collecting real environmental data.

目前系统面向 ESP32-S3 开发，已接入或预留的传感模块包括 SCD41 CO2 传感器、SHT35 温湿度传感器、PMS5003 颗粒物传感器、MLX90640 32x24 热成像阵列，和 HUB75 RGB LED 点阵屏驱动。  
The current system is developed for the ESP32-S3, with connected or reserved modules including the SCD41 CO₂ sensor, SHT35 temperature and humidity sensor, PMS5003 particulate matter sensor, MLX90640 32×24 thermal imaging array, and HUB75 RGB LED matrix display driver.

系统通过 I2C、UART、GPIO 等接口完成硬件数据读取与状态管理。  
The system reads hardware data and manages device status through interfaces such as I²C, UART, and GPIO.

研判层负责把传感器数据转化为可解释的环境结论。  
The assessment layer converts sensor data into interpretable environmental conclusions.

当前我已经实现了空气质量综合评价模块 `air_quality_evaluator`，它会把 CO2、PM1.0、PM2.5、PM10、温度、湿度和占用率放入同一个评价框架中，输出综合得分、等级、主要影响因素、单项红线触发项和建议动作。  
I have implemented the integrated air-quality evaluation module `air_quality_evaluator`, which places CO₂, PM1.0, PM2.5, PM10, temperature, humidity, and occupancy rate into a unified evaluation framework and outputs an overall score, level, major influencing factors, individual red-line triggers, and recommended actions.

同时，我实现了基于 MLX90640 的占用率检测服务，用热图中的人体热分布估计空间占用状态。  
I have also implemented an occupancy detection service based on the MLX90640, using human thermal distribution in the thermal map to estimate the occupancy status of the space.

展示层负责把结论传达给用户。  
The presentation layer is responsible for communicating the conclusions to users.

当前项目已经实现 ESP32 端 JSON 遥测上传、FastAPI 本地 Web 服务、SQLite 数据存储、实时/历史接口和浏览器看板；同时，项目中也完成了 HUB75 LED 点阵屏的驱动和基础测试接口，为后续门口提示牌式的本地显示提供基础。  
The project has already implemented JSON telemetry upload on the ESP32 side, a local FastAPI Web service, SQLite data storage, real-time and historical data interfaces, and a browser dashboard. At the same time, the HUB75 LED matrix display driver and basic test interfaces have been completed, providing the foundation for a future door-side local notification display.

## 四、当前已经完成的主要功能 / IV. Main Functions Completed So Far

### 1. ESP32-S3 主固件框架 / 1. ESP32-S3 Main Firmware Framework

我已经建立了基于 ESP-IDF 的主工程，`main.c` 中完成 NVS 初始化、传感器服务启动、热成像服务启动和遥测上传服务启动。  
I have established the main project based on ESP-IDF, and `main.c` completes NVS initialization, sensor service startup, thermal imaging service startup, and telemetry upload service startup.

项目代码不再把所有逻辑堆在一个文件里，而是拆分为传感器服务、热成像服务、空气质量评价、遥测上传、共享数据等模块。  
The project code no longer places all logic in a single file; instead, it is divided into modules such as sensor services, thermal imaging services, air-quality evaluation, telemetry upload, and shared data.

当前主固件已经具备统一数据快照结构，可以将环境数据、颗粒物数据、热成像数据、占用率结果和传感器错误状态集中保存，供上传、显示和后续控制逻辑复用。  
The current main firmware already has a unified data snapshot structure, which can centrally store environmental data, particulate matter data, thermal imaging data, occupancy results, and sensor error states for reuse in upload, display, and subsequent control logic.

### 2. 多传感器数据采集与融合基础 / 2. Foundation for Multi-Sensor Data Acquisition and Fusion

我已经实现或整理了 SHT35、SCD41、PMS5003、MLX90640、I2C 总线、HUB75 显示、对射传感器等驱动组件。  
I have implemented or organized driver components for the SHT35, SCD41, PMS5003, MLX90640, I²C bus, HUB75 display, and through-beam sensor.

系统能够围绕以下指标建立教室环境画像：  
The system can build a classroom environmental profile around the following indicators:

- CO2：反映通风不足和呼出空气累积。  
  CO₂: reflects insufficient ventilation and the accumulation of exhaled air.
- 温度、湿度：反映人体舒适度和呼吸环境条件。  
  Temperature and humidity: reflect human comfort and respiratory environmental conditions.
- PM1.0、PM2.5、PM10：反映细颗粒物、粗颗粒物和空气洁净度。  
  PM1.0, PM2.5, and PM10: reflect fine particles, coarse particles, and overall air cleanliness.
- MLX90640 热成像数据：提供匿名热分布和占用率估计。  
  MLX90640 thermal imaging data: provides anonymous thermal distribution and occupancy estimation.
- 设备状态：记录传感器是否正常、Wi-Fi 信号、设备 IP 和运行时间。  
  Device status: records whether sensors are functioning normally, Wi-Fi signal strength, device IP, and runtime.

这些数据已经统一进入正式遥测协议，ESP32 可以通过 `/api/telemetry` 上传到 Web 服务端。  
These data have been unified into the formal telemetry protocol, and the ESP32 can upload them to the Web server through `/api/telemetry`.

### 3. 空气质量综合评价算法 / 3. Integrated Air-Quality Evaluation Algorithm

我已经完成了 `air_quality_evaluator.c` 这个综合评价算法。  
I have completed the integrated evaluation algorithm in `air_quality_evaluator.c`.

这个模块不是简单求平均值，而是采用“分段评分 + 权重融合 + 红线约束 + 占用率动态修正”的综合评价方式。  
This module does not simply calculate an average value; instead, it adopts an integrated evaluation approach based on “segmented scoring + weighted fusion + red-line constraints + dynamic occupancy-based correction.”

系统会先把每个指标转化为 0-100 分，再根据不同指标的重要性进行加权。  
The system first converts each indicator into a score from 0 to 100, and then applies weights according to the importance of different indicators.

CO2 和 PM2.5 是核心指标，温湿度体现舒适性，PM1.0/PM10 提供颗粒物补充判断，占用率则作为空间负载和未来风险变化速度的提示因素。  
CO₂ and PM2.5 are core indicators; temperature and humidity represent comfort; PM1.0 and PM10 provide supplementary particulate matter judgment; and occupancy rate serves as an indicator of spatial load and the likely speed of future risk changes.

同时，我加入了红线机制。  
I have also added a red-line mechanism.

例如 CO2 超过 1000、1500、2000、3000 ppm 时，总分会受到不同程度限制；PM2.5、PM10、温度、湿度和占用率也有对应红线。  
For example, when CO₂ exceeds 1000, 1500, 2000, or 3000 ppm, the overall score is capped to different degrees; PM2.5, PM10, temperature, humidity, and occupancy rate also have corresponding red lines.

这样可以避免“某个关键指标已经很差，但被其他正常指标平均掉”的问题。  
This prevents the problem where “a critical indicator is already poor but is averaged out by other normal indicators.”

系统还会根据占用率动态调整 CO2、湿度和占用率本身的权重。  
The system also dynamically adjusts the weights of CO₂, humidity, and occupancy itself according to the occupancy rate.

当教室里人比较多时，即使 CO2 暂时还没有明显超标，系统也会更早提醒通风，因为高占用率意味着 CO2 和湿度可能更快上升。  
When many people are present in the classroom, the system can recommend ventilation earlier even if CO₂ has not yet significantly exceeded the threshold, because high occupancy means that CO₂ and humidity may rise more rapidly.

### 4. 基于热成像的占用率评估 / 4. Thermal-Imaging-Based Occupancy Evaluation

目前，我已经实现了基于 MLX90640 的占用率检测算法 V2。  
At present, I have implemented Version 2 of the occupancy detection algorithm based on the MLX90640.

MLX90640 输出的是 32x24 的低分辨率热图，总共 768 个温度像素。  
The MLX90640 outputs a low-resolution 32×24 thermal map with a total of 768 temperature pixels.

它看到的是热分布，而不是清晰人脸、衣着、动作或身份信息，因此天然更适合教室这种需要隐私保护的公共空间。  
It captures thermal distribution rather than clear facial features, clothing, actions, or identity information, which makes it naturally more suitable for public classroom spaces that require privacy protection.

同时这种低分辨率的热值图也便于使用诸如 ESP32 这种低价的 MCU 进行数据处理，避免了机器视觉识别人数所要使用的模块，诸如工业摄像头、算力板等更加昂贵的资源，进一步压缩整体系统的成本。  
At the same time, this low-resolution thermal map is convenient for low-cost MCUs such as the ESP32 to process, avoiding the need for more expensive resources typically required for machine-vision-based people counting, such as industrial cameras and computing boards, thereby further reducing the overall system cost.

热分布统计算法 V2 是基于我个人研究并设计的一套简单的算法。  
The Version 2 thermal distribution statistics algorithm is a simple algorithm that I designed based on my own research.

算法流程包括使用无人背景初始化、实时温度平滑、候选热源筛选、极端高温异常点剔除、人体参考温度估计、最终阈值生成、二值化、连通区域分析和多帧状态机。  
The algorithm process includes unoccupied background initialization, real-time temperature smoothing, candidate heat-source filtering, extreme high-temperature outlier removal, human reference temperature estimation, final threshold generation, binarization, connected-component analysis, and a multi-frame state machine.

它最终输出的不只是“有人/无人”，还包括占用率、热强度分数、综合占用分数这样的具体比例；此外还可以返回例如最大连通区域、有效像素数、背景温度、干扰阈值等调试和解释字段，可以方便进行参数设定和检测阈值的设定。  
Its final output is not limited to “occupied/unoccupied”; it also includes specific quantitative values such as occupancy rate, thermal intensity score, and integrated occupancy score. In addition, it can return debugging and explanation fields such as the largest connected region, valid pixel count, background temperature, and interference threshold, making it easier to set parameters and detection thresholds.

相较于 V1 版本，V2 版本不再采用第一版中的校准算法，并且舍弃了基于热分布标准差的高阈值算法；V1 版本算法更多关注最高温度点，这导致一整个连续热源会导致自动阈值的值被设置的极高，这样不便于热量统计。  
Compared with Version 1, Version 2 no longer uses the calibration algorithm from the first version and abandons the high-threshold method based on the standard deviation of thermal distribution. The Version 1 algorithm focused more on the highest temperature point, which could cause the automatic threshold to be set extremely high when a continuous heat source appeared, making thermal statistics difficult.

V2 版本使用更加保守的舍弃异常点算法，并且使用 top10%、top20%、top30% 的分区域平均值来更加精确地测算人体可能的温度，并使用这三个区域平均值按照 6.5：2.5：1 的加权算法来得到一个更加合理的人体温度。  
Version 2 uses a more conservative outlier-removal algorithm and applies the regional averages of the top 10%, top 20%, and top 30% temperature pixels to estimate possible human body temperature more accurately. These three regional averages are then combined using a 6.5:2.5:1 weighting algorithm to obtain a more reasonable human reference temperature.

这样既能减小高温异常区域的影响以减小单个小异常热源导致的误判，也能同时更加关注于高温区域的人体温度值，同时尽可能将人体的高温核心和较低温边缘平均，输出一个更加合理、中等大小的占用度。  
This approach reduces the influence of abnormally high-temperature regions and lowers the risk of misjudgment caused by a single small abnormal heat source, while still paying more attention to human temperature values in high-temperature regions. It also averages the high-temperature human core and lower-temperature edges as much as possible, producing a more reasonable and moderate occupancy estimate.

此外 V2 算法还基于初始化时候的背景温度和噪声温度差来实现智能化的阈值划分，能够更加适应整体画面的部分噪声影响。  
In addition, the Version 2 algorithm performs intelligent threshold division based on the initialized background temperature and the noise temperature difference, making it more adaptable to partial noise effects in the overall image.

这个功能使 ClassGuard 不必依赖普通摄像头做人脸或人体识别，也不需要昂贵的深度相机或专业人数统计设备。  
This function allows ClassGuard to avoid relying on ordinary cameras for face or human-body recognition, and it also removes the need for expensive depth cameras or professional people-counting devices.

它更适合输出“低、中、高、很高”这类占用等级，而不是承诺精确人数。  
It is more suitable for outputting occupancy levels such as “low,” “medium,” “high,” and “very high,” rather than promising an exact people count.

这种设计很符合校园场景对隐私和成本的要求。  
This design fits the privacy and cost requirements of campus scenarios very well.

### 5. Wi-Fi 上传与 Web 看板 / 5. Wi-Fi Upload and Web Dashboard

我已经实现了 ESP32 端 Wi-Fi 连接和 HTTP 上传逻辑。  
I have implemented the Wi-Fi connection and HTTP upload logic on the ESP32 side.

设备会把传感器数据、MLX90640 热成像统计、占用率结果、空气质量综合评价结果和设备状态打包成 JSON，上传到本地 Web 服务。  
The device packages sensor data, MLX90640 thermal imaging statistics, occupancy results, integrated air-quality evaluation results, and device status into JSON and uploads them to the local Web service.

Web 端已经完成 FastAPI 服务、SQLite 存储、最新数据查询、历史数据查询、SSE 实时事件接口和静态看板页面。  
On the Web side, the FastAPI service, SQLite storage, latest-data query, historical-data query, SSE real-time event interface, and static dashboard page have been completed.

服务端还能把 ESP32 上传的精简评价代码扩展成中文等级、警告标签和用户建议文案。  
The server can also expand the simplified evaluation codes uploaded by the ESP32 into Chinese levels, warning labels, and user-facing recommendation text.

这样，ESP32 不需要承担复杂文本生成和界面渲染压力，Web 端可以负责更友好的可视化展示。  
In this way, the ESP32 does not need to handle complex text generation or interface rendering, while the Web side can provide a more user-friendly visual presentation.

当前 Web 看板可以展示设备状态、综合评分、建议动作、CO2、温湿度、PM1.0、PM2.5、PM10、占用率、热成像统计和历史趋势曲线。  
The current Web dashboard can display device status, integrated score, recommended actions, CO₂, temperature and humidity, PM1.0, PM2.5, PM10, occupancy rate, thermal imaging statistics, and historical trend curves.

它让项目从“串口里有数据”进一步变成“用户可以看懂的数据界面”。  
It moves the project beyond “data appearing in the serial port” toward a data interface that users can actually understand.

### 6. LED 本地显示基础 / 6. Foundation for Local LED Display

项目中已经完成 HUB75 RGB LED 点阵屏驱动的 GPIO 初始化、输出开关、GPIO 测试和彩条显示接口。  
The project has completed GPIO initialization, output switching, GPIO testing, and color-bar display interfaces for the HUB75 RGB LED matrix driver.

我的目标是把它作为教室内部或设备外壳上的本地提示端，让用户不用打开网页，也能通过颜色、等级或简短提示快速理解当前空气状态。  
My goal is to use it as a local notification terminal inside the classroom or on the device enclosure, so that users can quickly understand the current air condition through colors, levels, or short messages without opening a webpage.

Web 看板适合详细分析和历史回看，LED 端适合现场即时提醒。  
The Web dashboard is suitable for detailed analysis and historical review, while the LED terminal is suitable for immediate on-site reminders.

两者结合后，ClassGuard 不只是一个单独的监测站，也能成为一个真正放在教室内部的“空气守门员”。  
When combined, ClassGuard becomes not merely an isolated monitoring station, but a true “air guardian” that can be placed inside a classroom.

## 五、核心创新点 / V. Core Innovations

### 创新点一：多数据融合综合评价 / Innovation 1: Integrated Evaluation Through Multi-Data Fusion

传统空气质量设备往往只突出某一个指标，例如 PM2.5 或 CO2。  
Traditional air-quality devices often emphasize only one indicator, such as PM2.5 or CO₂.

但教室环境问题并不是由单一指标决定的：CO2 代表通风不足，颗粒物代表污染和净化需求，温湿度影响舒适度和呼吸体验，占用率影响环境恶化速度和空间负载。  
However, classroom environmental problems are not determined by a single indicator: CO₂ reflects insufficient ventilation, particulate matter indicates pollution and purification needs, temperature and humidity affect comfort and respiratory experience, and occupancy rate influences the speed of environmental deterioration and spatial load.

我的创新点在于把这些数据放进同一个评价框架中，用综合评分、等级、红线、主要原因和行动建议来表达环境状态。  
My innovation lies in placing these data into the same evaluation framework and expressing environmental status through an integrated score, level, red lines, main causes, and recommended actions.

用户看到的不只是“CO2=1380 ppm”这种专业读数，而是可以直接理解的结论：当前综合评分是多少、主要问题是不是通风不足、是否建议提前通风、是否需要净化或降温除湿。  
Users do not only see a technical reading such as “CO₂ = 1380 ppm”; they see directly understandable conclusions, such as the current integrated score, whether the main problem is insufficient ventilation, whether early ventilation is recommended, and whether purification, cooling, or dehumidification is needed.

这种多数据融合方式有三个优势。  
This multi-data fusion approach has three advantages.

第一，它更接近真实教室环境。  
First, it is closer to the real classroom environment.

一个满座但暂时 CO2 不高的教室，和一个空教室但 PM2.5 偏高的教室，处理建议是不一样的。  
A fully occupied classroom with temporarily normal CO₂ levels and an empty classroom with elevated PM2.5 should receive different management suggestions.
第二，它更可解释。  
Second, it is more interpretable.
系统会保留单项得分、权重、红线和主要影响因素，而不是输出一个黑箱结论。  
The system retains individual scores, weights, red lines, and major influencing factors instead of outputting a black-box conclusion.

第三，它更适合行动。  
Third, it is more action-oriented.

最终输出不是“空气好/不好”这样笼统的判断，而是面向教室管理的具体建议，例如通风、提前通风、净化、降温、除湿或继续监测。  
The final output is not a vague judgment such as “air quality is good/bad,” but specific classroom-management suggestions such as ventilation, early ventilation, purification, cooling, dehumidification, or continued monitoring.

### 创新点二：利用热成像进行占用率评估，成本低且隐私友好 / Innovation 2: Low-Cost and Privacy-Friendly Occupancy Evaluation Using Thermal Imaging

教室占用率是本项目非常关键的变量。  
Classroom occupancy rate is a very important variable in this project.

人员越密集，CO2、湿度和呼吸相关环境风险往往变化越快。  
The more densely people occupy a space, the faster CO₂, humidity, and respiratory-related environmental risks tend to change.

但如果使用普通摄像头做人群识别，会带来隐私解释成本，也可能需要更高算力和更复杂算法，由此带来的是更高的成本和模型训练及调试的难度。  
However, using ordinary cameras for crowd recognition introduces privacy concerns and may require higher computing power and more complex algorithms, which leads to higher costs and greater difficulty in model training and debugging.

我选择使用 MLX90640 低分辨率热成像阵列来估计占用率。  
I chose to use the MLX90640 low-resolution thermal imaging array to estimate occupancy rate.

它的优势是只采集热分布，不采集人脸、身份和清晰行为图像。  
Its advantage is that it only collects thermal distribution and does not collect faces, identities, or clear behavioral images.

与传统机器视觉相比，它不需要训练复杂的人体检测模型，也不依赖云端 AI 推理；与深度相机或专业雷达相比，它成本更低，硬件结构更简单，更适合创客项目和校园原型部署。  
Compared with traditional machine vision, it does not require training complex human-detection models and does not depend on cloud-based AI inference; compared with depth cameras or professional radar systems, it is lower in cost, simpler in hardware structure, and more suitable for maker projects and campus prototype deployment.

为了让热成像不仅停留在“显示热图”，我实现了专门的占用率检测算法。  
To make thermal imaging more than just “displaying a thermal map,” I implemented a dedicated occupancy detection algorithm.

它不是固定用某个温度阈值判断人体，而是先建立无人背景，再根据当前热源分布动态估计人体参考温度，并通过连通区域和多帧状态机减少误判。  
It does not judge human presence using a fixed temperature threshold; instead, it first builds an unoccupied background, then dynamically estimates a human reference temperature based on the current heat-source distribution, and reduces misjudgment through connected components and a multi-frame state machine.

这样可以适应不同季节、不同教室背景和局部热源干扰。  
This allows the algorithm to adapt to different seasons, classroom backgrounds, and local heat-source interference.

我认为这个创新点的价值不只是“用了热成像”，而是把热成像转化成了可用于环境评价模型的占用率变量。  
I believe the value of this innovation is not merely that “thermal imaging is used,” but that thermal imaging is converted into an occupancy variable that can be used in the environmental evaluation model.

它让系统能够提前感知空间负载，从而在 CO2 尚未明显升高时给出更及时的通风建议。  
It enables the system to sense spatial load in advance, allowing it to provide more timely ventilation suggestions before CO₂ rises significantly.

### 创新点三：多样化综合展示，LED 与 Web 协同呈现 / Innovation 3: Diversified Integrated Presentation Through LED and Web Collaboration

一个环境监测系统能否被真正使用，不只取决于传感器是否准确，也取决于普通用户能否快速看懂。  
Whether an environmental monitoring system can be truly used depends not only on sensor accuracy, but also on whether ordinary users can quickly understand the information.

ClassGuard 的展示设计不是把所有数字堆在屏幕上，而是分成现场即时提示和详细数据看板两类。  
ClassGuard’s presentation design does not pile all numbers onto the screen; instead, it divides information into two categories: immediate on-site reminders and a detailed data dashboard.

LED 点阵屏适合作为教室的直观提示端，可以用颜色、等级、分数或简短文字告诉用户当前状态。  
The LED matrix screen is suitable as an intuitive notification terminal for the classroom, using colors, levels, scores, or short text to inform users of the current status.

学生和老师不需要打开手机或电脑，就能知道现在是否建议通风。  
Students and teachers can know whether ventilation is currently recommended without opening a phone or computer.

Web 看板则承担详细展示和管理分析功能。  
The Web dashboard, meanwhile, is responsible for detailed presentation and management analysis.

它可以显示实时值、历史曲线、综合评分、建议动作、红线记录、设备状态和占用率变化。  
It can display real-time values, historical curves, integrated scores, recommended actions, red-line records, device status, and occupancy changes.

管理者或者学生老师都可以通过 Web 看板观察开窗前后、上课前后、人员变化前后的空气质量趋势，从而判断干预是否有效。  
Managers, students, and teachers can use the Web dashboard to observe air-quality trends before and after window opening, before and after class, and before and after changes in occupancy, thereby judging whether interventions are effective.

这种多端展示方式让项目更完整：LED 负责“现场提醒”，Web 负责“数据解释”，ESP32 负责“边缘采集与计算”，服务端负责“存储、回看和友好文案”。  
This multi-terminal presentation approach makes the project more complete: the LED handles “on-site reminders,” the Web side handles “data explanation,” the ESP32 handles “edge acquisition and computation,” and the server handles “storage, review, and user-friendly text.”

它将技术数据转化为用户能理解、能执行、能复盘的环境管理信息。  
It transforms technical data into environmental management information that users can understand, act on, and review.

## 六、项目价值 / VI. Project Value

ClassGuard 的直接价值，是让教室空气状态从不可见变为可见。  
The direct value of ClassGuard is that it makes classroom air conditions visible instead of invisible.

它可以提醒老师和学生在合适的时间开窗通风、开启净化器或调整空间使用方式，亦或是为呼吸道敏感人群提供防护建议，由此减少凭感觉管理空气的盲区。  
It can remind teachers and students to open windows for ventilation, turn on air purifiers, or adjust space usage at appropriate times, and it can also provide protective suggestions for respiratory-sensitive individuals, thereby reducing the blind spots caused by managing air quality purely by feeling.

它的教育价值也很明显。  
Its educational value is also evident.

学生可以通过 Web 看板看到 CO2、PM2.5、温湿度和占用率随时间变化的曲线，理解通风、人员密度和空气质量之间的关系。  
Students can use the Web dashboard to view curves showing how CO₂, PM2.5, temperature, humidity, and occupancy rate change over time, helping them understand the relationship among ventilation, occupancy density, and air quality.

这个项目不仅是一个设备，也可以成为校园环境教育和 STEM 教学的载体。  
This project is not only a device, but can also serve as a platform for campus environmental education and STEM teaching.

它的推广价值在于成本低、全架构开源、使用通用模块、隐私友好。  
Its value for broader deployment lies in its low cost, fully open architecture, use of common modules, and privacy-friendly design.

相比安装传统摄像头或昂贵的商业人数统计系统，ClassGuard 更适合在教室、图书馆、自习室、会议室和培训机构中做低成本试点。  
Compared with installing traditional cameras or expensive commercial people-counting systems, ClassGuard is more suitable for low-cost pilot deployment in classrooms, libraries, study rooms, meeting rooms, and training institutions.

## 七、后续迭代方向 / VII. Future Iteration Directions

后续我计划继续完善以下方向：  
In future iterations, I plan to continue improving the following aspects:

1. 将 LED 点阵屏从基础驱动推进到正式信息页面，显示综合评分、等级和建议动作。  
   Upgrade the LED matrix screen from basic driver functionality to formal information pages that display the integrated score, level, and recommended actions.
2. 增加热图可视化上传或压缩传输，让 Web 端能够展示匿名热分布。  
   Add thermal-map visualization upload or compressed transmission so that the Web side can display anonymous thermal distribution.
3. 继续收集真实教室数据，校准占用率阈值、CO2 权重和红线规则。  
   Continue collecting real classroom data to calibrate occupancy thresholds, CO₂ weights, and red-line rules.
4. 增加通风前后对比报告，让系统能自动评估一次干预是否有效。  
   Add before-and-after ventilation comparison reports so that the system can automatically evaluate whether an intervention is effective.
5. 支持多教室节点管理，为学校后勤或实验室管理提供更完整的环境看板。  
   Support multi-classroom node management to provide a more complete environmental dashboard for school logistics or laboratory management.
6. 增添 ESP32 自动矫正热成像基准，基于 ESP32 联网功能，来获取时间或者其他智能教室项目（如获取课程时间信息等），定期在教室无人时候自动对热成像背景进行更新，来适应不同时间段（早中下午晚上），或者不同季节的温度不同造成的影响。  
   Add automatic ESP32-based correction of the thermal imaging baseline by using the ESP32 networking function to obtain time information or connect with other smart-classroom projects, such as course schedule information, and periodically update the thermal background when the classroom is unoccupied, so as to adapt to different periods of the day—morning, noon, afternoon, and evening—or the effects of seasonal temperature differences.
7. 来让占有率统计更加智能、准确，可以自动适应不同情景下的不同背景温度的干扰。  
   This would make occupancy statistics more intelligent and accurate, allowing the system to automatically adapt to interference caused by different background temperatures in different scenarios.
8. 可以联合其他开源的室外空气质量信息，给出更加完整的建议，例如当外部空气 PM2.5 高于室内的时候，不会输出类似开窗通风这种可能恶化室内空气质量的建议，可能会建议打开室内通风净化，或是打开教室门与教学楼内进行简单的通风。  
   The system can be combined with other open-source outdoor air-quality information to provide more complete recommendations. For example, when outdoor PM2.5 is higher than indoor PM2.5, it should avoid recommending window ventilation that may worsen indoor air quality, and may instead suggest indoor ventilation and purification or simple ventilation by opening the classroom door to the teaching building.
9. 可以进一步使用诸如 2.5GHz 人体雷达等对教室内是否有人进行判断，来修正热成像受到环境干扰时的误判断，并能够让热成像智能校准数据，来达到更加精确的占有率判断。  
   The system can further use technologies such as 2.5 GHz human-presence radar to determine whether people are present in the classroom, correct misjudgments caused by environmental interference in thermal imaging, and enable intelligent calibration of thermal imaging data for more accurate occupancy estimation.

## 八、总结 / VIII. Conclusion

我们 ClassGuard 的目标，不是再做一个普通空气质量显示器，而是做一个能理解教室场景、能保护隐私、能融合多源数据、能给出行动建议的“空气守门员”。  
The goal of ClassGuard is not to create another ordinary air-quality display, but to build an “air guardian” that understands classroom scenarios, protects privacy, integrates multi-source data, and provides actionable recommendations.

它的创新性体现在三个方面：用多数据融合替代单指标报警，用低分辨率热成像实现低成本的隐私友好的占用率评估，用 LED 与 Web 多端展示把复杂数据转化为易懂的环境管理建议。  
Its innovation is reflected in three aspects: replacing single-indicator alarms with multi-data fusion, using low-resolution thermal imaging to achieve low-cost and privacy-friendly occupancy evaluation, and transforming complex data into easy-to-understand environmental management suggestions through coordinated LED and Web presentation.

从当前实现来看，项目已经完成了从传感器采集、热成像占用率、综合评价、Wi-Fi 上传到 Web 看板的主要数据链路。  
Based on the current implementation, the project has completed the main data chain from sensor acquisition, thermal-imaging-based occupancy estimation, integrated evaluation, and Wi-Fi upload to the Web dashboard.

下一步，我会继续把本地 LED 提示、真实场景验证和展示材料打磨完整，让这个系统更像一个真正可以放进教室的产品原型。  
Next, I will continue refining the local LED notification function, real-scenario validation, and presentation materials, so that the system becomes closer to a real product prototype that can be placed inside classrooms.
