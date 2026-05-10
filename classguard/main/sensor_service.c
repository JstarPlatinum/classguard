#include "sensor_service.h"

#include <string.h>
#include "app_config.h"
#include "app_data.h"
#include "driver/uart.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pin_map.h"
#include "pms5003.h"
#include "scd41.h"
#include "sht35.h"

#define ENV_SENSOR_TASK_STACK_SIZE 4096U
#define PMS5003_TASK_STACK_SIZE 4096U
#define SENSOR_TASK_PRIORITY 5U

#define SENSOR_INIT_RETRY_DELAY_MS 2000U
#define ENV_LOOP_DELAY_MS 100U
#define PMS5003_READ_TIMEOUT_MS 3000U
#define PMS5003_ERROR_RETRY_DELAY_MS 1000U
#define PMS5003_RESET_PULSE_MS 100U
#define PMS5003_MAX_CONSECUTIVE_ERRORS 5U

static bool init_sht35(cg_sht35_t *sht35)
{
    esp_err_t ret = cg_sht35_init(sht35, CG_I2C_ENV_PORT, CG_ADDR_SHT35_A);
    if (ret == ESP_OK) {
        cg_app_data_clear_sensor_error(CG_SENSOR_STATUS_SHT35);
        return true;
    }

    ret = cg_sht35_init(sht35, CG_I2C_ENV_PORT, CG_ADDR_SHT35_B);
    if (ret == ESP_OK) {
        cg_app_data_clear_sensor_error(CG_SENSOR_STATUS_SHT35);
        return true;
    }

    cg_app_data_set_sensor_error(CG_SENSOR_STATUS_SHT35, "sht35_init", ret);
    return false;
}

static bool init_scd41(cg_scd41_t *scd41)
{
    esp_err_t ret = cg_scd41_init(scd41, CG_I2C_ENV_PORT);
    if (ret != ESP_OK) {
        cg_app_data_set_sensor_error(CG_SENSOR_STATUS_SCD41, "scd41_init", ret);
        return false;
    }

    (void)cg_scd41_stop_periodic_measurement(scd41);
    ret = cg_scd41_start_periodic_measurement(scd41);
    if (ret != ESP_OK) {
        cg_app_data_set_sensor_error(CG_SENSOR_STATUS_SCD41, "scd41_start", ret);
        return false;
    }

    cg_app_data_clear_sensor_error(CG_SENSOR_STATUS_SCD41);
    return true;
}

static void environment_sensor_task(void *arg)
{
    (void)arg;

    cg_sht35_t sht35 = {0};
    cg_scd41_t scd41 = {0};
    bool sht35_ready = false;
    bool scd41_ready = false;
    uint32_t sht35_elapsed_ms = CG_SHT35_DEFAULT_PERIOD_MS;
    uint32_t scd41_elapsed_ms = 0;
    uint32_t sht35_retry_elapsed_ms = SENSOR_INIT_RETRY_DELAY_MS;
    uint32_t scd41_retry_elapsed_ms = SENSOR_INIT_RETRY_DELAY_MS;

    for (;;) {
        if (!sht35_ready && sht35_retry_elapsed_ms >= SENSOR_INIT_RETRY_DELAY_MS) {
            sht35_ready = init_sht35(&sht35);
            sht35_retry_elapsed_ms = 0;
        }
        if (!scd41_ready && scd41_retry_elapsed_ms >= SENSOR_INIT_RETRY_DELAY_MS) {
            scd41_ready = init_scd41(&scd41);
            scd41_elapsed_ms = 0;
            scd41_retry_elapsed_ms = 0;
        }

        if (sht35_ready && sht35_elapsed_ms >= CG_SHT35_DEFAULT_PERIOD_MS) {
            environment_frame_t frame = {0};
            esp_err_t ret = cg_sht35_read_single_shot(&sht35, &frame);
            if (ret == ESP_OK && frame.sht35_valid) {
                cg_app_data_update_sht35(&frame);
                cg_app_data_clear_sensor_error(CG_SENSOR_STATUS_SHT35);
            } else {
                cg_app_data_set_sensor_error(CG_SENSOR_STATUS_SHT35, "sht35_read", ret);
                sht35_ready = false;
                sht35_retry_elapsed_ms = 0;
            }
            sht35_elapsed_ms = 0;
        }

        if (scd41_ready && scd41_elapsed_ms >= CG_SCD41_DEFAULT_PERIOD_MS) {
            environment_frame_t frame = {0};
            esp_err_t ret = cg_scd41_read_measurement(&scd41, &frame);
            if (ret == ESP_OK && frame.scd41_valid) {
                cg_app_data_update_scd41(&frame);
                cg_app_data_clear_sensor_error(CG_SENSOR_STATUS_SCD41);
            } else {
                cg_app_data_set_sensor_error(CG_SENSOR_STATUS_SCD41, "scd41_read", ret);
                scd41_ready = false;
                scd41_retry_elapsed_ms = 0;
            }
            scd41_elapsed_ms = 0;
        }

        uint32_t delay_ms = ENV_LOOP_DELAY_MS;
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
        sht35_elapsed_ms += delay_ms;
        scd41_elapsed_ms += delay_ms;
        sht35_retry_elapsed_ms += delay_ms;
        scd41_retry_elapsed_ms += delay_ms;
    }
}

static void pms5003_sensor_task(void *arg)
{
    (void)arg;

    cg_pms5003_t pms;
    bool ready = false;
    uint32_t consecutive_errors = 0;

    for (;;) {
        if (!ready) {
            esp_err_t ret = cg_pms5003_init(&pms);
            if (ret != ESP_OK) {
                cg_app_data_set_sensor_error(CG_SENSOR_STATUS_PMS5003, "pms5003_init", ret);
                vTaskDelay(pdMS_TO_TICKS(SENSOR_INIT_RETRY_DELAY_MS));
                continue;
            }

            (void)cg_pms5003_set_active(&pms, true);
            ret = cg_pms5003_reset(&pms, PMS5003_RESET_PULSE_MS);
            if (ret != ESP_OK) {
                cg_app_data_set_sensor_error(CG_SENSOR_STATUS_PMS5003, "pms5003_reset", ret);
            }

            vTaskDelay(pdMS_TO_TICKS(CG_PMS_WAKEUP_DISCARD_MS));
            (void)uart_flush_input(pms.uart_port);
            cg_app_data_clear_sensor_error(CG_SENSOR_STATUS_PMS5003);
            consecutive_errors = 0;
            ready = true;
        }

        pm_frame_t frame;
        memset(&frame, 0, sizeof(frame));

        esp_err_t ret = cg_pms5003_read_frame(&pms, &frame, pdMS_TO_TICKS(PMS5003_READ_TIMEOUT_MS));
        if (ret == ESP_OK && frame.valid) {
            consecutive_errors = 0;
            cg_app_data_update_pms5003(&frame);
            cg_app_data_clear_sensor_error(CG_SENSOR_STATUS_PMS5003);
            continue;
        }

        consecutive_errors++;
        cg_app_data_set_sensor_error(CG_SENSOR_STATUS_PMS5003, "pms5003_read", ret);

        if (consecutive_errors >= PMS5003_MAX_CONSECUTIVE_ERRORS) {
            (void)cg_pms5003_reset(&pms, PMS5003_RESET_PULSE_MS);
            (void)uart_flush_input(pms.uart_port);
            consecutive_errors = 0;
        }

        vTaskDelay(pdMS_TO_TICKS(PMS5003_ERROR_RETRY_DELAY_MS));
    }
}

esp_err_t cg_sensor_service_start(void)
{
    BaseType_t env_ok = xTaskCreate(environment_sensor_task,
                                    "env_sensor",
                                    ENV_SENSOR_TASK_STACK_SIZE,
                                    NULL,
                                    SENSOR_TASK_PRIORITY,
                                    NULL);
    BaseType_t pms_ok = xTaskCreate(pms5003_sensor_task,
                                    "pms5003_sensor",
                                    PMS5003_TASK_STACK_SIZE,
                                    NULL,
                                    SENSOR_TASK_PRIORITY,
                                    NULL);

    return (env_ok == pdPASS && pms_ok == pdPASS) ? ESP_OK : ESP_ERR_NO_MEM;
}
