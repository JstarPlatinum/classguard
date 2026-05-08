#include "scd41_serial_test.h"

#include <stdio.h>
#include "app_config.h"
#include "data_types.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pin_map.h"
#include "scd41.h"

/*
 * Temporary SCD41 serial test.
 *
 * All printf output for this test is intentionally kept in this file so the
 * whole test can be removed later by deleting this file pair and removing the
 * scd41_serial_test_run() call from main.c.
 */

#define SCD41_TEST_FIRST_READ_DELAY_MS 5000U
#define SCD41_TEST_READ_PERIOD_MS 5000U

static void print_banner(void)
{
    printf("\n");
    printf("============================================================\n");
    printf("ClassGuard SCD41 serial test\n");
    printf("I2C1 SDA GPIO%d, SCL GPIO%d, freq %u Hz\n", CG_PIN_ENV_SDA, CG_PIN_ENV_SCL, CG_I2C_ENV_FREQ_HZ);
    printf("SCD41 address: 0x%02X\n", CG_ADDR_SCD41);
    printf("Output format: timestamp_ms, co2_ppm, temperature_c, humidity_rh\n");
    printf("============================================================\n");
}

static bool init_scd41(cg_scd41_t *scd41)
{
    esp_err_t ret = cg_scd41_init(scd41, CG_I2C_ENV_PORT);
    if (ret != ESP_OK) {
        printf("[SCD41_TEST] Probe 0x%02X failed: %s\n", CG_ADDR_SCD41, esp_err_to_name(ret));
        return false;
    }

    printf("[SCD41_TEST] Found SCD41 at 0x%02X\n", CG_ADDR_SCD41);

    ret = cg_scd41_stop_periodic_measurement(scd41);
    if (ret != ESP_OK) {
        printf("[SCD41_TEST] stop periodic returned: %s; continuing\n", esp_err_to_name(ret));
    }

    ret = cg_scd41_start_periodic_measurement(scd41);
    if (ret != ESP_OK) {
        printf("[SCD41_TEST] start periodic failed: %s\n", esp_err_to_name(ret));
        return false;
    }

    printf("[SCD41_TEST] Periodic measurement started. Waiting %u ms for first sample...\n", SCD41_TEST_FIRST_READ_DELAY_MS);
    vTaskDelay(pdMS_TO_TICKS(SCD41_TEST_FIRST_READ_DELAY_MS));
    return true;
}

void scd41_serial_test_run(void)
{
    print_banner();

    cg_scd41_t scd41;
    while (!init_scd41(&scd41)) {
        printf("[SCD41_TEST] SCD41 not ready. Check wiring/power/pullups. Retrying in 2s...\n");
        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    while (true) {
        environment_frame_t frame = {0};
        esp_err_t ret = cg_scd41_read_measurement(&scd41, &frame);
        if (ret == ESP_OK && frame.scd41_valid) {
            printf("[SCD41_TEST] %lu ms, co2=%u ppm, temp=%.2f C, humidity=%.2f %%RH\n",
                   (unsigned long)frame.timestamp_ms,
                   frame.co2_ppm,
                   frame.temperature_c,
                   frame.humidity_rh);
        } else {
            printf("[SCD41_TEST] read failed: %s\n", esp_err_to_name(ret));
        }

        vTaskDelay(pdMS_TO_TICKS(SCD41_TEST_READ_PERIOD_MS));
    }
}
