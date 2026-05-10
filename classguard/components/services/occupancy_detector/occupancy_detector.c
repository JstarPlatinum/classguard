#include "occupancy_detector.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "app_config.h"

static float clampf(float value, float min_value, float max_value)
{
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static float maxf(float a, float b)
{
    return a > b ? a : b;
}

static int compare_float_ascending(const void *left, const void *right)
{
    float a = *(const float *)left;
    float b = *(const float *)right;
    if (a < b) {
        return -1;
    }
    if (a > b) {
        return 1;
    }
    return 0;
}

static int compare_float_descending(const void *left, const void *right)
{
    return compare_float_ascending(right, left);
}

static void fill_default_result(const cg_occupancy_detector_t *detector,
                                const thermal_frame_t *frame,
                                occupancy_frame_t *result)
{
    memset(result, 0, sizeof(*result));
    result->timestamp_ms = frame->timestamp_ms;
    result->state = detector->state;
    result->occupied = detector->state == CG_OCCUPANCY_OCCUPIED;
    result->background_temp = detector->background_temp;
    result->interference_threshold = detector->interference_threshold;
    result->human_ref_temp = detector->human_ref_temp;
    result->final_threshold = detector->final_threshold;
    result->threshold = detector->final_threshold;
    result->candidate_count = detector->candidate_count;
    result->outlier_count = detector->outlier_count;
}

void cg_occupancy_detector_init(cg_occupancy_detector_t *detector)
{
    if (detector == NULL) {
        return;
    }

    memset(detector, 0, sizeof(*detector));
    detector->state = CG_OCCUPANCY_UNOCCUPIED;
}

static float mean_values(const float *values, uint16_t start, uint16_t end)
{
    if (end <= start) {
        return 0.0f;
    }

    float sum = 0.0f;
    for (uint16_t i = start; i < end; ++i) {
        sum += values[i];
    }
    return sum / (float)(end - start);
}

static void finalize_background_init(cg_occupancy_detector_t *detector)
{
    for (uint16_t i = 0; i < CG_MLX90640_PIXEL_COUNT; ++i) {
        detector->background[i] /= (float)detector->init_frames;
        detector->smooth[i] = detector->background[i];
        detector->sort_values[i] = detector->background[i];
    }

    qsort(detector->sort_values,
          CG_MLX90640_PIXEL_COUNT,
          sizeof(detector->sort_values[0]),
          compare_float_ascending);

    uint16_t background_count = (uint16_t)((float)CG_MLX90640_PIXEL_COUNT * CG_OCCUPANCY_BACKGROUND_PERCENT);
    if (background_count == 0) {
        background_count = 1;
    }
    if (background_count >= CG_MLX90640_PIXEL_COUNT) {
        background_count = CG_MLX90640_PIXEL_COUNT - 1U;
    }

    detector->background_temp = mean_values(detector->sort_values, 0, background_count);
    float interference_raw = mean_values(detector->sort_values, background_count, CG_MLX90640_PIXEL_COUNT);
    detector->interference_threshold = clampf(interference_raw,
                                              detector->background_temp + CG_OCCUPANCY_INTERFERENCE_MIN_DELTA,
                                              detector->background_temp + CG_OCCUPANCY_INTERFERENCE_MAX_DELTA);
    detector->human_ref_temp = detector->interference_threshold;
    detector->final_threshold = detector->interference_threshold;
    detector->initialized = true;
}

static void accumulate_background_init(cg_occupancy_detector_t *detector,
                                       const thermal_frame_t *frame,
                                       occupancy_frame_t *result)
{
    if (detector->init_frames == 0) {
        memset(detector->background, 0, sizeof(detector->background));
    }

    for (uint16_t i = 0; i < CG_MLX90640_PIXEL_COUNT; ++i) {
        detector->background[i] += frame->pixels[i];
    }
    detector->init_frames++;

    if (detector->init_frames >= CG_OCCUPANCY_BACKGROUND_INIT_FRAMES) {
        finalize_background_init(detector);
        result->background_temp = detector->background_temp;
        result->interference_threshold = detector->interference_threshold;
        result->human_ref_temp = detector->human_ref_temp;
        result->final_threshold = detector->final_threshold;
        result->threshold = detector->final_threshold;
        result->valid = true;
    }
}

static uint8_t neighbor_count(const uint8_t *mask, int x, int y)
{
    uint8_t count = 0;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) {
                continue;
            }

            int nx = x + dx;
            int ny = y + dy;
            if (nx < 0 || nx >= (int)CG_MLX90640_WIDTH || ny < 0 || ny >= (int)CG_MLX90640_HEIGHT) {
                continue;
            }

            if (mask[ny * CG_MLX90640_WIDTH + nx] != 0) {
                count++;
            }
        }
    }
    return count;
}

static uint8_t hot_support_count(const cg_occupancy_detector_t *detector, uint16_t index)
{
    uint8_t count = 0;
    int x = index % CG_MLX90640_WIDTH;
    int y = index / CG_MLX90640_WIDTH;

    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) {
                continue;
            }

            int nx = x + dx;
            int ny = y + dy;
            if (nx < 0 || nx >= (int)CG_MLX90640_WIDTH || ny < 0 || ny >= (int)CG_MLX90640_HEIGHT) {
                continue;
            }

            uint16_t neighbor = (uint16_t)(ny * CG_MLX90640_WIDTH + nx);
            if (detector->outlier_mask[neighbor] == 0 &&
                detector->smooth[neighbor] > detector->interference_threshold) {
                count++;
            }
        }
    }
    return count;
}

static void collect_candidates(cg_occupancy_detector_t *detector)
{
    detector->candidate_count = 0;
    memset(detector->outlier_mask, 0, sizeof(detector->outlier_mask));
    detector->outlier_count = 0;

    for (uint16_t i = 0; i < CG_MLX90640_PIXEL_COUNT; ++i) {
        if (detector->smooth[i] > detector->interference_threshold) {
            detector->candidate_indices[detector->candidate_count++] = i;
        }
    }
}

static void remove_extreme_outliers(cg_occupancy_detector_t *detector)
{
    for (uint8_t iter = 0; iter < CG_OCCUPANCY_MAX_OUTLIER_REMOVE_ITER; ++iter) {
        float max_temp = -INFINITY;
        for (uint16_t i = 0; i < detector->candidate_count; ++i) {
            uint16_t index = detector->candidate_indices[i];
            if (detector->outlier_mask[index] == 0 && detector->smooth[index] > max_temp) {
                max_temp = detector->smooth[index];
            }
        }

        if (max_temp <= CG_OCCUPANCY_HUMAN_UPPER_TEMP_C) {
            break;
        }

        float peak_threshold = max_temp - CG_OCCUPANCY_PEAK_DELTA;
        uint16_t peak_count = 0;
        bool has_spatial_support = false;
        for (uint16_t i = 0; i < detector->candidate_count; ++i) {
            uint16_t index = detector->candidate_indices[i];
            if (detector->outlier_mask[index] != 0 || detector->smooth[index] < peak_threshold) {
                continue;
            }

            detector->queue[peak_count++] = index;
            if (hot_support_count(detector, index) >= CG_OCCUPANCY_MIN_SUPPORT_PIXELS) {
                has_spatial_support = true;
            }
        }

        if (peak_count == 0) {
            break;
        }

        if (peak_count <= CG_OCCUPANCY_OUTLIER_COUNT_LIMIT && !has_spatial_support) {
            for (uint16_t i = 0; i < peak_count; ++i) {
                uint16_t index = detector->queue[i];
                if (detector->outlier_mask[index] == 0) {
                    detector->outlier_mask[index] = 1;
                    detector->outlier_count++;
                }
            }
            continue;
        }

        break;
    }
}

static bool build_valid_candidates(cg_occupancy_detector_t *detector)
{
    detector->valid_candidate_count = 0;
    for (uint16_t i = 0; i < detector->candidate_count; ++i) {
        uint16_t index = detector->candidate_indices[i];
        if (detector->outlier_mask[index] != 0) {
            continue;
        }

        detector->valid_indices[detector->valid_candidate_count] = index;
        detector->sort_values[detector->valid_candidate_count] = detector->smooth[index];
        detector->valid_candidate_count++;
    }

    return detector->valid_candidate_count >= CG_OCCUPANCY_MIN_CANDIDATE_COUNT;
}

static float mean_sorted_range(const float *values, uint16_t start, uint16_t end)
{
    if (end <= start) {
        return values[start];
    }
    return mean_values(values, start, end);
}

static void compute_human_threshold(cg_occupancy_detector_t *detector)
{
    qsort(detector->sort_values,
          detector->valid_candidate_count,
          sizeof(detector->sort_values[0]),
          compare_float_descending);

    uint16_t n = detector->valid_candidate_count;
    uint16_t n10 = (uint16_t)((float)n * 0.10f);
    uint16_t n20 = (uint16_t)((float)n * 0.20f);
    uint16_t n30 = (uint16_t)((float)n * 0.30f);

    if (n10 < 1U) {
        n10 = 1U;
    }
    if (n20 <= n10) {
        n20 = n10 + 1U;
    }
    if (n30 <= n20) {
        n30 = n20 + 1U;
    }
    if (n30 > n) {
        n30 = n;
    }
    if (n20 >= n30 && n30 > 1U) {
        n20 = n30 - 1U;
    }
    if (n10 >= n20 && n20 > 1U) {
        n10 = n20 - 1U;
    }

    float mean_top10 = mean_sorted_range(detector->sort_values, 0, n10);
    float mean_10_20 = mean_sorted_range(detector->sort_values, n10, n20);
    float mean_20_30 = mean_sorted_range(detector->sort_values, n20, n30);

    detector->human_ref_temp = (6.5f * mean_top10 + 2.5f * mean_10_20 + mean_20_30) / 10.0f;
    detector->human_ref_temp = maxf(detector->human_ref_temp, detector->interference_threshold);

    detector->final_threshold = detector->interference_threshold +
                                CG_OCCUPANCY_HUMAN_THRESHOLD_BETA *
                                    (detector->human_ref_temp - detector->interference_threshold);
    detector->final_threshold = clampf(detector->final_threshold,
                                       detector->interference_threshold,
                                       detector->human_ref_temp);
}

static void remove_isolated_noise(cg_occupancy_detector_t *detector)
{
    memcpy(detector->scratch_mask, detector->mask, sizeof(detector->scratch_mask));

    for (int y = 0; y < (int)CG_MLX90640_HEIGHT; ++y) {
        for (int x = 0; x < (int)CG_MLX90640_WIDTH; ++x) {
            size_t index = (size_t)y * CG_MLX90640_WIDTH + (size_t)x;
            if (detector->scratch_mask[index] == 0) {
                continue;
            }
            if (neighbor_count(detector->scratch_mask, x, y) < 1) {
                detector->mask[index] = 0;
            }
        }
    }
}

static void fill_temp_bins(const cg_occupancy_detector_t *detector, occupancy_frame_t *result)
{
    memset(result->bins, 0, sizeof(result->bins));
    for (uint16_t i = 0; i < CG_MLX90640_PIXEL_COUNT; ++i) {
        float temp = detector->smooth[i];
        if (temp < detector->interference_threshold) {
            result->bins[0]++;
        } else if (temp < detector->final_threshold) {
            result->bins[1]++;
        } else if (temp < detector->human_ref_temp) {
            result->bins[2]++;
        } else if (temp < CG_OCCUPANCY_HUMAN_UPPER_TEMP_C) {
            result->bins[3]++;
        } else {
            result->bins[4]++;
        }
    }
}

static void analyze_regions(cg_occupancy_detector_t *detector, occupancy_frame_t *result)
{
    memset(detector->visited, 0, sizeof(detector->visited));
    memset(detector->valid_human_mask, 0, sizeof(detector->valid_human_mask));

    uint16_t valid_pixels = 0;
    uint16_t max_region_area = 0;
    float max_region_temp = -INFINITY;

    for (uint16_t start = 0; start < CG_MLX90640_PIXEL_COUNT; ++start) {
        if (detector->mask[start] == 0 || detector->visited[start] != 0) {
            continue;
        }

        uint16_t head = 0;
        uint16_t tail = 0;
        uint16_t area = 0;
        float region_max_temp = -INFINITY;
        float region_temp_sum = 0.0f;

        detector->queue[tail++] = start;
        detector->visited[start] = 1;

        while (head < tail) {
            uint16_t index = detector->queue[head++];
            area++;
            float temp = detector->smooth[index];
            region_temp_sum += temp;
            if (temp > region_max_temp) {
                region_max_temp = temp;
            }

            int x = index % CG_MLX90640_WIDTH;
            int y = index / CG_MLX90640_WIDTH;
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    if (dx == 0 && dy == 0) {
                        continue;
                    }

                    int nx = x + dx;
                    int ny = y + dy;
                    if (nx < 0 || nx >= (int)CG_MLX90640_WIDTH || ny < 0 || ny >= (int)CG_MLX90640_HEIGHT) {
                        continue;
                    }

                    uint16_t next = (uint16_t)(ny * CG_MLX90640_WIDTH + nx);
                    if (detector->mask[next] != 0 && detector->visited[next] == 0) {
                        detector->visited[next] = 1;
                        detector->queue[tail++] = next;
                    }
                }
            }
        }

        float mean_temp = region_temp_sum / (float)area;
        bool valid_region = area >= CG_OCCUPANCY_MIN_REGION_AREA &&
                            mean_temp >= detector->final_threshold &&
                            region_max_temp >= detector->final_threshold + CG_OCCUPANCY_MIN_COMPONENT_MAX_DELTA;
        if (!valid_region) {
            continue;
        }

        valid_pixels += area;
        if (area > max_region_area) {
            max_region_area = area;
        }
        if (region_max_temp > max_region_temp) {
            max_region_temp = region_max_temp;
        }

        for (uint16_t i = 0; i < tail; ++i) {
            detector->valid_human_mask[detector->queue[i]] = 1;
        }
    }

    float denom = maxf(detector->human_ref_temp - detector->final_threshold, 0.5f);
    float heat_sum = 0.0f;
    for (uint16_t i = 0; i < CG_MLX90640_PIXEL_COUNT; ++i) {
        if (detector->valid_human_mask[i] == 0) {
            continue;
        }
        heat_sum += clampf((detector->smooth[i] - detector->final_threshold) / denom, 0.0f, 1.0f);
    }

    result->valid_pixels = valid_pixels;
    result->max_region_area = max_region_area;
    result->max_delta = max_region_temp > -INFINITY ? max_region_temp - detector->background_temp : 0.0f;
    result->occupancy_ratio = (float)valid_pixels / (float)CG_MLX90640_PIXEL_COUNT;
    result->occupancy_heat_score = heat_sum / (float)CG_MLX90640_PIXEL_COUNT;
    result->occupancy_score = 0.8f * result->occupancy_ratio + 0.2f * result->occupancy_heat_score;
}

static void update_state(cg_occupancy_detector_t *detector, occupancy_frame_t *result)
{
    bool occupied_frame = result->max_region_area >= CG_OCCUPANCY_MIN_REGION_AREA &&
                          result->occupancy_score >= CG_OCCUPANCY_OCCUPIED_THRESHOLD;
    bool suspected_frame = detector->candidate_count >= CG_OCCUPANCY_MIN_CANDIDATE_COUNT &&
                           result->occupancy_score >= CG_OCCUPANCY_SUSPECTED_THRESHOLD;

    if (occupied_frame) {
        detector->occupied_count++;
        detector->suspected_count = 0;
        detector->empty_count = 0;
    } else if (suspected_frame) {
        detector->occupied_count = 0;
        detector->suspected_count++;
        detector->empty_count = 0;
    } else {
        detector->occupied_count = 0;
        detector->suspected_count = 0;
        detector->empty_count++;
    }

    if (detector->occupied_count >= CG_OCCUPANCY_OCCUPIED_CONFIRM_FRAMES) {
        detector->state = CG_OCCUPANCY_OCCUPIED;
    } else if (detector->suspected_count >= CG_OCCUPANCY_POSSIBLE_CONFIRM_FRAMES) {
        detector->state = CG_OCCUPANCY_POSSIBLE_OCCUPIED;
    }

    if (detector->empty_count >= CG_OCCUPANCY_EMPTY_CONFIRM_FRAMES) {
        detector->state = CG_OCCUPANCY_UNOCCUPIED;
    } else if (detector->empty_count >= CG_OCCUPANCY_EMPTY_POSSIBLE_FRAMES &&
               detector->state == CG_OCCUPANCY_OCCUPIED) {
        detector->state = CG_OCCUPANCY_POSSIBLE_OCCUPIED;
    }

    result->state = detector->state;
    result->occupied = detector->state == CG_OCCUPANCY_OCCUPIED;
}

static void update_background(cg_occupancy_detector_t *detector)
{
    for (uint16_t i = 0; i < CG_MLX90640_PIXEL_COUNT; ++i) {
        if (detector->smooth[i] > detector->interference_threshold) {
            continue;
        }
        detector->background[i] = (1.0f - CG_OCCUPANCY_ALPHA) * detector->background[i] +
                                  CG_OCCUPANCY_ALPHA * detector->smooth[i];
    }
}

static void make_empty_runtime_result(cg_occupancy_detector_t *detector,
                                      const thermal_frame_t *frame,
                                      occupancy_frame_t *result)
{
    detector->human_ref_temp = detector->interference_threshold;
    detector->final_threshold = detector->interference_threshold;
    fill_default_result(detector, frame, result);
    fill_temp_bins(detector, result);
    update_state(detector, result);
    result->timestamp_ms = frame->timestamp_ms;
    result->valid = true;
}

esp_err_t cg_occupancy_detector_update(cg_occupancy_detector_t *detector,
                                       const thermal_frame_t *frame,
                                       occupancy_frame_t *result)
{
    if (detector == NULL || frame == NULL || result == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!frame->valid) {
        return ESP_ERR_INVALID_STATE;
    }

    fill_default_result(detector, frame, result);
    if (!detector->initialized) {
        accumulate_background_init(detector, frame, result);
        return ESP_OK;
    }

    for (uint16_t i = 0; i < CG_MLX90640_PIXEL_COUNT; ++i) {
        detector->smooth[i] = (1.0f - CG_OCCUPANCY_SMOOTH_ALPHA) * detector->smooth[i] +
                              CG_OCCUPANCY_SMOOTH_ALPHA * frame->pixels[i];
        detector->delta[i] = detector->smooth[i] - detector->background[i];
    }

    collect_candidates(detector);
    if (detector->candidate_count < CG_OCCUPANCY_MIN_CANDIDATE_COUNT) {
        make_empty_runtime_result(detector, frame, result);
        update_background(detector);
        return ESP_OK;
    }

    remove_extreme_outliers(detector);
    if (!build_valid_candidates(detector)) {
        make_empty_runtime_result(detector, frame, result);
        update_background(detector);
        return ESP_OK;
    }

    compute_human_threshold(detector);

    for (uint16_t i = 0; i < CG_MLX90640_PIXEL_COUNT; ++i) {
        detector->mask[i] = detector->smooth[i] > detector->final_threshold &&
                            detector->outlier_mask[i] == 0
                                ? 1U
                                : 0U;
    }

    remove_isolated_noise(detector);
    fill_default_result(detector, frame, result);
    fill_temp_bins(detector, result);
    analyze_regions(detector, result);
    update_state(detector, result);
    update_background(detector);

    result->timestamp_ms = frame->timestamp_ms;
    result->valid = true;
    return ESP_OK;
}
