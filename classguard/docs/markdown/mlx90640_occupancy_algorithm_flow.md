# 基于 MLX90640 热阵列的占用率判断算法实现流程

## 1. 设计目标

本算法面向 ESP32-S3 + MLX90640 的低分辨率热阵列检测场景，用于根据热图数据判断区域内是否有人，并输出一个可用于 LED 点阵、网页端或串口调试的占用率结果。

MLX90640 输出的是 `32 × 24 = 768` 个温度像素点。直接用固定温度阈值，例如“高于 28 ℃ 判定为有人”，在季节变化、空调房、阳光照射、设备发热等环境下容易失效。因此本方案采用“相对背景温差”的思路，而不是直接判断绝对温度。

核心思想可以概括为：

```text
当前温度图 T
    ↓
背景温度图 B
    ↓
温差图 D = T - B
    ↓
动态阈值提取人体候选区域
    ↓
连通区域分析排除噪声
    ↓
计算占用率与有人/无人状态
```

---

## 2. 数据定义

### 2.1 温度矩阵

MLX90640 每帧输出 768 个浮点温度值，可按行列组织为：

```text
T[y][x], y = 0~23, x = 0~31
```

也可以在程序中保存为一维数组：

```cpp
float frame[768];
```

一维索引与二维坐标之间的关系为：

```cpp
index = y * 32 + x;
x = index % 32;
y = index / 32;
```

### 2.2 背景矩阵

背景矩阵表示“无人状态下，每个像素通常看到的背景温度”。

```cpp
float background[768];
```

背景不是一个单一温度值，而是 768 个像素各自的背景值。这样可以适应墙面、桌面、窗户、设备外壳等不同区域本身温度不同的情况。

### 2.3 温差矩阵

温差矩阵用于表示当前帧相对于背景的升温程度：

```cpp
delta[i] = frame[i] - background[i];
```

其中：

```text
delta[i] > 0：当前像素比背景更热
delta[i] ≈ 0：接近背景
delta[i] < 0：当前像素比背景更冷
```

占用判断主要基于 `delta`，而不是直接基于 `frame`。

---

## 3. 初始化流程

系统刚启动时，建议先采集一段无人背景帧。如果现场可以保证镜头前无人，初始化过程会更稳定。

### 3.1 推荐初始化参数

| 参数 | 建议值 | 说明 |
|---|---:|---|
| 初始化帧数 | 30~90 帧 | 如果采样为 10 Hz，大约对应 3~9 秒 |
| MLX90640 输出频率 | 8~16 Hz 起步 | 调试稳定后再提高到 32 Hz |
| 初始化方式 | 多帧平均或中位数 | 中位数更抗瞬时噪声 |
| 是否输出结果 | 初始化期间不输出正式占用率 | 避免背景未稳定时误判 |

### 3.2 初始化算法

```text
启动系统
    ↓
连续采集 N 帧 MLX90640 温度矩阵
    ↓
对每个像素求平均值或中位数
    ↓
得到 background[768]
    ↓
进入实时检测流程
```

使用平均值时：

```cpp
background[i] = sum(frame_k[i]) / N;
```

若使用中位数，需要对每个像素收集 N 个温度值后排序。ESP32-S3 上为了节省内存和计算量，初期可以先用平均值实现，后续再优化。

---

## 4. 实时检测主流程

实时检测流程建议固定周期运行，例如每 100 ms 或 200 ms 处理一次热图。即使 MLX90640 实际采样为 16~30 Hz，占用率输出也不需要每帧刷新，3~10 Hz 的输出频率通常已经足够。

```text
读取当前热图 frame[768]
    ↓
时间滤波，降低单帧噪声
    ↓
计算温差 delta[768]
    ↓
计算动态阈值 threshold
    ↓
生成候选热区 mask[768]
    ↓
去除孤立噪声
    ↓
连通区域分析
    ↓
计算占用率 occupancy
    ↓
根据连续帧状态输出无人 / 疑似有人 / 有人
    ↓
更新背景 background
```

---

## 5. 时间滤波

MLX90640 单帧数据可能存在轻微噪声，建议对当前帧做简单时间滤波。初期推荐使用指数滑动平均：

```cpp
smooth[i] = beta * frame[i] + (1.0f - beta) * smooth[i];
```

推荐：

| 参数 | 建议值 | 说明 |
|---|---:|---|
| beta | 0.3~0.6 | 数值越大，响应越快；数值越小，越平滑 |

初始化时可以直接令：

```cpp
smooth[i] = frame[i];
```

处理流程中后续使用 `smooth[i]` 代替原始 `frame[i]`。

---

## 6. 动态背景建模

### 6.1 背景更新公式

背景可以用指数滑动平均缓慢更新：

```cpp
background[i] = (1.0f - alpha) * background[i] + alpha * smooth[i];
```

推荐：

| 参数 | 建议值 | 说明 |
|---|---:|---|
| alpha | 0.002~0.02 | 背景更新速度 |
| 普通室内 | 0.005 | 较稳妥 |
| 空调/日照变化较明显 | 0.01~0.02 | 适应更快，但要避免把人学成背景 |

### 6.2 人体候选区域不更新背景

为了避免长时间站立的人被系统学习成背景，需要对疑似人体区域冻结背景更新。

```cpp
if (is_candidate_pixel) {
    // 不更新 background[i]
} else {
    background[i] = (1 - alpha) * background[i] + alpha * smooth[i];
}
```

实际实现时，可以先用一个较低的候选阈值判断是否冻结背景：

```cpp
freeze_threshold = max(1.0f, dynamic_threshold * 0.7f);
```

当：

```cpp
delta[i] > freeze_threshold
```

则该像素暂时不参与背景更新。

---

## 7. 温差图计算

温差图是整个算法的核心：

```cpp
delta[i] = smooth[i] - background[i];
```

为了避免异常低温或偶然负值影响判断，可以对 `delta` 做裁剪：

```cpp
if (delta[i] < -5.0f) delta[i] = -5.0f;
if (delta[i] > 20.0f) delta[i] = 20.0f;
```

这不是必须步骤，但在实际调试中有助于防止异常值污染统计。

---

## 8. 动态阈值计算

### 8.1 简化方案：均值 + 标准差

ESP32-S3 上实现最简单的是：

```cpp
mean = average(delta)
std = standard_deviation(delta)
threshold = mean + k * std
threshold = max(threshold, min_delta_threshold)
```

推荐参数：

| 参数 | 建议值 | 说明 |
|---|---:|---|
| k | 2.0~3.0 | 越大越保守，误检少但可能漏检 |
| min_delta_threshold | 1.2~1.8 ℃ | 最低温差阈值 |

示例：

```cpp
threshold = max(1.5f, mean + 2.5f * std);
```

这种方法实现简单，适合第一版固件。

### 8.2 更稳健方案：中位数 + MAD

如果后续希望增强抗异常热源能力，可以使用中位数和 MAD：

```text
median_D = median(delta)
MAD = median(|delta - median_D|)
sigma ≈ 1.4826 × MAD
threshold = median_D + k × sigma
```

再加最低阈值限制：

```cpp
threshold = max(1.2f, median_D + 3.0f * sigma);
```

MAD 对异常热源不敏感，例如画面中出现一个小型发热设备时，它不会像均值/标准差那样被明显拉高。

---

## 9. 候选热区生成

根据动态阈值生成二值掩膜：

```cpp
mask[i] = delta[i] > threshold ? 1 : 0;
```

为了进一步利用你原本的“按温度区间统计”思想，可以同时统计不同温差区间的像素数量：

| ΔT 区间 | 统计变量 | 可能含义 |
|---|---|---|
| ΔT < 1 ℃ | bin_0 | 背景 |
| 1~2 ℃ | bin_1 | 轻微升温 |
| 2~4 ℃ | bin_2 | 明显热源候选 |
| 4~6 ℃ | bin_3 | 强热源 |
| >6 ℃ | bin_4 | 近距离人体或其他热源 |

建议统计 `ΔT` 分布，而不是统计绝对温度分布。这样即使室内温度从冬季 18 ℃ 变成夏季 29 ℃，算法仍然关注“相对背景是否升温”。

---

## 10. 孤立噪声去除

由于 MLX90640 分辨率较低，某些单点噪声可能被误判为热源。可以用邻域计数去除孤立点。

对每个 `mask[i] == 1` 的像素，统计周围 8 邻域中候选像素数量：

```text
周围候选点数量 < 1 或 < 2
    ↓
认为是孤立噪声，将该点置为 0
```

建议初始规则：

```cpp
if (neighbor_count < 1) {
    mask[i] = 0;
}
```

如果环境噪声较多，可以提高到：

```cpp
neighbor_count < 2
```

但阈值过高可能漏掉远距离人体，因为远处人体在热图中可能只占很少像素。

---

## 11. 连通区域分析

候选热区不是只看像素数量，还要看是否形成了连续区域。人体在热图中通常表现为一个或多个连续热斑，而不是完全随机的散点。

### 11.1 分析指标

对每个连通区域计算：

| 指标 | 含义 |
|---|---|
| area | 连通区域像素数量 |
| mean_delta | 区域平均温差 |
| max_delta | 区域最大温差 |
| bbox_width | 外接矩形宽度 |
| bbox_height | 外接矩形高度 |
| score | 区域得分 |

### 11.2 人体候选区域规则

可先采用经验规则：

```text
area >= 4
mean_delta >= 1.3 ℃
max_delta >= 2.0 ℃
```

近距离人体可能占几十到上百个像素；远距离人体可能只占 4~10 个像素。实际阈值应根据 MLX90640 安装高度、视场角、检测距离和镜头朝向调整。

如果设备安装在房间角落，看的是较大区域，可以适当降低 `area`；如果设备离目标很近或视场内有明显热源干扰，则需要提高 `area` 与 `mean_delta`。

---

## 12. 占用率计算方法

### 12.1 简单像素占用率

最简单的占用率为：

```cpp
occupancy_ratio = valid_human_pixels / 768.0f;
```

其中 `valid_human_pixels` 是通过连通区域筛选后的有效人体候选像素数量。

示例：

```text
有效人体像素数 = 40
占用率 = 40 / 768 ≈ 5.2%
```

这个值不是严格的“房间面积占用率”，而是“热图视场中的人体热异常占比”。

### 12.2 加权占用率

更平滑的方式是根据温差强度给每个像素打分：

```cpp
score_i = clamp((delta[i] - delta_low) / (delta_high - delta_low), 0, 1);
```

推荐初始参数：

```cpp
delta_low = 1.0f;
delta_high = 5.0f;
```

那么：

```text
ΔT <= 1 ℃：得分 0
ΔT = 3 ℃：得分约 0.5
ΔT >= 5 ℃：得分 1
```

总占用率：

```cpp
occupancy_score = sum(score_i over valid human regions) / 768.0f;
```

这种方式比单纯二值化更稳定，因为它同时考虑了热区面积和热区强度。

### 12.3 推荐输出指标

建议系统同时输出以下数据，便于调试和后续优化：

| 输出项 | 说明 |
|---|---|
| occupancy_ratio | 有效热区像素占比 |
| occupancy_score | 加权占用得分 |
| max_region_area | 最大热区面积 |
| max_region_delta | 最大热区温差 |
| bin_0~bin_4 | ΔT 分箱统计 |
| state | 无人 / 疑似有人 / 有人 |

---

## 13. 状态机设计

占用判断不建议只根据单帧结果直接切换，否则人走动、遮挡、热噪声都会造成输出抖动。建议加入连续帧状态机。

### 13.1 状态定义

```cpp
enum OccupancyState {
    UNOCCUPIED,
    POSSIBLE_OCCUPIED,
    OCCUPIED
};
```

### 13.2 状态转移规则

推荐初始规则：

| 条件 | 状态变化 |
|---|---|
| 连续 3 帧检测到有效人体热区 | `UNOCCUPIED` → `POSSIBLE_OCCUPIED` |
| 连续 5 帧检测到有效人体热区 | `POSSIBLE_OCCUPIED` → `OCCUPIED` |
| 连续 30 帧未检测到有效人体热区 | `OCCUPIED` → `POSSIBLE_OCCUPIED` 或 `UNOCCUPIED` |
| 连续 60 帧未检测到有效人体热区 | 强制回到 `UNOCCUPIED` |

如果你的处理频率为 5 Hz：

```text
连续 5 帧 ≈ 1 秒
连续 30 帧 ≈ 6 秒
连续 60 帧 ≈ 12 秒
```

这个节奏适合占用检测。它不会对瞬时噪声过度敏感，也不会在人刚离开时立刻误判无人。

---

## 14. 与 SHT35 / SCD41 的辅助融合

SHT35 和 SCD41 的温湿度数据不建议直接作为人体判断阈值，但可以用于环境状态修正。

### 14.1 可用方式

| 传感器 | 可辅助内容 |
|---|---|
| SHT35 | 提供空气温度、湿度，用于判断环境变化趋势 |
| SCD41 | 提供 CO₂ 浓度变化，可辅助验证空间内是否长期有人 |
| MLX90640 | 提供热分布，是占用率判断核心 |

### 14.2 简单融合策略

如果 MLX90640 判断为有人，同时 CO₂ 在一段时间内上升，则可以提高置信度：

```text
热区存在 + CO₂ 上升趋势明显
    ↓
占用状态置信度提高
```

如果热图显示疑似有人，但 CO₂ 长期无变化，并且热区位置固定不动，则可能是静态热源，例如电脑、暖气片或阳光照射区域。

### 14.3 不建议的方式

不建议直接使用：

```cpp
if (mlx_pixel_temp > sht35_air_temp + 3.0f) {
    occupied = true;
}
```

原因是 MLX90640 测的是表面温度，SHT35 测的是空气温度，两者物理对象不同。可以比较趋势，但不宜简单相减后作为唯一判据。

---

## 15. 推荐代码组织结构

建议将算法封装成独立模块，避免和传感器读取、LED 显示、网页通信混在一起。

```text
src/
├── main.cpp
├── sensors/
│   ├── mlx90640_driver.cpp
│   ├── mlx90640_driver.h
│   ├── sht35_driver.cpp
│   ├── sht35_driver.h
│   ├── scd41_driver.cpp
│   └── scd41_driver.h
├── occupancy/
│   ├── occupancy_detector.cpp
│   └── occupancy_detector.h
├── display/
│   ├── hub75_display.cpp
│   └── hub75_display.h
├── comm/
│   ├── serial_protocol.cpp
│   └── serial_protocol.h
└── config/
    └── app_config.h
```

### 15.1 `occupancy_detector.h` 建议接口

```cpp
#pragma once
#include <stdint.h>

#define MLX_WIDTH 32
#define MLX_HEIGHT 24
#define MLX_PIXELS 768

enum OccupancyState {
    UNOCCUPIED = 0,
    POSSIBLE_OCCUPIED = 1,
    OCCUPIED = 2
};

struct OccupancyResult {
    float occupancy_ratio;
    float occupancy_score;
    float threshold;
    float max_delta;
    int valid_pixels;
    int max_region_area;
    int bins[5];
    OccupancyState state;
};

class OccupancyDetector {
public:
    void begin(const float* first_frame);
    OccupancyResult update(const float* frame);

private:
    float background[MLX_PIXELS];
    float smooth[MLX_PIXELS];
    float delta[MLX_PIXELS];
    uint8_t mask[MLX_PIXELS];

    bool initialized = false;
    int occupied_count = 0;
    int empty_count = 0;
    OccupancyState state = UNOCCUPIED;

    float computeDynamicThreshold();
    void buildMask(float threshold);
    void removeIsolatedNoise();
    void analyzeRegions(OccupancyResult& result);
    void updateBackground(float freeze_threshold);
};
```

---

## 16. 占用率算法伪代码

```cpp
OccupancyResult OccupancyDetector::update(const float* frame) {
    OccupancyResult result = {};

    // 1. 初始化
    if (!initialized) {
        begin(frame);
        result.state = UNOCCUPIED;
        return result;
    }

    // 2. 时间滤波
    for (int i = 0; i < MLX_PIXELS; i++) {
        smooth[i] = beta * frame[i] + (1.0f - beta) * smooth[i];
    }

    // 3. 计算温差
    for (int i = 0; i < MLX_PIXELS; i++) {
        delta[i] = smooth[i] - background[i];
    }

    // 4. 动态阈值
    float threshold = computeDynamicThreshold();
    result.threshold = threshold;

    // 5. 生成候选热区
    buildMask(threshold);

    // 6. 去除孤立噪声
    removeIsolatedNoise();

    // 7. 连通区域分析与占用率计算
    analyzeRegions(result);

    // 8. 状态机更新
    bool detected = (result.max_region_area >= 4 &&
                     result.max_delta >= 2.0f &&
                     result.occupancy_score > 0.01f);

    if (detected) {
        occupied_count++;
        empty_count = 0;
    } else {
        empty_count++;
        occupied_count = 0;
    }

    if (occupied_count >= 5) {
        state = OCCUPIED;
    } else if (occupied_count >= 3) {
        state = POSSIBLE_OCCUPIED;
    }

    if (empty_count >= 60) {
        state = UNOCCUPIED;
    } else if (empty_count >= 30 && state == OCCUPIED) {
        state = POSSIBLE_OCCUPIED;
    }

    result.state = state;

    // 9. 更新背景，疑似人体区域冻结
    float freeze_threshold = max(1.0f, threshold * 0.7f);
    updateBackground(freeze_threshold);

    return result;
}
```

---

## 17. 参数建议表

| 参数 | 初始值 | 可调范围 | 调大后的效果 | 调小后的效果 |
|---|---:|---:|---|---|
| `beta` | 0.5 | 0.3~0.7 | 响应更快，噪声更明显 | 更平滑，响应更慢 |
| `alpha` | 0.005 | 0.002~0.02 | 背景适应更快 | 更不容易把人学成背景 |
| `min_delta_threshold` | 1.5 ℃ | 1.0~2.5 ℃ | 误检减少，漏检增加 | 更灵敏，误检增加 |
| `k_std` | 2.5 | 2.0~3.5 | 更保守 | 更灵敏 |
| `min_region_area` | 4 | 3~12 | 排除小热源 | 更容易检测远处人体 |
| `min_region_max_delta` | 2.0 ℃ | 1.5~4.0 ℃ | 排除弱热源 | 提高灵敏度 |
| `occupied_confirm_frames` | 5 | 3~10 | 状态更稳定 | 响应更快 |
| `empty_confirm_frames` | 60 | 20~120 | 离开后保持更久 | 更快判定无人 |

---

## 18. 调试输出建议

第一版固件不要只输出最终“有人/无人”，建议通过串口或网页输出中间值，方便判断算法哪里出了问题。

建议每秒输出一次：

```text
state=OCCUPIED
occupancy_ratio=0.052
occupancy_score=0.031
threshold=1.64
max_delta=5.28
valid_pixels=42
max_region_area=38
bins=[650, 72, 32, 10, 4]
```

如果出现误判，可以根据这些信息判断原因：

| 现象 | 可能原因 | 调整方向 |
|---|---|---|
| 无人时也经常有人 | 阈值太低、背景没初始化好、固定热源干扰 | 提高 `min_delta_threshold` 或增加区域面积阈值 |
| 人在远处检测不到 | 面积阈值太高、阈值太保守 | 降低 `min_region_area` 或 `k_std` |
| 人站久后消失 | 背景更新太快，把人学成背景 | 降低 `alpha`，人体区域冻结背景 |
| 输出状态跳变 | 连续帧机制太短 | 增加确认帧数 |
| 阳光照射导致误判 | 大面积固定热源 | 加入热区位置稳定性判断或提高阈值 |

---

## 19. 推荐实现顺序

建议不要一次性把所有功能写完，而是按可验证的小步骤推进：

```text
步骤 A：只读取 MLX90640，并串口打印 768 个温度值中的 min / max / mean
步骤 B：显示 32×24 简单热图，确认手靠近时热斑位置正确
步骤 C：初始化 background[768]，输出 delta 的 min / max / mean
步骤 D：加入动态阈值，输出 mask 中的候选像素数量
步骤 E：加入孤立点过滤，观察误检是否减少
步骤 F：加入连通区域分析，输出最大热区面积和最大温差
步骤 G：加入占用率计算，输出 occupancy_ratio 和 occupancy_score
步骤 H：加入状态机，输出无人 / 疑似有人 / 有人
步骤 I：再融合 SHT35 / SCD41 数据，提升长期判断可靠性
```

---

## 20. 最终推荐算法版本

第一版可采用以下组合，既容易实现，也足够适合 ESP32-S3 运行：

```text
MLX90640 温度帧
    ↓
指数滑动平均
    ↓
像素级背景建模
    ↓
delta = 当前温度 - 背景温度
    ↓
threshold = max(1.5 ℃, mean(delta) + 2.5 × std(delta))
    ↓
delta > threshold 得到候选热区
    ↓
去除孤立点
    ↓
连通区域分析
    ↓
area / mean_delta / max_delta 筛选人体热区
    ↓
occupancy_score = 有效热区加权得分 / 768
    ↓
连续帧状态机输出无人 / 疑似有人 / 有人
```

