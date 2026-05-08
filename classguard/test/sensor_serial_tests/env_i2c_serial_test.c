#include "env_i2c_serial_test.h"

#include <stdbool.h>
#include <stdio.h>
#include "app_config.h"
#include "data_types.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pin_map.h"
#include "scd41.h"
#include "sht35.h"

/*
 * Temporary shared-I2C environment sensor serial test.
 *
 * Wiring:
 *   GPIO41 -> SCD41 SDA and SHT35 SDA
 *   GPIO42 -> SCD41 SCL and SHT35 SCL
 *
 * All printf output for this test is intentionally kept in this file so the
 * whole test can be removed later by deleting this file pair and removing the
 * env_i2c_serial_test_run() call from main.c.
 */

#define ENV_TEST_SHT35_PERIOD_MS 2000U
#define ENV_TEST_SCD41_PERIOD_MS 5000U
#define ENV_TEST_LOOP_PERIOD_MS 200U
#define ENV_TEST_SCD41_FIRST_DELAY_MS 5000U

static void print_banner(void)
{
    printf("\n");
    printf("============================================================\n");
    printf("ClassGuard shared I2C environment serial test\n");
    printf("I2C1 SDA GPIO%d -> SCD41 SDA + SHT35 SDA\n", CG_PIN_ENV_SDA);
    printf("I2C1 SCL GPIO%d -> SCD41 SCL + SHT35 SCL\n", CG_PIN_ENV_SCL);
    printf("I2C freq: %u Hz\n", CG_I2C_ENV_FREQ_HZ);
    printf("SCD41 address: 0x%02X, SHT35 addresses: 0x%02X/0x%02X\n", CG_ADDR_SCD41, CG_ADDR_SHT35_A, CG_ADDR_SHT35_B);
    printf("SHT35 reads every %u ms; SCD41 reads every %u ms\n", ENV_TEST_SHT35_PERIOD_MS, ENV_TEST_SCD41_PERIOD_MS);
    printf("============================================================\n");
}

static bool init_sht35(cg_sht35_t *sht35)
{
    esp_err_t ret = cg_sht35_init(sht35, CG_I2C_ENV_PORT, CG_ADDR_SHT35_A);
    if (ret == ESP_OK) {
        printf("[ENV_I2C_TEST][SHT35] Found at 0x%02X\n", CG_ADDR_SHT35_A);
        return true;
    }

    printf("[ENV_I2C_TEST][SHT35] Probe 0x%02X failed: %s\n", CG_ADDR_SHT35_A, esp_err_to_name(ret));

    ret = cg_sht35_init(sht35, CG_I2C_ENV_PORT, CG_ADDR_SHT35_B);
    if (ret == ESP_OK) {
        printf("[ENV_I2C_TEST][SHT35] Found at 0x%02X\n", CG_ADDR_SHT35_B);
        return true;
    }

    printf("[ENV_I2C_TEST][SHT35] Probe 0x%02X failed: %s\n", CG_ADDR_SHT35_B, esp_err_to_name(ret));
    return false;
}

static bool init_scd41(cg_scd41_t *scd41)
{
    esp_err_t ret = cg_scd41_init(scd41, CG_I2C_ENV_PORT);
    if (ret != ESP_OK) {
        printf("[ENV_I2C_TEST][SCD41] Probe 0x%02X failed: %s\n", CG_ADDR_SCD41, esp_err_to_name(ret));
        return false;
    }

    printf("[ENV_I2C_TEST][SCD41] Found at 0x%02X\n", CG_ADDR_SCD41);

    ret = cg_scd41_stop_periodic_measurement(scd41);
    if (ret != ESP_OK) {
        printf("[ENV_I2C_TEST][SCD41] stop periodic returned: %s; continuing\n", esp_err_to_name(ret));
    }

    ret = cg_scd41_start_periodic_measurement(scd41);
    if (ret != ESP_OK) {
        printf("[ENV_I2C_TEST][SCD41] start periodic failed: %s\n", esp_err_to_name(ret));
        return false;
    }

    printf("[ENV_I2C_TEST][SCD41] Periodic measurement started. First read after %u ms.\n", ENV_TEST_SCD41_FIRST_DELAY_MS);
    return true;
}

static void read_sht35(cg_sht35_t *sht35)
{
    environment_frame_t frame = {0};
    esp_err_t ret = cg_sht35_read_single_shot(sht35, &frame);
    if (ret == ESP_OK && frame.sht35_valid) {
        printf("[ENV_I2C_TEST][SHT35] %lu ms, temp=%.2f C, humidity=%.2f %%RH\n",
               (unsigned long)frame.timestamp_ms,
               frame.temperature_c,
               frame.humidity_rh);
    } else {
        printf("[ENV_I2C_TEST][SHT35] read failed: %s\n", esp_err_to_name(ret));
    }
}

static void read_scd41(cg_scd41_t *scd41)
{
    environment_frame_t frame = {0};
    esp_err_t ret = cg_scd41_read_measurement(scd41, &frame);
    if (ret == ESP_OK && frame.scd41_valid) {
        printf("[ENV_I2C_TEST][SCD41] %lu ms, co2=%u ppm, temp=%.2f C, humidity=%.2f %%RH\n",
               (unsigned long)frame.timestamp_ms,
               frame.co2_ppm,
               frame.temperature_c,
               frame.humidity_rh);
    } else {
        printf("[ENV_I2C_TEST][SCD41] read failed: %s\n", esp_err_to_name(ret));
    }
}

void env_i2c_serial_test_run(void)
{
    print_banner();

    cg_sht35_t sht35;
    cg_scd41_t scd41;

    bool sht35_ready = false;
    bool scd41_ready = false;
    while (!sht35_ready || !scd41_ready) {
        if (!sht35_ready) {
            sht35_ready = init_sht35(&sht35);
        }
        if (!scd41_ready) {
            scd41_ready = init_scd41(&scd41);
        }

        if (!sht35_ready || !scd41_ready) {
            printf("[ENV_I2C_TEST] Waiting for both sensors. Check shared SDA/SCL wiring, power, and pullups. Retrying in 2s...\n");
            vTaskDelay(pdMS_TO_TICKS(2000));
        }
    }

    int64_t now_ms = esp_timer_get_time() / 1000LL;
    int64_t next_sht35_ms = now_ms;
    int64_t next_scd41_ms = now_ms + ENV_TEST_SCD41_FIRST_DELAY_MS;

    while (true) {
        now_ms = esp_timer_get_time() / 1000LL;

        if (now_ms >= next_sht35_ms) {
            read_sht35(&sht35);
            next_sht35_ms = now_ms + ENV_TEST_SHT35_PERIOD_MS;
        }

        if (now_ms >= next_scd41_ms) {
            read_scd41(&scd41);
            next_scd41_ms = now_ms + ENV_TEST_SCD41_PERIOD_MS;
        }

        vTaskDelay(pdMS_TO_TICKS(ENV_TEST_LOOP_PERIOD_MS));
    }
}
