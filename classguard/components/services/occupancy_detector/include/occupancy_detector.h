#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "data_types.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float background[CG_MLX90640_PIXEL_COUNT];
    float smooth[CG_MLX90640_PIXEL_COUNT];
    float delta[CG_MLX90640_PIXEL_COUNT];
    float sort_values[CG_MLX90640_PIXEL_COUNT];
    uint8_t mask[CG_MLX90640_PIXEL_COUNT];
    uint8_t scratch_mask[CG_MLX90640_PIXEL_COUNT];
    uint8_t outlier_mask[CG_MLX90640_PIXEL_COUNT];
    uint8_t valid_human_mask[CG_MLX90640_PIXEL_COUNT];
    uint8_t visited[CG_MLX90640_PIXEL_COUNT];
    uint16_t queue[CG_MLX90640_PIXEL_COUNT];
    uint16_t candidate_indices[CG_MLX90640_PIXEL_COUNT];
    uint16_t valid_indices[CG_MLX90640_PIXEL_COUNT];
    uint16_t init_frames;
    uint16_t candidate_count;
    uint16_t valid_candidate_count;
    uint16_t outlier_count;
    uint16_t occupied_count;
    uint16_t suspected_count;
    uint16_t empty_count;
    float background_temp;
    float interference_threshold;
    float human_ref_temp;
    float final_threshold;
    cg_occupancy_state_t state;
    bool initialized;
} cg_occupancy_detector_t;

void cg_occupancy_detector_init(cg_occupancy_detector_t *detector);
esp_err_t cg_occupancy_detector_update(cg_occupancy_detector_t *detector,
                                       const thermal_frame_t *frame,
                                       occupancy_frame_t *result);

#ifdef __cplusplus
}
#endif
