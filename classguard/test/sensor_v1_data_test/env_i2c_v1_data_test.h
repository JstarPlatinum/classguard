/*
 * Environment I2C v1 data interface:
 *   env_i2c_v1_data_test_get_sht35(...) copies the latest SHT35 frame.
 *   env_i2c_v1_data_test_get_scd41(...) copies the latest SCD41 frame.
 *   The frame data is updated by env_i2c_v1_data_test_run(); no serial text
 *   output is produced here.
 */
#pragma once

#include <stdbool.h>
#include "data_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void env_i2c_v1_data_test_run(void);
bool env_i2c_v1_data_test_get_sht35(environment_frame_t *out_frame);
bool env_i2c_v1_data_test_get_scd41(environment_frame_t *out_frame);

#ifdef __cplusplus
}
#endif
