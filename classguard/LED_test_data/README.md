# LED Test Data

This folder provides fixed ESP32-side sensor data for LED matrix display development.

Use `#include "led_test_data.h"` and call:

```c
led_test_data_frame_t data;
led_test_data_get_all(&data);
```

The mock data uses the same structs and field names as the real drivers:

- SHT35: `sht35_timestamp_ms`, `sht35_temperature_c`, `sht35_humidity_rh`, `sht35_valid`
- SCD41: `scd41_timestamp_ms`, `scd41_co2_ppm`, `scd41_temperature_c`, `scd41_humidity_rh`, `scd41_valid`
- PMS5003: `pm_frame_t` fields such as `pm2_5_atm`, `pm10_atm`, `particles_0_3um`, `valid`

When replacing this with real data later, keep the display layer reading from these same field names.
