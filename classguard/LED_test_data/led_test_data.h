/*
 * LED display fixed test data interface:
 *   led_test_data_get_sht35(...) fills the same SHT35 fields written by the
 *   real SHT35 driver: sht35_timestamp_ms, sht35_temperature_c,
 *   sht35_humidity_rh, sht35_valid.
 *   led_test_data_get_scd41(...) fills the same SCD41 fields written by the
 *   real SCD41 driver: scd41_timestamp_ms, scd41_co2_ppm,
 *   scd41_temperature_c, scd41_humidity_rh, scd41_valid.
 *   led_test_data_get_pms5003(...) fills pm_frame_t with the same field names
 *   used by cg_pms5003_read_frame(...).
 */
#pragma once

#include "data_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    environment_frame_t environment;
    pm_frame_t pms5003;
} led_test_data_frame_t;

void led_test_data_get_sht35(environment_frame_t *out_frame);
void led_test_data_get_scd41(environment_frame_t *out_frame);
void led_test_data_get_environment(environment_frame_t *out_frame);
void led_test_data_get_pms5003(pm_frame_t *out_frame);
void led_test_data_get_all(led_test_data_frame_t *out_frame);

#ifdef __cplusplus
}
#endif
