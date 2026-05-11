#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "data_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CG_AQ_MAX_REDLINES 8U
#define CG_AQ_MAX_REASONS 3U
#define CG_AQ_TEXT_LEN 32U
#define CG_AQ_LABEL_LEN 48U
#define CG_AQ_ACTION_LEN 32U

typedef struct {
    bool valid;
    float co2_ppm;
    float pm1_0_ugm3;
    float pm2_5_ugm3;
    float pm10_ugm3;
    float temperature_c;
    float humidity_rh;
    float occupancy_ratio;
} cg_air_quality_input_t;

typedef struct {
    bool has_co2;
    bool has_pm1_0;
    bool has_pm2_5;
    bool has_pm10;
    bool has_temperature;
    bool has_humidity;
    bool has_occupancy;
} cg_air_quality_validity_t;

typedef struct {
    float co2;
    float pm1_0;
    float pm2_5;
    float pm10;
    float temperature;
    float humidity;
    float occupancy;
} cg_air_quality_metric_values_t;

typedef struct {
    float weighted_score;
    float display_score;
    char level[CG_AQ_LABEL_LEN];
    char action[CG_AQ_ACTION_LEN];
    char message[CG_AQ_TEXT_LEN];

    cg_air_quality_validity_t valid;
    cg_air_quality_metric_values_t scores;
    cg_air_quality_metric_values_t weights;

    size_t redline_count;
    char redlines[CG_AQ_MAX_REDLINES][CG_AQ_LABEL_LEN];
    size_t main_reason_count;
    char main_reasons[CG_AQ_MAX_REASONS][CG_AQ_LABEL_LEN];
} cg_air_quality_result_t;

void cg_air_quality_input_init(cg_air_quality_input_t *out_input);

void cg_air_quality_input_from_frames(const environment_frame_t *environment,
                                      const pm_frame_t *pm,
                                      const occupancy_frame_t *occupancy,
                                      cg_air_quality_input_t *out_input);

void cg_air_quality_evaluate(const cg_air_quality_input_t *input,
                             cg_air_quality_result_t *out_result);

#ifdef __cplusplus
}
#endif
