# ClassGuard Formal Sensor Telemetry v0.1

This document records the data names and types used by the formal SHT35,
SCD41 and PMS5003 integration. Use these names when adding LED display,
rules, storage or later MLX90640 processing code.

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
      "frame_rate": 0.0,
      "temp_min_c": 24.0,
      "temp_max_c": 24.0,
      "temp_avg_c": 24.0
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
MLX90640 is intentionally fixed in this version and is not driven together with
SHT35, SCD41 and PMS5003.
