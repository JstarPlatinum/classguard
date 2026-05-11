#include "fixed_sensor_data.h"

#include <string.h>
#include "app_config.h"

#define FIXED_TIMESTAMP_MS 123456U
#define FIXED_THERMAL_BACKGROUND_C 24.2f

static float fixed_thermal_pixel(uint16_t index)
{
    uint16_t x = index % CG_MLX90640_WIDTH;
    uint16_t y = index / CG_MLX90640_WIDTH;
    float value = FIXED_THERMAL_BACKGROUND_C + (float)((x + y) % 5U) * 0.05f;

    if (x >= 8U && x <= 13U && y >= 7U && y <= 14U) {
        value = 30.4f + (float)((x + y) % 4U) * 0.35f;
    } else if (x >= 20U && x <= 25U && y >= 8U && y <= 16U) {
        value = 31.0f + (float)((x + (2U * y)) % 5U) * 0.30f;
    } else if (x >= 15U && x <= 18U && y >= 17U && y <= 20U) {
        value = 28.6f + (float)((x + y) % 3U) * 0.25f;
    }

    if (x == 30U && y == 2U) {
        value = 35.7f;
    }

    return value;
}

static void fill_fixed_thermal_frame(thermal_frame_t *thermal)
{
    thermal->timestamp_ms = FIXED_TIMESTAMP_MS;
    thermal->min_temp_c = 1000.0f;
    thermal->max_temp_c = -1000.0f;
    thermal->avg_temp_c = 0.0f;
    thermal->hotspot_index = 0U;
    thermal->valid = true;

    float sum = 0.0f;
    for (uint16_t i = 0; i < CG_MLX90640_PIXEL_COUNT; ++i) {
        float value = fixed_thermal_pixel(i);
        thermal->pixels[i] = value;
        sum += value;

        if (value < thermal->min_temp_c) {
            thermal->min_temp_c = value;
        }
        if (value > thermal->max_temp_c) {
            thermal->max_temp_c = value;
            thermal->hotspot_index = i;
        }
    }

    thermal->avg_temp_c = sum / (float)CG_MLX90640_PIXEL_COUNT;
}

void cg_fixed_sensor_data_make_snapshot(cg_app_sensor_snapshot_t *out_snapshot)
{
    if (out_snapshot == NULL) {
        return;
    }

    memset(out_snapshot, 0, sizeof(*out_snapshot));

    out_snapshot->environment.scd41_timestamp_ms = FIXED_TIMESTAMP_MS;
    out_snapshot->environment.scd41_co2_ppm = 1380U;
    out_snapshot->environment.scd41_temperature_c = 26.7f;
    out_snapshot->environment.scd41_humidity_rh = 62.5f;
    out_snapshot->environment.scd41_valid = true;

    out_snapshot->environment.sht35_timestamp_ms = FIXED_TIMESTAMP_MS;
    out_snapshot->environment.sht35_temperature_c = 26.3f;
    out_snapshot->environment.sht35_humidity_rh = 63.1f;
    out_snapshot->environment.sht35_valid = true;

    out_snapshot->pm.timestamp_ms = FIXED_TIMESTAMP_MS;
    out_snapshot->pm.pm1_0_cf1 = 15U;
    out_snapshot->pm.pm2_5_cf1 = 32U;
    out_snapshot->pm.pm10_cf1 = 54U;
    out_snapshot->pm.pm1_0_atm = 13U;
    out_snapshot->pm.pm2_5_atm = 28U;
    out_snapshot->pm.pm10_atm = 49U;
    out_snapshot->pm.particles_0_3um = 1180U;
    out_snapshot->pm.particles_0_5um = 420U;
    out_snapshot->pm.particles_1_0um = 96U;
    out_snapshot->pm.particles_2_5um = 26U;
    out_snapshot->pm.particles_5_0um = 6U;
    out_snapshot->pm.particles_10um = 2U;
    out_snapshot->pm.frames_read = 120U;
    out_snapshot->pm.checksum_errors = 0U;
    out_snapshot->pm.valid = true;

    fill_fixed_thermal_frame(&out_snapshot->thermal);

    out_snapshot->occupancy.timestamp_ms = FIXED_TIMESTAMP_MS;
    out_snapshot->occupancy.occupancy_ratio = 0.72f;
    out_snapshot->occupancy.occupancy_heat_score = 0.18f;
    out_snapshot->occupancy.occupancy_score = 0.612f;
    out_snapshot->occupancy.threshold = 27.8f;
    out_snapshot->occupancy.background_temp = 24.4f;
    out_snapshot->occupancy.interference_threshold = 25.7f;
    out_snapshot->occupancy.human_ref_temp = 31.4f;
    out_snapshot->occupancy.final_threshold = 27.8f;
    out_snapshot->occupancy.max_delta = 11.3f;
    out_snapshot->occupancy.candidate_count = 128U;
    out_snapshot->occupancy.outlier_count = 1U;
    out_snapshot->occupancy.valid_pixels = 82U;
    out_snapshot->occupancy.max_region_area = 48U;
    out_snapshot->occupancy.bins[0] = 596U;
    out_snapshot->occupancy.bins[1] = 44U;
    out_snapshot->occupancy.bins[2] = 58U;
    out_snapshot->occupancy.bins[3] = 69U;
    out_snapshot->occupancy.bins[4] = 1U;
    out_snapshot->occupancy.state = CG_OCCUPANCY_OCCUPIED;
    out_snapshot->occupancy.occupied = true;
    out_snapshot->occupancy.valid = true;

    out_snapshot->sensor_error_mask = 0U;
    out_snapshot->error_message[0] = '\0';
}

void cg_fixed_sensor_data_load_to_app_data(void)
{
    cg_app_sensor_snapshot_t snapshot;
    cg_fixed_sensor_data_make_snapshot(&snapshot);

    cg_app_data_update_scd41(&snapshot.environment);
    cg_app_data_update_sht35(&snapshot.environment);
    cg_app_data_update_pms5003(&snapshot.pm);
    cg_app_data_update_thermal(&snapshot.thermal);
    cg_app_data_update_occupancy(&snapshot.occupancy);

    cg_app_data_clear_sensor_error(CG_SENSOR_STATUS_SHT35);
    cg_app_data_clear_sensor_error(CG_SENSOR_STATUS_SCD41);
    cg_app_data_clear_sensor_error(CG_SENSOR_STATUS_PMS5003);
    cg_app_data_clear_sensor_error(CG_SENSOR_STATUS_MLX90640);
}

void cg_fixed_sensor_data_evaluate_air_quality(cg_air_quality_result_t *out_result)
{
    if (out_result == NULL) {
        return;
    }

    cg_app_sensor_snapshot_t snapshot;
    cg_fixed_sensor_data_make_snapshot(&snapshot);

    cg_air_quality_input_t input;
    cg_air_quality_input_from_frames(&snapshot.environment, &snapshot.pm, &snapshot.occupancy, &input);
    cg_air_quality_evaluate(&input, out_result);
}
