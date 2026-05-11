#include "air_quality_evaluator.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    const char *key;
    float value;
} reason_candidate_t;

static float clamp_float(float value, float low, float high)
{
    if (value < low) {
        return low;
    }
    if (value > high) {
        return high;
    }
    return value;
}

static float piecewise_score(float x, const float points[][2], size_t count)
{
    if (count == 0U) {
        return 0.0f;
    }
    if (x <= points[0][0]) {
        return points[0][1];
    }
    if (x >= points[count - 1U][0]) {
        return points[count - 1U][1];
    }

    for (size_t i = 0; i + 1U < count; ++i) {
        float x0 = points[i][0];
        float y0 = points[i][1];
        float x1 = points[i + 1U][0];
        float y1 = points[i + 1U][1];
        if (x >= x0 && x <= x1) {
            float ratio = (x - x0) / (x1 - x0);
            return y0 + ratio * (y1 - y0);
        }
    }
    return points[count - 1U][1];
}

static float comfort_score(float x,
                           float ideal_low,
                           float ideal_high,
                           float accept_low,
                           float accept_high,
                           float warn_low,
                           float warn_high,
                           float bad_low,
                           float bad_high)
{
    if (x >= ideal_low && x <= ideal_high) {
        return 100.0f;
    }
    if (x >= accept_low && x < ideal_low) {
        const float points[][2] = {{accept_low, 85.0f}, {ideal_low, 100.0f}};
        return piecewise_score(x, points, 2U);
    }
    if (x > ideal_high && x <= accept_high) {
        const float points[][2] = {{ideal_high, 100.0f}, {accept_high, 85.0f}};
        return piecewise_score(x, points, 2U);
    }
    if (x >= warn_low && x < accept_low) {
        const float points[][2] = {{warn_low, 65.0f}, {accept_low, 85.0f}};
        return piecewise_score(x, points, 2U);
    }
    if (x > accept_high && x <= warn_high) {
        const float points[][2] = {{accept_high, 85.0f}, {warn_high, 65.0f}};
        return piecewise_score(x, points, 2U);
    }
    if (x >= bad_low && x < warn_low) {
        const float points[][2] = {{bad_low, 45.0f}, {warn_low, 65.0f}};
        return piecewise_score(x, points, 2U);
    }
    if (x > warn_high && x <= bad_high) {
        const float points[][2] = {{warn_high, 65.0f}, {bad_high, 45.0f}};
        return piecewise_score(x, points, 2U);
    }
    return 25.0f;
}

static void copy_text(char *dst, size_t dst_len, const char *src)
{
    if (dst == NULL || dst_len == 0U) {
        return;
    }
    if (src == NULL) {
        dst[0] = '\0';
        return;
    }
    strlcpy(dst, src, dst_len);
}

static void append_redline(cg_air_quality_result_t *result, const char *text)
{
    if (result->redline_count >= CG_AQ_MAX_REDLINES) {
        return;
    }
    copy_text(result->redlines[result->redline_count],
              sizeof(result->redlines[result->redline_count]),
              text);
    result->redline_count++;
}

static void set_level(float score, char *level, size_t level_len)
{
    if (score >= 85.0f) {
        copy_text(level, level_len, "excellent");
    } else if (score >= 70.0f) {
        copy_text(level, level_len, "good");
    } else if (score >= 55.0f) {
        copy_text(level, level_len, "fair");
    } else if (score >= 40.0f) {
        copy_text(level, level_len, "poor");
    } else {
        copy_text(level, level_len, "severe");
    }
}

static void add_reason_candidate(reason_candidate_t *items,
                                 size_t *count,
                                 const char *key,
                                 float score)
{
    items[*count].key = key;
    items[*count].value = score;
    (*count)++;
}

static void fill_main_reasons(cg_air_quality_result_t *result,
                              reason_candidate_t *items,
                              size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        for (size_t j = i + 1U; j < count; ++j) {
            if (items[j].value < items[i].value) {
                reason_candidate_t tmp = items[i];
                items[i] = items[j];
                items[j] = tmp;
            }
        }
    }

    for (size_t i = 0; i < count && result->main_reason_count < CG_AQ_MAX_REASONS; ++i) {
        if (items[i].value >= 75.0f) {
            continue;
        }
        copy_text(result->main_reasons[result->main_reason_count],
                  sizeof(result->main_reasons[result->main_reason_count]),
                  items[i].key);
        result->main_reason_count++;
    }
}

static void select_action_code(const cg_air_quality_input_t *input, cg_air_quality_result_t *result)
{
    result->message[0] = '\0';

    if (result->valid.has_co2 && input->co2_ppm > 1500.0f) {
        copy_text(result->action, sizeof(result->action), "ventilation");
        return;
    }

    if (result->valid.has_occupancy && input->occupancy_ratio >= 0.80f &&
        (!result->valid.has_co2 || input->co2_ppm <= 1000.0f)) {
        copy_text(result->action, sizeof(result->action), "early_ventilation");
        return;
    }

    if (result->valid.has_pm2_5 && input->pm2_5_ugm3 > 75.0f) {
        copy_text(result->action, sizeof(result->action), "purify_air");
        return;
    }

    if (result->valid.has_temperature && input->temperature_c > 30.0f) {
        copy_text(result->action, sizeof(result->action), "cooling");
        return;
    }

    if (result->valid.has_humidity && input->humidity_rh > 80.0f) {
        copy_text(result->action, sizeof(result->action), "dehumidify");
        return;
    }

    if (result->main_reason_count > 0U) {
        copy_text(result->action, sizeof(result->action), "improve_by_reason");
        return;
    }

    copy_text(result->action, sizeof(result->action), "keep_monitoring");
}

void cg_air_quality_input_init(cg_air_quality_input_t *out_input)
{
    if (out_input == NULL) {
        return;
    }

    memset(out_input, 0, sizeof(*out_input));
    out_input->co2_ppm = NAN;
    out_input->pm1_0_ugm3 = NAN;
    out_input->pm2_5_ugm3 = NAN;
    out_input->pm10_ugm3 = NAN;
    out_input->temperature_c = NAN;
    out_input->humidity_rh = NAN;
    out_input->occupancy_ratio = NAN;
}

void cg_air_quality_input_from_frames(const environment_frame_t *environment,
                                      const pm_frame_t *pm,
                                      const occupancy_frame_t *occupancy,
                                      cg_air_quality_input_t *out_input)
{
    if (out_input == NULL) {
        return;
    }

    cg_air_quality_input_init(out_input);

    if (environment != NULL && environment->scd41_valid) {
        out_input->co2_ppm = (float)environment->scd41_co2_ppm;
        out_input->temperature_c = environment->scd41_temperature_c;
        out_input->humidity_rh = environment->scd41_humidity_rh;
        out_input->valid = true;
    }

    if (environment != NULL && environment->sht35_valid) {
        out_input->temperature_c = environment->sht35_temperature_c;
        out_input->humidity_rh = environment->sht35_humidity_rh;
        out_input->valid = true;
    }

    if (pm != NULL && pm->valid) {
        out_input->pm1_0_ugm3 = (float)pm->pm1_0_atm;
        out_input->pm2_5_ugm3 = (float)pm->pm2_5_atm;
        out_input->pm10_ugm3 = (float)pm->pm10_atm;
        out_input->valid = true;
    }

    if (occupancy != NULL && occupancy->valid) {
        out_input->occupancy_ratio = clamp_float(occupancy->occupancy_ratio, 0.0f, 1.0f);
        out_input->valid = true;
    }
}

void cg_air_quality_evaluate(const cg_air_quality_input_t *input,
                             cg_air_quality_result_t *out_result)
{
    if (out_result == NULL) {
        return;
    }

    memset(out_result, 0, sizeof(*out_result));
    out_result->display_score = 0.0f;
    out_result->weighted_score = 0.0f;
    copy_text(out_result->level, sizeof(out_result->level), "no_data");
    copy_text(out_result->action, sizeof(out_result->action), "no_data");
    out_result->message[0] = '\0';

    if (input == NULL || !input->valid) {
        return;
    }

    out_result->valid.has_co2 = isfinite(input->co2_ppm) && input->co2_ppm > 0.0f;
    out_result->valid.has_pm1_0 = isfinite(input->pm1_0_ugm3) && input->pm1_0_ugm3 >= 0.0f;
    out_result->valid.has_pm2_5 = isfinite(input->pm2_5_ugm3) && input->pm2_5_ugm3 >= 0.0f;
    out_result->valid.has_pm10 = isfinite(input->pm10_ugm3) && input->pm10_ugm3 >= 0.0f;
    out_result->valid.has_temperature =
        isfinite(input->temperature_c) && input->temperature_c > -40.0f && input->temperature_c < 85.0f;
    out_result->valid.has_humidity =
        isfinite(input->humidity_rh) && input->humidity_rh >= 0.0f && input->humidity_rh <= 100.0f;
    out_result->valid.has_occupancy =
        isfinite(input->occupancy_ratio) && input->occupancy_ratio >= 0.0f && input->occupancy_ratio <= 1.0f;

    float cap_score = 100.0f;
    reason_candidate_t candidates[7];
    size_t candidate_count = 0U;

    float weights_co2 = 0.30f;
    float weights_pm2_5 = 0.22f;
    float weights_pm10 = 0.10f;
    float weights_pm1_0 = 0.08f;
    float weights_temperature = 0.10f;
    float weights_humidity = 0.10f;
    float weights_occupancy = 0.10f;

    if (out_result->valid.has_occupancy) {
        float r = clamp_float(input->occupancy_ratio, 0.0f, 1.0f);
        weights_co2 *= fminf(1.80f, 1.0f + 0.80f * fmaxf(0.0f, r - 0.50f) / 0.50f);
        weights_humidity *= fminf(1.50f, 1.0f + 0.50f * fmaxf(0.0f, r - 0.65f) / 0.35f);
        weights_occupancy *= fminf(2.00f, 1.0f + fmaxf(0.0f, r - 0.60f) / 0.40f);
    }

    if (out_result->valid.has_co2) {
        const float points[][2] = {
            {400.0f, 100.0f}, {800.0f, 100.0f}, {1000.0f, 80.0f},
            {1500.0f, 60.0f}, {2000.0f, 40.0f}, {3000.0f, 20.0f}, {5000.0f, 5.0f},
        };
        out_result->scores.co2 = piecewise_score(input->co2_ppm, points, 7U);
        add_reason_candidate(candidates, &candidate_count, "co2", out_result->scores.co2);
        if (input->co2_ppm > 3000.0f) {
            cap_score = fminf(cap_score, 30.0f);
            append_redline(out_result, "co2_gt_3000");
        } else if (input->co2_ppm > 2000.0f) {
            cap_score = fminf(cap_score, 40.0f);
            append_redline(out_result, "co2_gt_2000");
        } else if (input->co2_ppm > 1500.0f) {
            cap_score = fminf(cap_score, 60.0f);
            append_redline(out_result, "co2_gt_1500");
        } else if (input->co2_ppm > 1000.0f) {
            cap_score = fminf(cap_score, 75.0f);
            append_redline(out_result, "co2_gt_1000");
        }
    }

    if (out_result->valid.has_pm2_5) {
        const float points[][2] = {
            {0.0f, 100.0f}, {15.0f, 100.0f}, {35.0f, 85.0f}, {50.0f, 70.0f},
            {75.0f, 50.0f}, {150.0f, 30.0f}, {250.0f, 15.0f},
        };
        out_result->scores.pm2_5 = piecewise_score(input->pm2_5_ugm3, points, 7U);
        add_reason_candidate(candidates, &candidate_count, "pm2_5", out_result->scores.pm2_5);
        if (input->pm2_5_ugm3 > 150.0f) {
            cap_score = fminf(cap_score, 35.0f);
            append_redline(out_result, "pm25_gt_150");
        } else if (input->pm2_5_ugm3 > 75.0f) {
            cap_score = fminf(cap_score, 50.0f);
            append_redline(out_result, "pm25_gt_75");
        }
    }

    if (out_result->valid.has_pm10) {
        const float points[][2] = {
            {0.0f, 100.0f}, {45.0f, 100.0f}, {75.0f, 85.0f}, {100.0f, 70.0f},
            {150.0f, 50.0f}, {300.0f, 30.0f}, {500.0f, 15.0f},
        };
        out_result->scores.pm10 = piecewise_score(input->pm10_ugm3, points, 7U);
        add_reason_candidate(candidates, &candidate_count, "pm10", out_result->scores.pm10);
        if (input->pm10_ugm3 > 300.0f) {
            cap_score = fminf(cap_score, 40.0f);
            append_redline(out_result, "pm10_gt_300");
        } else if (input->pm10_ugm3 > 150.0f) {
            cap_score = fminf(cap_score, 55.0f);
            append_redline(out_result, "pm10_gt_150");
        }
    }

    if (out_result->valid.has_pm1_0) {
        const float points[][2] = {
            {0.0f, 100.0f}, {10.0f, 100.0f}, {20.0f, 85.0f}, {35.0f, 70.0f},
            {50.0f, 50.0f}, {100.0f, 30.0f}, {150.0f, 15.0f},
        };
        out_result->scores.pm1_0 = piecewise_score(input->pm1_0_ugm3, points, 7U);
        add_reason_candidate(candidates, &candidate_count, "pm1_0", out_result->scores.pm1_0);
        if (input->pm1_0_ugm3 > 50.0f) {
            cap_score = fminf(cap_score, 60.0f);
            append_redline(out_result, "pm1_gt_50");
        }
    }

    if (out_result->valid.has_temperature) {
        out_result->scores.temperature = comfort_score(input->temperature_c,
                                                       22.0f,
                                                       26.0f,
                                                       20.0f,
                                                       28.0f,
                                                       18.0f,
                                                       30.0f,
                                                       16.0f,
                                                       32.0f);
        add_reason_candidate(candidates, &candidate_count, "temperature", out_result->scores.temperature);
        if (input->temperature_c > 32.0f) {
            cap_score = fminf(cap_score, 60.0f);
            append_redline(out_result, "temperature_gt_32");
        } else if (input->temperature_c > 30.0f) {
            cap_score = fminf(cap_score, 70.0f);
            append_redline(out_result, "temperature_gt_30");
        }
    }

    if (out_result->valid.has_humidity) {
        out_result->scores.humidity = comfort_score(input->humidity_rh,
                                                    40.0f,
                                                    60.0f,
                                                    30.0f,
                                                    70.0f,
                                                    25.0f,
                                                    80.0f,
                                                    20.0f,
                                                    85.0f);
        add_reason_candidate(candidates, &candidate_count, "humidity", out_result->scores.humidity);
        if (input->humidity_rh > 85.0f) {
            cap_score = fminf(cap_score, 60.0f);
            append_redline(out_result, "humidity_gt_85");
        } else if (input->humidity_rh > 80.0f) {
            cap_score = fminf(cap_score, 65.0f);
            append_redline(out_result, "humidity_gt_80");
        } else if (input->humidity_rh < 20.0f) {
            cap_score = fminf(cap_score, 60.0f);
            append_redline(out_result, "humidity_lt_20");
        } else if (input->humidity_rh < 25.0f) {
            cap_score = fminf(cap_score, 70.0f);
            append_redline(out_result, "humidity_lt_25");
        }
    }

    if (out_result->valid.has_occupancy) {
        const float points[][2] = {
            {0.0f, 100.0f}, {0.30f, 100.0f}, {0.50f, 90.0f}, {0.65f, 75.0f},
            {0.80f, 60.0f}, {0.90f, 40.0f}, {1.0f, 25.0f},
        };
        out_result->scores.occupancy = piecewise_score(input->occupancy_ratio, points, 7U);
        add_reason_candidate(candidates, &candidate_count, "occupancy", out_result->scores.occupancy);
        if (input->occupancy_ratio >= 0.95f) {
            cap_score = fminf(cap_score, 55.0f);
            append_redline(out_result, "occupancy_ge_95");
        } else if (input->occupancy_ratio >= 0.90f) {
            cap_score = fminf(cap_score, 60.0f);
            append_redline(out_result, "occupancy_ge_90");
        } else if (input->occupancy_ratio >= 0.80f) {
            cap_score = fminf(cap_score, 70.0f);
            append_redline(out_result, "occupancy_ge_80");
        } else if (input->occupancy_ratio >= 0.65f) {
            cap_score = fminf(cap_score, 80.0f);
            append_redline(out_result, "occupancy_ge_65");
        }
    }

    float weight_sum = 0.0f;
    weight_sum += out_result->valid.has_co2 ? weights_co2 : 0.0f;
    weight_sum += out_result->valid.has_pm2_5 ? weights_pm2_5 : 0.0f;
    weight_sum += out_result->valid.has_pm10 ? weights_pm10 : 0.0f;
    weight_sum += out_result->valid.has_pm1_0 ? weights_pm1_0 : 0.0f;
    weight_sum += out_result->valid.has_temperature ? weights_temperature : 0.0f;
    weight_sum += out_result->valid.has_humidity ? weights_humidity : 0.0f;
    weight_sum += out_result->valid.has_occupancy ? weights_occupancy : 0.0f;

    if (weight_sum <= 0.0f) {
        return;
    }

    out_result->weights.co2 = out_result->valid.has_co2 ? weights_co2 / weight_sum : 0.0f;
    out_result->weights.pm2_5 = out_result->valid.has_pm2_5 ? weights_pm2_5 / weight_sum : 0.0f;
    out_result->weights.pm10 = out_result->valid.has_pm10 ? weights_pm10 / weight_sum : 0.0f;
    out_result->weights.pm1_0 = out_result->valid.has_pm1_0 ? weights_pm1_0 / weight_sum : 0.0f;
    out_result->weights.temperature = out_result->valid.has_temperature ? weights_temperature / weight_sum : 0.0f;
    out_result->weights.humidity = out_result->valid.has_humidity ? weights_humidity / weight_sum : 0.0f;
    out_result->weights.occupancy = out_result->valid.has_occupancy ? weights_occupancy / weight_sum : 0.0f;

    out_result->weighted_score =
        out_result->scores.co2 * out_result->weights.co2 +
        out_result->scores.pm2_5 * out_result->weights.pm2_5 +
        out_result->scores.pm10 * out_result->weights.pm10 +
        out_result->scores.pm1_0 * out_result->weights.pm1_0 +
        out_result->scores.temperature * out_result->weights.temperature +
        out_result->scores.humidity * out_result->weights.humidity +
        out_result->scores.occupancy * out_result->weights.occupancy;

    out_result->display_score = fminf(out_result->weighted_score, cap_score);
    set_level(out_result->display_score, out_result->level, sizeof(out_result->level));
    fill_main_reasons(out_result, candidates, candidate_count);
    select_action_code(input, out_result);
}
