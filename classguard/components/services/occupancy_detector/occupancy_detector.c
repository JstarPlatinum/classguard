#include "occupancy_detector.h"

#include <math.h>
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

static void fill_default_result(const cg_occupancy_detector_t *detector, const thermal_frame_t *frame, occupancy_frame_t *result)
{
    memset(result, 0, sizeof(*result));
    result->timestamp_ms = frame->timestamp_ms;
    result->state = detector->state;
    result->occupied = detector->state == CG_OCCUPANCY_OCCUPIED;
}

void cg_occupancy_detector_init(cg_occupancy_detector_t *detector)
{
    if (detector == NULL) {
        return;
    }

    memset(detector, 0, sizeof(*detector));
    detector->state = CG_OCCUPANCY_UNOCCUPIED;
}

static void accumulate_background_init(cg_occupancy_detector_t *detector, const thermal_frame_t *frame, occupancy_frame_t *result)
{
    if (detector->init_frames == 0) {
        memset(detector->background, 0, sizeof(detector->background));
    }

    for (uint16_t i = 0; i < CG_MLX90640_PIXEL_COUNT; ++i) {
        detector->background[i] += frame->pixels[i];
    }
    detector->init_frames++;

    if (detector->init_frames >= CG_OCCUPANCY_BACKGROUND_INIT_FRAMES) {
        for (uint16_t i = 0; i < CG_MLX90640_PIXEL_COUNT; ++i) {
            detector->background[i] /= (float)detector->init_frames;
            detector->smooth[i] = detector->background[i];
        }
        detector->initialized = true;
        result->valid = true;
    }
}

static float compute_threshold(cg_occupancy_detector_t *detector)
{
    float sum = 0.0f;
    for (uint16_t i = 0; i < CG_MLX90640_PIXEL_COUNT; ++i) {
        sum += detector->delta[i];
    }

    float mean = sum / (float)CG_MLX90640_PIXEL_COUNT;
    float variance_sum = 0.0f;
    for (uint16_t i = 0; i < CG_MLX90640_PIXEL_COUNT; ++i) {
        float diff = detector->delta[i] - mean;
        variance_sum += diff * diff;
    }

    float stddev = sqrtf(variance_sum / (float)CG_MLX90640_PIXEL_COUNT);
    float threshold = mean + CG_OCCUPANCY_STD_K * stddev;
    return threshold > CG_OCCUPANCY_MIN_DELTA_THRESHOLD ? threshold : CG_OCCUPANCY_MIN_DELTA_THRESHOLD;
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

static void fill_delta_bins(const cg_occupancy_detector_t *detector, occupancy_frame_t *result)
{
    memset(result->bins, 0, sizeof(result->bins));
    for (uint16_t i = 0; i < CG_MLX90640_PIXEL_COUNT; ++i) {
        float delta = detector->delta[i];
        if (delta < 1.0f) {
            result->bins[0]++;
        } else if (delta < 2.0f) {
            result->bins[1]++;
        } else if (delta < 4.0f) {
            result->bins[2]++;
        } else if (delta < 6.0f) {
            result->bins[3]++;
        } else {
            result->bins[4]++;
        }
    }
}

static void analyze_regions(cg_occupancy_detector_t *detector, occupancy_frame_t *result)
{
    memset(detector->visited, 0, sizeof(detector->visited));

    float score_sum = 0.0f;
    uint16_t valid_pixels = 0;
    uint16_t max_region_area = 0;
    float max_region_delta = 0.0f;

    for (uint16_t start = 0; start < CG_MLX90640_PIXEL_COUNT; ++start) {
        if (detector->mask[start] == 0 || detector->visited[start] != 0) {
            continue;
        }

        uint16_t head = 0;
        uint16_t tail = 0;
        uint16_t area = 0;
        float region_max_delta = -INFINITY;
        float region_delta_sum = 0.0f;

        detector->queue[tail++] = start;
        detector->visited[start] = 1;

        while (head < tail) {
            uint16_t index = detector->queue[head++];
            area++;
            float delta = detector->delta[index];
            region_delta_sum += delta;
            if (delta > region_max_delta) {
                region_max_delta = delta;
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

        float mean_delta = region_delta_sum / (float)area;
        bool valid_region = area >= CG_OCCUPANCY_MIN_REGION_AREA &&
                            region_max_delta >= CG_OCCUPANCY_MIN_REGION_MAX_DELTA &&
                            mean_delta >= CG_OCCUPANCY_MIN_DELTA_THRESHOLD;
        if (!valid_region) {
            continue;
        }

        valid_pixels += area;
        if (area > max_region_area) {
            max_region_area = area;
        }
        if (region_max_delta > max_region_delta) {
            max_region_delta = region_max_delta;
        }

        for (uint16_t i = 0; i < tail; ++i) {
            uint16_t index = detector->queue[i];
            score_sum += clampf((detector->delta[index] - 1.0f) / 4.0f, 0.0f, 1.0f);
        }
    }

    result->valid_pixels = valid_pixels;
    result->max_region_area = max_region_area;
    result->max_delta = max_region_delta;
    result->occupancy_ratio = (float)valid_pixels / (float)CG_MLX90640_PIXEL_COUNT;
    result->occupancy_score = score_sum / (float)CG_MLX90640_PIXEL_COUNT;
}

static void update_state(cg_occupancy_detector_t *detector, occupancy_frame_t *result)
{
    bool detected = result->max_region_area >= CG_OCCUPANCY_MIN_REGION_AREA &&
                    result->max_delta >= CG_OCCUPANCY_MIN_REGION_MAX_DELTA &&
                    result->occupancy_ratio > 0.0f;

    if (detected) {
        detector->occupied_count++;
        detector->empty_count = 0;
    } else {
        detector->empty_count++;
        detector->occupied_count = 0;
    }

    if (detector->occupied_count >= CG_OCCUPANCY_OCCUPIED_CONFIRM_FRAMES) {
        detector->state = CG_OCCUPANCY_OCCUPIED;
    } else if (detector->occupied_count >= CG_OCCUPANCY_POSSIBLE_CONFIRM_FRAMES) {
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

static void update_background(cg_occupancy_detector_t *detector, float threshold)
{
    float freeze_threshold = threshold * 0.7f;
    if (freeze_threshold < 1.0f) {
        freeze_threshold = 1.0f;
    }

    for (uint16_t i = 0; i < CG_MLX90640_PIXEL_COUNT; ++i) {
        if (detector->delta[i] > freeze_threshold) {
            continue;
        }
        detector->background[i] = (1.0f - CG_OCCUPANCY_ALPHA) * detector->background[i] +
                                  CG_OCCUPANCY_ALPHA * detector->smooth[i];
    }
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
        detector->smooth[i] = CG_OCCUPANCY_BETA * frame->pixels[i] +
                              (1.0f - CG_OCCUPANCY_BETA) * detector->smooth[i];
        detector->delta[i] = detector->smooth[i] - detector->background[i];
    }

    float threshold = compute_threshold(detector);
    result->threshold = threshold;

    for (uint16_t i = 0; i < CG_MLX90640_PIXEL_COUNT; ++i) {
        detector->mask[i] = detector->delta[i] >= threshold ? 1U : 0U;
    }

    remove_isolated_noise(detector);
    fill_delta_bins(detector, result);
    analyze_regions(detector, result);
    update_state(detector, result);
    update_background(detector, threshold);

    result->timestamp_ms = frame->timestamp_ms;
    result->valid = true;
    return ESP_OK;
}
