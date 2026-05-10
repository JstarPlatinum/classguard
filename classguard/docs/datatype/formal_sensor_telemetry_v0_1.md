# ClassGuard Formal Sensor Telemetry v0.1

This document records the data names and types used by the formal SHT35,
SCD41, PMS5003 and MLX90640 occupancy integration. Use these names when
adding LED display, rules, storage or later heatmap transfer code.

共享数据接口：app_data.h (line 26)

后续 LED 显示可以直接调用：

```c
uint32_t sensor_error_mask;
cg_app_sensor_snapshot_t snapshot;
cg_app_data_get_latest(&snapshot);
```

## ESP32 Shared Data Interface

Header: `main/app_data.h`

Function:

```c
bool cg_app_data_get_latest(cg_app_sensor_snapshot_t *out_snapshot);
```

Type:

```c
typedef struct {
    environment_frame_t environment;
    pm_frame_t pm;
    thermal_frame_t thermal;
    occupancy_frame_t occupancy;
    uint32_t sensor_error_mask;
    char error_message[192];
} cg_app_sensor_snapshot_t;
```

## Environment Data

Source type: `environment_frame_t` in `components/common/include/data_types.h`

| Field | C type | Unit | Source | Meaning |
|---|---:|---|---|---|
| `scd41_timestamp_ms` | `uint32_t` | ms | SCD41 | SCD41 sample timestamp |
| `scd41_co2_ppm` | `uint16_t` | ppm | SCD41 | CO2 concentration |
| `scd41_temperature_c` | `float` | deg C | SCD41 | SCD41 internal temperature reading |
| `scd41_humidity_rh` | `float` | %RH | SCD41 | SCD41 humidity reading |
| `scd41_valid` | `bool` | boolean | SCD41 | Whether SCD41 fields are valid |
| `sht35_timestamp_ms` | `uint32_t` | ms | SHT35 | SHT35 sample timestamp |
| `sht35_temperature_c` | `float` | deg C | SHT35 | Main ambient temperature for display |
| `sht35_humidity_rh` | `float` | %RH | SHT35 | Main ambient humidity for display |
| `sht35_valid` | `bool` | boolean | SHT35 | Whether SHT35 fields are valid |

## PMS5003 Data

Source type: `pm_frame_t` in `components/common/include/data_types.h`

| Field | C type | Unit | Meaning |
|---|---:|---|---|
| `timestamp_ms` | `uint32_t` | ms | PMS5003 frame timestamp |
| `pm1_0_atm` | `uint16_t` | ug/m3 | PM1.0 atmospheric concentration |
| `pm2_5_atm` | `uint16_t` | ug/m3 | PM2.5 atmospheric concentration |
| `pm10_atm` | `uint16_t` | ug/m3 | PM10 atmospheric concentration |
| `pm1_0_cf1` | `uint16_t` | ug/m3 | PM1.0 CF=1 concentration |
| `pm2_5_cf1` | `uint16_t` | ug/m3 | PM2.5 CF=1 concentration |
| `pm10_cf1` | `uint16_t` | ug/m3 | PM10 CF=1 concentration |
| `particles_0_3um` | `uint16_t` | count/0.1L | Particle count larger than 0.3 um |
| `particles_0_5um` | `uint16_t` | count/0.1L | Particle count larger than 0.5 um |
| `particles_1_0um` | `uint16_t` | count/0.1L | Particle count larger than 1.0 um |
| `particles_2_5um` | `uint16_t` | count/0.1L | Particle count larger than 2.5 um |
| `particles_5_0um` | `uint16_t` | count/0.1L | Particle count larger than 5.0 um |
| `particles_10um` | `uint16_t` | count/0.1L | Particle count larger than 10 um |
| `frames_read` | `uint32_t` | count | Valid PMS5003 frames since boot |
| `checksum_errors` | `uint32_t` | count | PMS5003 checksum errors since boot |
| `valid` | `bool` | boolean | Whether PMS5003 fields are valid |

## MLX90640 Thermal Data

Source type: `thermal_frame_t` in `components/common/include/data_types.h`

| Field | C type | Unit | Meaning |
|---|---:|---|---|
| `timestamp_ms` | `uint32_t` | ms | MLX90640 frame timestamp |
| `pixels[768]` | `float[]` | deg C | Full 32x24 thermal frame kept in ESP32 shared memory |
| `min_temp_c` | `float` | deg C | Minimum temperature in the current frame |
| `max_temp_c` | `float` | deg C | Maximum temperature in the current frame |
| `avg_temp_c` | `float` | deg C | Average temperature in the current frame |
| `hotspot_index` | `uint16_t` | index | Hottest pixel index |
| `valid` | `bool` | boolean | Whether MLX90640 thermal fields are valid |

## MLX90640 Occupancy Data

Source type: `occupancy_frame_t` in `components/common/include/data_types.h`

| Field | C type | Unit | Meaning |
|---|---:|---|---|
| `timestamp_ms` | `uint32_t` | ms | Occupancy result timestamp |
| `occupancy_ratio` | `float` | ratio | Valid human-like hot area ratio |
| `occupancy_heat_score` | `float` | score | Heat intensity score |
| `occupancy_score` | `float` | score | Combined occupancy score |
| `background_temp` | `float` | deg C | Estimated background temperature |
| `interference_threshold` | `float` | deg C | Interference filtering threshold |
| `human_ref_temp` | `float` | deg C | Human reference temperature |
| `final_threshold` | `float` | deg C | Final detection threshold |
| `max_delta` | `float` | deg C | Maximum delta above background |
| `candidate_count` | `uint16_t` | count | Candidate hot pixels before filtering |
| `outlier_count` | `uint16_t` | count | Extreme outlier pixels |
| `valid_pixels` | `uint16_t` | count | Valid human-like pixels |
| `max_region_area` | `uint16_t` | count | Largest connected region area |
| `bins[5]` | `uint16_t[]` | count | Delta temperature bins |
| `state` | `cg_occupancy_state_t` | enum | 0 unoccupied, 1 possible, 2 occupied |
| `occupied` | `bool` | boolean | Final occupied flag |
| `valid` | `bool` | boolean | Whether occupancy fields are valid |

## Status Data

| Field | C type | Meaning |
|---|---:|---|
| `sensor_error_mask` | `uint32_t` | Bitmask of current sensor errors |
| `error_message` | `char[192]` | Semicolon-separated sensor error summary for Web display |

Error bits:

| Bit name | Value | Meaning |
|---|---:|---|
| `CG_SENSOR_STATUS_SHT35` | `0x01` | SHT35 init/read error |
| `CG_SENSOR_STATUS_SCD41` | `0x02` | SCD41 init/start/read error |
| `CG_SENSOR_STATUS_PMS5003` | `0x04` | PMS5003 init/reset/read error |
| `CG_SENSOR_STATUS_MLX90640` | `0x08` | MLX90640 init/read or occupancy error |

## HTTP Telemetry JSON

POST target: `/api/telemetry`

```json
{
  "device_id": "esp32s3_node_001",
  "firmware": "classguard-formal-v0.1.0",
  "timestamp": 0,
  "uptime_ms": 123456,
  "wifi": {
    "rssi": -45,
    "ip": "192.168.0.10"
  },
  "sensors": {
    "scd41": {
      "co2_ppm": 620,
      "temperature_c": 25.10,
      "humidity_percent": 50.20
    },
    "sht35": {
      "temperature_c": 24.80,
      "humidity_percent": 51.00
    },
    "pms5003": {
      "pm1_0": 8,
      "pm2_5": 12,
      "pm10": 18
    },
    "mlx90640": {
      "temp_min_c": 24.0,
      "temp_max_c": 32.5,
      "temp_avg_c": 26.2,
      "occupied": true,
      "occupancy_ratio": 0.043,
      "occupancy_heat_score": 0.020,
      "occupancy_score": 0.038,
      "state": 2,
      "max_delta": 5.4,
      "valid_pixels": 33,
      "max_region_area": 18
    }
  },
  "status": {
    "sensor_ok": true,
    "error_code": 0,
    "error_message": ""
  }
}
```

If a sensor has not produced valid data, its numeric fields are sent as `null`.
MLX90640 is now driven by `main/thermal_service.c`. This formal service updates
both `thermal` and `occupancy` in the shared snapshot. It does not upload the
full 32x24 heatmap yet; only thermal statistics and occupancy summary fields are
included in `/api/telemetry`.
