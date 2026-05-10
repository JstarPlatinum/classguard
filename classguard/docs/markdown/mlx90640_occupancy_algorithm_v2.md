# MLX90640 占用率判断算法流程 V2

> 适用对象：ESP32-S3 + MLX90640 热阵列 + SHT35/SCD41 环境传感器 + LED 点阵显示系统  
> 核心目标：在不同季节、不同室内温度、存在小面积高温异常点以及人群密集热块的情况下，稳定估计人体占用状态与占用率。

---

## 1. 设计思路概述

V2 算法不再依赖固定温度阈值，也不直接使用标准差、MAD 等离散程度来决定人体阈值。原因在于：当画面中出现大面积连续人体热块时，整体温度分布会被抬高，离散程度阈值可能随之增大，导致真实人体区域被漏检。另一方面，热水杯、反光点、坏点或局部高温设备可能只占少数像素，却会显著拉高 top 温度统计，因此需要先处理极端异常点。

本算法采用如下策略：

```text
无人初始化背景 → 估计背景温度与环境干扰温度
        ↓
实时帧平滑 → 候选热源筛选
        ↓
极端高温异常点剔除
        ↓
基于 top 10% / 10%~20% / 20%~30% 温度段估计人体代表温度
        ↓
由干扰阈值与人体代表温度共同生成最终检测阈值
        ↓
二值化、连通区域分析、持续帧判断
        ↓
输出无人 / 疑似有人 / 有人 / 占用率
```

---

## 2. 数据定义

MLX90640 每帧输出 `32 × 24 = 768` 个温度值，记为：

```text
T[i], i = 0, 1, 2, ..., 767
```

其中 `T[i]` 是第 `i` 个像素对应视场区域的温度，单位为摄氏度 ℃。

算法中使用的主要变量如下：

| 变量名 | 含义 |
|---|---|
| `T_raw[768]` | MLX90640 当前原始温度帧 |
| `T_smooth[768]` | 时间平滑、必要时空间平滑后的温度帧 |
| `Init[768]` | 初始化阶段 45 帧融合得到的无人背景温度图 |
| `background_temp` | 初始化无人状态下的背景温度估计值 |
| `interference_raw` | 初始化无人状态下的环境干扰温度原始估计值 |
| `interference_threshold` | 经过上下限约束后的环境干扰阈值 |
| `candidate_pixels` | 高于干扰阈值的候选热源像素集合 |
| `outlier_mask[768]` | 极端异常点掩膜 |
| `human_ref_temp` | 当前画面中人体热源代表温度 |
| `final_threshold` | 用于人体候选区域二值化的最终检测阈值 |
| `mask[768]` | 人体候选区域二值图 |
| `occupancy_area_ratio` | 面积占用率 |
| `occupancy_heat_score` | 热强度加权占用率 |
| `occupancy_score` | 综合占用率输出 |

---

## 3. 初始化阶段：无人背景与干扰阈值估计

### 3.1 初始化条件

系统启动后，需要确保 MLX90640 视场中处于“完全无人”的理想状态。初始化阶段建议让设备先预热并丢弃前若干帧，待热阵列输出稳定后再开始采集。

推荐流程：

```text
系统上电
    ↓
等待 MLX90640 稳定，例如 1~3 s
    ↓
丢弃前 5~10 帧
    ↓
采集 45 帧无人热图
    ↓
对 45 帧进行时间平滑，得到 Init[768]
```

### 3.2 时间平滑

初始化阶段可对 45 帧做逐像素平均：

```text
Init[i] = (T_1[i] + T_2[i] + ... + T_45[i]) / 45
```

如果实时性要求更高，也可以用指数滑动平均：

```text
Init_new[i] = (1 - α_init) × Init_old[i] + α_init × T_current[i]
```

初始化阶段推荐：

```text
α_init = 0.05 ~ 0.15
```

对于固定 45 帧初始化，直接平均更直观，也更便于调试。

### 3.3 背景温度估计

将 `Init[768]` 中的温度值按从低到高排序：

```text
Init_sorted = sort_ascending(Init)
```

取最低 70% 像素作为背景噪声区域：

```text
background_pixels = Init_sorted[0 : 70%]
background_temp = mean(background_pixels)
```

这样做的假设是：无人状态下，大部分视场属于普通背景；即便有少量墙面反光、设备外壳或局部热区，也会主要落在较高的 30% 区间，不会直接污染背景估计。

### 3.4 环境干扰温度估计

取平滑后最高 30% 温度作为可能的环境干扰区域：

```text
interference_pixels = Init_sorted[70% : 100%]
interference_raw = mean(interference_pixels)
```

由于初始化阶段可能存在固定热源或局部反光点，`interference_raw` 不能无限抬高，需要进行上下限约束。

建议使用：

```text
interference_threshold = clamp(
    interference_raw,
    background_temp + 0.8,
    background_temp + 2.5
)
```

其中：

| 约束 | 作用 |
|---|---|
| `background_temp + 0.8 ℃` | 防止环境过于均匀时阈值过低，引发噪声误判 |
| `background_temp + 2.5 ℃` | 防止初始化阶段局部固定热源把干扰阈值抬得过高 |

> 注意：这里更建议使用 `background_temp + 2.5 ℃`，而不是直接使用 SHT35/SCD41 的空气温度 + 3 ℃。MLX90640 看到的是物体表面温度，SHT35/SCD41 测的是空气温度，二者物理意义不同。空气温度可以作为辅助校验，但不宜直接作为热图阈值核心。

---

## 4. 实时检测阶段总流程

实时检测阶段每获得一帧新的 MLX90640 温度矩阵，就执行一次占用率判断。考虑到 ESP32-S3 还需要驱动 LED 点阵、解析 PMS5003、读取 SHT35/SCD41，推荐占用率检测频率设为最高 `5~10 Hz`

```text
读取当前温度帧 T_raw[768]
        ↓
时间平滑 / 轻微空间平滑
        ↓
根据 interference_threshold 筛选候选热源像素
        ↓
极端高温异常点剔除
        ↓
排序并计算 top 10% / 10%~20% / 20%~30% 均值
        ↓
计算 human_ref_temp
        ↓
计算 final_threshold
        ↓
生成人体候选 mask
        ↓
连通区域分析
        ↓
持续帧状态机
        ↓
计算并输出 occupancy_score
```

---

## 5. 实时温度平滑

### 5.1 时间平滑

为降低 MLX90640 的帧间抖动，建议对每个像素做指数滑动平均：

```text
T_smooth[i] = (1 - α) × T_smooth_prev[i] + α × T_raw[i]
```

推荐参数：

```text
α = 0.25 ~ 0.40
```

如果希望响应更快，取 `0.40`；如果希望显示和判断更稳定，取 `0.25~0.30`。

### 5.2 空间平滑（可选）

可选用 3×3 轻微空间平滑，但不建议过度平滑，否则人体边缘会被扩散，影响占用率估计。

推荐方式：

```text
T_spatial[x, y] = 0.5 × T[x, y] + 0.5 × mean(neighbor_8)
```

也可以在调试早期先不做空间平滑，只使用时间平滑，避免算法复杂度过高。

---

## 6. 候选热源筛选

实时帧经过平滑后，先筛选高于干扰阈值的像素：

```text
candidate_pixels = { T_smooth[i] | T_smooth[i] > interference_threshold }
```

同时保留这些像素的索引位置：

```text
candidate_indices = { i | T_smooth[i] > interference_threshold }
```

如果候选像素数量过少，说明当前热源证据不足：

```text
if candidate_count < min_candidate_count:
    输出无人或低置信度
```

推荐初值：

```text
min_candidate_count = 5 ~ 8 
```

---

## 7. 极端高温异常点剔除

### 7.1 为什么需要异常点剔除

如果画面中存在热水杯、局部反光、坏点或高温电子元件，它们可能只占极少数像素，但温度远高于人体热图中的合理范围。这些点会抬高 top 10% / 10%~20% / 20%~30% 的统计结果，导致人体代表温度偏高，最终阈值偏高，造成真实人体区域漏检。

### 7.2 不建议使用摄氏度百分比法

推荐使用固定温差窗口：

```text
peak_band_threshold = Tmax - Δpeak
```

推荐：

```text
Δpeak = 1.0 ~ 1.5 ℃
```

### 7.3 异常点判断条件

只有同时满足以下条件的最高温邻域，才建议视为异常：

```text
1. Tmax > human_upper_temp
2. T >= Tmax - Δpeak 的像素数量 ≤ outlier_count_limit
3. 这些高温点周围缺少足够的中温热区支撑
```

推荐参数：

| 参数 | 建议初值 | 含义 |
|---|---:|---|
| `human_upper_temp` | 36.0 ℃ | 人体热图最高合理温度上限，可根据实测调整 |
| `Δpeak` | 1.2 ℃ | 最高温邻域窗口 |
| `outlier_count_limit` | 3 | 极端高温点数量上限 |
| `min_support_pixels` | 4 | 周围热区支撑像素数量下限 |
| `max_outlier_remove_iter` | 3 | 单帧最多迭代剔除次数 |

### 7.4 空间邻域支撑判断

对于每个高温异常候选点，统计其 8 邻域中满足以下条件的像素数量：

```text
T_smooth[j] > interference_threshold
```

如果周围支持像素很少，说明它更像孤立异常点；如果周围有连续热区，则可能是真实人体热块中的高温核心，不应轻易删除。

判断建议：

```text
if peak_count <= 3 and support_pixels < 4:
    视为极端异常点
else:
    保留
```

### 7.5 迭代剔除流程

```text
outlier_mask 全部初始化为 0

repeat 最多 max_outlier_remove_iter 次:
    从未被剔除的 candidate_pixels 中找到 Tmax

    if Tmax <= human_upper_temp:
        break

    peak_band_threshold = Tmax - Δpeak
    peak_pixels = { i | T_smooth[i] >= peak_band_threshold 且 outlier_mask[i] == 0 }

    if count(peak_pixels) <= outlier_count_limit 且缺少空间邻域支撑:
        将 peak_pixels 标记为异常点 outlier_mask[i] = 1
    else:
        break
```

异常点不建议从原始热图中真正删除，而应通过 `outlier_mask` 在后续统计中排除。这样可以保留完整热图用于调试和显示。

---

## 8. top 10% / 20% / 30% 人体代表温度估计

### 8.1 有效候选像素集合

极端异常点剔除后，用以下条件构建有效候选集合：

```text
valid_candidates = {
    T_smooth[i] |
    T_smooth[i] > interference_threshold
    and outlier_mask[i] == 0
}
```

如果有效候选像素太少：

```text
if valid_candidate_count < min_valid_candidate_count:
    输出无人或疑似无人
```

推荐：

```text
min_valid_candidate_count = 5 ~ 8
```

### 8.2 按温度降序排序

```text
valid_sorted = sort_descending(valid_candidates)
```

### 8.3 分段均值计算

将有效候选像素按温度从高到低分为三个区间：

```text
mean_top10  = 平均最高 0%~10% 区间
mean_10_20  = 平均 10%~20% 区间
mean_20_30  = 平均 20%~30% 区间
```

如果候选像素数量较少，应保证每段至少有 2 个像素 ， 否则视为无人。实际实现中可以使用整数索引保护：

```text
n = valid_candidate_count
n10 = max(1, floor(n × 0.10))
n20 = max(n10 + 1, floor(n × 0.20))
n30 = max(n20 + 1, floor(n × 0.30))
```

### 8.4 加权计算人体代表温度

按 `6.5 : 2.5 : 1` 加权：

```text
human_ref_temp = (
    6.5 × mean_top10 +
    2.5 × mean_10_20 +
    1.0 × mean_20_30
) / 10.0
```

并加入下限保护：

```text
human_ref_temp = max(human_ref_temp, interference_threshold)
```

注意：`human_ref_temp` 表示当前画面中“人体热源代表温度”或“高置信人体温度”，不建议直接作为最终二值化阈值。若直接用它做阈值，可能只保留人体最热核心区域，导致占用率偏低。

---

## 9. 最终检测阈值生成

推荐使用干扰阈值与人体代表温度之间的插值作为最终阈值：

```text
final_threshold = interference_threshold + β × (human_ref_temp - interference_threshold)
```

推荐参数：

```text
β = 0.30 ~ 0.40
```

| β 取值 | 效果 |
|---:|---|
| 0.25~0.30 | 检测更敏感，能保留较多人体边缘区域，但误检略高 |
| 0.35~0.40 | 更稳健，能减少环境干扰误判 |
| 0.45 以上 | 阈值偏高，可能低估占用率 |

建议初始使用：

```text
β = 0.37
```

并加入约束：

```text
final_threshold = max(final_threshold, interference_threshold)
final_threshold = min(final_threshold, human_ref_temp)
```

---

## 10. 二值化与连通区域分析

### 10.1 生成人体候选 mask

```text
mask[i] = 1, if T_smooth[i] > final_threshold and outlier_mask[i] == 0
mask[i] = 0, otherwise
```

### 10.2 连通区域分析

对 `32 × 24` 的 mask 做 4 邻域或 8 邻域连通区域分析。人体热块一般具有连续区域特征，孤立点不应直接判定为有人。

每个连通区域统计：

| 指标 | 含义 |
|---|---|
| `component_area` | 区域像素数 |
| `component_max_temp` | 区域最高温 |
| `component_mean_temp` | 区域平均温 |
| `component_bbox` | 区域外接矩形 |
| `component_centroid` | 区域中心位置 |

有效人体区域建议满足：

```text
component_area >= min_human_area
component_mean_temp >= final_threshold
component_max_temp >= final_threshold + 0.5
```

推荐初值：

| 参数 | 建议初值 |
|---|---:|
| `min_human_area` | 3~7 像素 |
| `min_component_max_delta` | 0.5 ℃ |

如果安装距离较远，人体只占少量像素，`min_human_area` 可以取 3；如果安装距离较近、人体区域较大，则可取 6~10。

---

## 11. 占用率计算

建议同时计算面积占用率和热强度加权占用率，再融合输出。

### 11.1 面积占用率

```text
occupancy_area_ratio = valid_human_mask_pixel_count / 768
```

其中 `valid_human_mask_pixel_count` 是属于有效人体连通区域的像素数量，而不是所有超过阈值的孤立像素数量。

### 11.2 热强度加权占用率

对每个有效人体像素计算热强度得分：

```text
score[i] = clamp(
    (T_smooth[i] - final_threshold) / (human_ref_temp - final_threshold),
    0,
    1
)
```

为了避免分母过小，需要加入保护：

```text
denominator = max(human_ref_temp - final_threshold, 0.5)
```

热强度占用率：

```text
occupancy_heat_score = sum(score[i]) / 768
```

### 11.3 综合占用率

推荐融合：

```text
occupancy_score = 0.8 × occupancy_area_ratio + 0.2 × occupancy_heat_score
```

如果你的应用更关注“区域面积”，可以提高面积权重；如果更关注“热源强度”，可以提高热强度权重。

---

## 12. 状态机：无人 / 疑似有人 / 有人

单帧判断容易受到噪声影响，建议引入持续帧状态机。

### 12.1 单帧判定

```text
if 有效人体连通区域存在 且 occupancy_score >= occupied_threshold:
    frame_state = OCCUPIED
elif candidate_count 足够但连通区域不足:
    frame_state = SUSPECTED
else:
    frame_state = EMPTY
```

推荐初始阈值：

| 参数 | 建议初值 |
|---|---:|
| `occupied_threshold` | 0.02~0.05 |
| `suspected_threshold` | 0.01~0.02 |

具体数值应根据安装高度、视场范围和目标人数进行标定。

### 12.2 多帧状态机

```text
连续 3~5 帧检测到 OCCUPIED → 输出有人
连续 2~3 帧检测到 SUSPECTED → 输出疑似有人
连续 20~50 帧未检测到有效热区 → 输出无人
```

如果检测频率为 `10 Hz`：

| 状态转换 | 建议持续时间 |
|---|---:|
| 无人 → 有人 | 0.3~0.5 s |
| 有人 → 无人 | 2~5 s |

人体进入时应快速响应，人体离开后可以适当延迟，避免抖动。

---

## 13. SHT35 / SCD41 的辅助作用

SHT35/SCD41 的温度不直接参与 MLX90640 的主阈值判断，但可以用于辅助校验：

```text
if abs(background_temp - air_temp) > 5.0 ℃:
    提示安装场景可能存在特殊背景，例如阳光、热设备、冷墙面
```

也可以在长期运行中用于判断是否需要重新初始化：

```text
if air_temp 相比初始化时变化超过 5 ℃:
    建议触发重新标定或缓慢更新 background_temp / interference_threshold
```

但若重新标定无法保证无人状态，不建议自动覆盖初始化背景。

---

## 14. 推荐参数表

| 参数 | 推荐初值 | 可调范围 | 说明 |
|---|---:|---:|---|
| `init_frames` | 45 | 30~60 | 无人初始化帧数 |
| `background_percent` | 70% | 60%~80% | 初始化背景像素比例 |
| `interference_percent` | 30% | 20%~40% | 初始化干扰像素比例 |
| `interference_min_delta` | 0.8 ℃ | 0.5~1.2 ℃ | 干扰阈值下限 |
| `interference_max_delta` | 2.5 ℃ | 2.0~4.0 ℃ | 干扰阈值上限 |
| `smooth_alpha` | 0.30 | 0.25~0.40 | 实时温度时间平滑系数 |
| `human_upper_temp` | 36.0 ℃ | 35.5~37.0 ℃ | 人体热图最高合理温度上限 |
| `Δpeak` | 1.2 ℃ | 1.0~1.5 ℃ | 极端高温邻域窗口 |
| `outlier_count_limit` | 3 | 2~4 | 极端高温点数量上限 |
| `min_support_pixels` | 4 | 3~6 | 空间邻域支撑像素数 |
| `max_outlier_remove_iter` | 3 | 2~5 | 单帧异常剔除最大迭代次数 |
| `β` | 0.37 | 0.30~0.40 | 最终阈值插值比例 |
| `min_candidate_count` | 6 | 5~10 | 最少候选热源像素数 |
| `min_human_area` | 4 | 4~10 | 最小人体连通区域面积 |
| `occupied_threshold` | 0.03 | 0.02~0.05 | 有人占用率阈值 |
| `empty_hold_frames` | 20 | 20~50 | 有人变无人的延迟帧数 |

---

## 15. ESP32-S3 上的实现建议

### 15.1 数据类型

MLX90640 温度值建议使用 `float` 保存：

```cpp
float tempRaw[768];
float tempSmooth[768];
float initTemp[768];
uint8_t mask[768];
uint8_t outlierMask[768];
```

`ESP32-S3` 具备单精度浮点运算能力，处理 768 个像素的浮点计算是可行的。真正的耗时通常来自 MLX90640 的 I2C 读取和温度解算，而不是占用率算法本身。

### 15.2 排序优化

V2 初期可以直接对候选像素排序，代码简单，方便验证。若后期需要进一步优化，可改为温度直方图法：

```text
20~40 ℃ 按 0.1 ℃ 分箱 → 约 200 个 bin
```

这样可避免频繁浮点排序。

### 15.3 任务频率建议

| 任务 | 推荐频率 |
|---|---:|
| MLX90640 采集 | 5 Hz |
| 占用率算法 | 跟随 MLX90640 新帧执行 |
| LED 点阵刷新 | 独立高频任务 / DMA 驱动 |
| SHT35 | 1 Hz |
| SCD41 | 0.2 Hz，即约 5 秒一次 |
| PMS5003 | 0.2 Hz 左右解析一次 |

---

## 16. 模块化代码组织建议

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
│   ├── occupancy_config.h
│   ├── occupancy_init.cpp
│   ├── occupancy_init.h
│   ├── occupancy_filter.cpp
│   ├── occupancy_filter.h
│   ├── occupancy_outlier.cpp
│   ├── occupancy_outlier.h
│   ├── occupancy_detector.cpp
│   └── occupancy_detector.h
├── display/
│   ├── led_matrix.cpp
│   └── led_matrix.h
└── utils/
    ├── ring_buffer.h
    └── debug_tools.h
```

建议将算法配置集中写入 `occupancy_config.h`：

```cpp
#pragma once

#define MLX_PIXELS 768
#define INIT_FRAMES 45

#define BACKGROUND_PERCENT 0.70f
#define INTERFERENCE_MIN_DELTA 0.8f
#define INTERFERENCE_MAX_DELTA 3.0f

#define SMOOTH_ALPHA 0.30f

#define HUMAN_UPPER_TEMP 36.0f
#define PEAK_DELTA 1.2f
#define OUTLIER_COUNT_LIMIT 3
#define MIN_SUPPORT_PIXELS 4
#define MAX_OUTLIER_REMOVE_ITER 3

#define HUMAN_THRESHOLD_BETA 0.35f
#define MIN_CANDIDATE_COUNT 6
#define MIN_HUMAN_AREA 6

#define OCCUPIED_THRESHOLD 0.03f
#define SUSPECTED_THRESHOLD 0.015f
#define OCCUPIED_HOLD_FRAMES 4
#define EMPTY_HOLD_FRAMES 30
```

---

## 17. 核心伪代码

```cpp
struct OccupancyResult {
    float backgroundTemp;
    float interferenceThreshold;
    float humanRefTemp;
    float finalThreshold;
    float occupancyAreaRatio;
    float occupancyHeatScore;
    float occupancyScore;
    int validHumanPixels;
    int largestComponentArea;
    int state; // 0 empty, 1 suspected, 2 occupied
};
```

### 17.1 初始化伪代码

```cpp
void initBackground(float frames[INIT_FRAMES][768]) {
    float initTemp[768];

    for (int i = 0; i < 768; i++) {
        float sum = 0;
        for (int f = 0; f < INIT_FRAMES; f++) {
            sum += frames[f][i];
        }
        initTemp[i] = sum / INIT_FRAMES;
    }

    sortAscending(initTemp, 768);

    int bgCount = (int)(768 * 0.70f);
    float bgSum = 0;
    for (int i = 0; i < bgCount; i++) {
        bgSum += initTemp[i];
    }
    backgroundTemp = bgSum / bgCount;

    float intSum = 0;
    int intCount = 768 - bgCount;
    for (int i = bgCount; i < 768; i++) {
        intSum += initTemp[i];
    }
    float interferenceRaw = intSum / intCount;

    interferenceThreshold = clamp(
        interferenceRaw,
        backgroundTemp + 0.8f,
        backgroundTemp + 3.0f
    );
}
```

### 17.2 实时检测伪代码

```cpp
OccupancyResult detectOccupancy(float tempRaw[768]) {
    // 1. 时间平滑
    for (int i = 0; i < 768; i++) {
        tempSmooth[i] = (1.0f - SMOOTH_ALPHA) * tempSmooth[i]
                      + SMOOTH_ALPHA * tempRaw[i];
    }

    // 2. 候选像素筛选
    Candidate candidates[768];
    int n = 0;
    for (int i = 0; i < 768; i++) {
        outlierMask[i] = 0;
        if (tempSmooth[i] > interferenceThreshold) {
            candidates[n++] = {i, tempSmooth[i]};
        }
    }

    if (n < MIN_CANDIDATE_COUNT) {
        return makeEmptyResult();
    }

    // 3. 极端异常点剔除
    removeExtremeOutliers(tempSmooth, outlierMask, interferenceThreshold);

    // 4. 构建有效候选集合
    Candidate valid[768];
    int m = 0;
    for (int k = 0; k < n; k++) {
        int idx = candidates[k].index;
        if (!outlierMask[idx]) {
            valid[m++] = candidates[k];
        }
    }

    if (m < MIN_CANDIDATE_COUNT) {
        return makeEmptyResult();
    }

    // 5. 降序排序并计算 top 区间
    sortDescending(valid, m);

    float meanTop10 = meanRange(valid, 0.00f, 0.10f, m);
    float mean10_20 = meanRange(valid, 0.10f, 0.20f, m);
    float mean20_30 = meanRange(valid, 0.20f, 0.30f, m);

    float humanRefTemp = (
        6.5f * meanTop10 +
        2.5f * mean10_20 +
        1.0f * mean20_30
    ) / 10.0f;

    humanRefTemp = max(humanRefTemp, interferenceThreshold);

    // 6. 生成最终检测阈值
    float finalThreshold = interferenceThreshold
                         + HUMAN_THRESHOLD_BETA * (humanRefTemp - interferenceThreshold);

    finalThreshold = clamp(finalThreshold, interferenceThreshold, humanRefTemp);

    // 7. 二值化
    for (int i = 0; i < 768; i++) {
        mask[i] = (tempSmooth[i] > finalThreshold && !outlierMask[i]) ? 1 : 0;
    }

    // 8. 连通区域分析
    ComponentStats stats = connectedComponentAnalysis(mask, tempSmooth, finalThreshold);

    // 9. 占用率计算
    float areaRatio = stats.validHumanPixels / 768.0f;

    float heatSum = 0;
    float denom = max(humanRefTemp - finalThreshold, 0.5f);
    for (int i = 0; i < 768; i++) {
        if (stats.validHumanMask[i]) {
            float s = (tempSmooth[i] - finalThreshold) / denom;
            heatSum += clamp(s, 0.0f, 1.0f);
        }
    }

    float heatScore = heatSum / 768.0f;
    float occupancyScore = 0.6f * areaRatio + 0.4f * heatScore;

    // 10. 状态机
    int state = updateOccupancyState(occupancyScore, stats);

    return {
        backgroundTemp,
        interferenceThreshold,
        humanRefTemp,
        finalThreshold,
        areaRatio,
        heatScore,
        occupancyScore,
        stats.validHumanPixels,
        stats.largestComponentArea,
        state
    };
}
```

---

## 18. 调试输出建议

初期调试时不要只输出最终占用率，建议同时输出关键中间变量：

```text
background_temp
interference_threshold
candidate_count
outlier_count
human_ref_temp
final_threshold
largest_component_area
occupancy_area_ratio
occupancy_heat_score
occupancy_score
state
```

串口输出示例：

```text
BG=24.6, INT=26.1, CAND=84, OUT=1, HREF=31.4, TH=27.9, AREA=48, OCC=0.071, STATE=OCCUPIED
```

通过这些变量可以判断：

| 现象 | 可能原因 |
|---|---|
| `interference_threshold` 过高 | 初始化无人阶段存在固定热源或阳光干扰 |
| `outlier_count` 经常很高 | 画面中存在高温物体、反光点或坏点 |
| `human_ref_temp` 很高但 `AREA` 很小 | 小型高温热源误入统计 |
| `final_threshold` 过高 | `β` 偏大，或 top 温度被异常点拉高 |
| `occupancy_score` 抖动 | 时间平滑不足或状态机保持帧数太短 |

---

## 19. 算法优势与边界

### 19.1 优势

- 避免固定温度阈值在季节变化下失效。
- 通过无人初始化建立场景专属背景与干扰阈值。
- 在人群密集、大面积连续热块下，不依赖全局离散程度设阈值。
- 使用 top 10% / 20% / 30% 加权估计人体代表温度，能更贴近当前画面中的人体热源强度。
- 加入极端异常点剔除，减少小面积高温点对阈值的污染。
- 通过连通区域与持续帧状态机抑制孤立噪声和瞬时误判。

### 19.2 边界情况

以下场景仍需要额外处理或现场标定：

- 画面中存在大面积加热器、暖气片、阳光直射区域。
- 高温物体面积较大，无法通过“少数极端点”规则剔除。
- 人体穿着厚衣物，表面温度接近背景。
- MLX90640 安装角度导致人体只占极少像素。
- 初始化无人阶段并非真正无人，导致背景和干扰阈值被污染。

对于固定热源长期存在的场景，可进一步增加“固定位置热源屏蔽图”，在多次无人初始化或人工标定后，将固定热源区域从人体检测中排除。

---

## 20. 推荐实验验证方法

建议按以下场景逐步测试：

| 测试场景 | 目的 |
|---|---|
| 无人、普通室温 | 验证背景和干扰阈值是否合理 |
| 单人远距离进入 | 验证小面积人体热区是否能检出 |
| 单人近距离进入 | 验证大面积人体热块占用率是否合理 |
| 多人同时进入 | 验证人群密集场景下阈值是否不会异常升高 |
| 热水杯 / 小型热源进入 | 验证极端异常点剔除是否有效 |
| 固定热源长期存在 | 验证是否需要固定热源屏蔽图 |
| 空调或室温变化 | 验证不同环境温度下稳定性 |

每组测试建议记录：

```text
原始热图
平滑热图
异常点 mask
人体候选 mask
final_threshold
occupancy_score
实际人数 / 实际占用状态
```

这些数据后续可以用于参数标定，也可以作为项目答辩或技术文档中的验证依据。
