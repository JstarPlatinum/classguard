#include "sht35_serial_test.h"

#include <stdbool.h>
#include <stdio.h>
#include "app_config.h"
#include "data_types.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pin_map.h"
#include "sht35.h"

/*
 * Temporary SHT35 serial test.
 *
 * All printf output for this test is intentionally kept in this file so the
 * whole test can be removed later by deleting this file pair and removing the
 * sht35_serial_test_run() call from main.c.
 */

#define SHT35_TEST_READ_PERIOD_MS 2000U

static void print_banner(void)
{
    printf("\n");
    printf("============================================================\n");
    printf("ClassGuard SHT35 serial test\n");
    printf("I2C1 SDA GPIO%d, SCL GPIO%d, freq %u Hz\n", CG_PIN_ENV_SDA, CG_PIN_ENV_SCL, CG_I2C_ENV_FREQ_HZ);
    printf("Trying SHT35 addresses: 0x%02X, 0x%02X\n", CG_ADDR_SHT35_A, CG_ADDR_SHT35_B);
    printf("Output format: timestamp_ms, temperature_c, humidity_rh\n");
    printf("============================================================\n");
}

static bool init_sht35(cg_sht35_t *sht35)
{
    esp_err_t ret = cg_sht35_init(sht35, CG_I2C_ENV_PORT, CG_ADDR_SHT35_A);
    if (ret == ESP_OK) {
        printf("[SHT35_TEST] Found SHT35 at 0x%02X\n", CG_ADDR_SHT35_A);
        return true;
    }

    printf("[SHT35_TEST] Probe 0x%02X failed: %s\n", CG_ADDR_SHT35_A, esp_err_to_name(ret));

    ret = cg_sht35_init(sht35, CG_I2C_ENV_PORT, CG_ADDR_SHT35_B);
    if (ret == ESP_OK) {
        printf("[SHT35_TEST] Found SHT35 at 0x%02X\n", CG_ADDR_SHT35_B);
        return true;
    }

    printf("[SHT35_TEST] Probe 0x%02X failed: %s\n", CG_ADDR_SHT35_B, esp_err_to_name(ret));
    return false;
}

void sht35_serial_test_run(void)
{
    print_banner();

    cg_sht35_t sht35;
    while (!init_sht35(&sht35)) {
        printf("[SHT35_TEST] SHT35 not found. Check wiring/power/pullups. Retrying in 2s...\n");
        vTaskDelay(pdMS_TO_TICKS(SHT35_TEST_READ_PERIOD_MS));
    }

    while (true) {
        environment_frame_t frame = {0};
        esp_err_t ret = cg_sht35_read_single_shot(&sht35, &frame);
        if (ret == ESP_OK && frame.sht35_valid) {
            printf("[SHT35_TEST] %lu ms, temp=%.2f C, humidity=%.2f %%RH\n",
                   (unsigned long)frame.timestamp_ms,
                   frame.temperature_c,
                   frame.humidity_rh);
        } else {
            printf("[SHT35_TEST] read failed: %s\n", esp_err_to_name(ret));
        }

        vTaskDelay(pdMS_TO_TICKS(SHT35_TEST_READ_PERIOD_MS));
    }
}
