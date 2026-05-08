/*
 * Environment I2C v1 data interface:
 *   env_i2c_v1_data_test_get_sht35(...) returns the latest cached
 *   environment_frame_t with sht35_timestamp_ms, sht35_temperature_c,
 *   sht35_humidity_rh, sht35_valid.
 *   env_i2c_v1_data_test_get_scd41(...) returns the latest cached
 *   environment_frame_t with scd41_timestamp_ms, scd41_co2_ppm,
 *   scd41_temperature_c, scd41_humidity_rh, scd41_valid.
 */
#include "env_i2c_v1_data_test.h"

#include <string.h>
#include "app_config.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pin_map.h"
#include "scd41.h"
#include "sht35.h"

#define ENV_I2C_RETRY_DELAY_MS 2000U
#define ENV_I2C_LOOP_DELAY_MS 100U

static environment_frame_t s_latest_sht35;
static environment_frame_t s_latest_scd41;

bool env_i2c_v1_data_test_get_sht35(environment_frame_t *out_frame)
{
    if (out_frame == NULL || !s_latest_sht35.sht35_valid) {
        return false;
    }

    *out_frame = s_latest_sht35;
    return true;
}

bool env_i2c_v1_data_test_get_scd41(environment_frame_t *out_frame)
{
    if (out_frame == NULL || !s_latest_scd41.scd41_valid) {
        return false;
    }

    *out_frame = s_latest_scd41;
    return true;
}

static bool init_sht35(cg_sht35_t *sht35)
{
    if (cg_sht35_init(sht35, CG_I2C_ENV_PORT, CG_ADDR_SHT35_A) == ESP_OK) {
        return true;
    }

    return cg_sht35_init(sht35, CG_I2C_ENV_PORT, CG_ADDR_SHT35_B) == ESP_OK;
}

static bool init_scd41(cg_scd41_t *scd41)
{
    if (cg_scd41_init(scd41, CG_I2C_ENV_PORT) != ESP_OK) {
        return false;
    }

    (void)cg_scd41_stop_periodic_measurement(scd41);
    return cg_scd41_start_periodic_measurement(scd41) == ESP_OK;
}

void env_i2c_v1_data_test_run(void)
{
    cg_sht35_t sht35 = {0};
    cg_scd41_t scd41 = {0};
    bool sht35_ready = false;
    bool scd41_ready = false;
    uint32_t sht35_elapsed_ms = 0;
    uint32_t scd41_elapsed_ms = 0;

    memset(&s_latest_sht35, 0, sizeof(s_latest_sht35));
    memset(&s_latest_scd41, 0, sizeof(s_latest_scd41));

    while (!sht35_ready || !scd41_ready) {
        if (!sht35_ready) {
            sht35_ready = init_sht35(&sht35);
        }

        if (!scd41_ready) {
            scd41_ready = init_scd41(&scd41);
        }

        if (!sht35_ready || !scd41_ready) {
            vTaskDelay(pdMS_TO_TICKS(ENV_I2C_RETRY_DELAY_MS));
        }
    }

    for (;;) {
        if (sht35_elapsed_ms >= CG_SHT35_DEFAULT_PERIOD_MS) {
            environment_frame_t frame = {0};
            if (cg_sht35_read_single_shot(&sht35, &frame) == ESP_OK) {
                s_latest_sht35 = frame;
            } else {
                s_latest_sht35.sht35_valid = false;
            }
            sht35_elapsed_ms = 0;
        }

        if (scd41_elapsed_ms >= CG_SCD41_DEFAULT_PERIOD_MS) {
            environment_frame_t frame = {0};
            if (cg_scd41_read_measurement(&scd41, &frame) == ESP_OK) {
                s_latest_scd41 = frame;
            } else {
                s_latest_scd41.scd41_valid = false;
            }
            scd41_elapsed_ms = 0;
        }

        vTaskDelay(pdMS_TO_TICKS(ENV_I2C_LOOP_DELAY_MS));
        sht35_elapsed_ms += ENV_I2C_LOOP_DELAY_MS;
        scd41_elapsed_ms += ENV_I2C_LOOP_DELAY_MS;
    }
}
