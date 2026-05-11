#pragma once

#include "air_quality_evaluator.h"
#include "app_data.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Fixed sensor data for display/debug flows.
 *
 * The returned data uses the same names, structs and C types as the real
 * sensor pipeline, so display code can keep using cg_app_data_get_latest().
 */
void cg_fixed_sensor_data_make_snapshot(cg_app_sensor_snapshot_t *out_snapshot);
void cg_fixed_sensor_data_load_to_app_data(void);
void cg_fixed_sensor_data_evaluate_air_quality(cg_air_quality_result_t *out_result);

#ifdef __cplusplus
}
#endif
