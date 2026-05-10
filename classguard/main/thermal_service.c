#include "thermal_service.h"

#include <string.h>
#include "app_config.h"
#include "app_data.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mlx90640.h"
#include "occupancy_detector.h"
#include "pin_map.h"

#define THERMAL_TASK_STACK_SIZE 24576U
#define THERMAL_TASK_PRIORITY 5U
#define THERMAL_INIT_RETRY_DELAY_MS 2000U

static void thermal_occupancy_task(void *arg)
{
    (void)arg;

    static cg_mlx90640_t mlx;
    static cg_occupancy_detector_t detector;
    bool ready = false;

    cg_occupancy_detector_init(&detector);

    for (;;) {
        if (!ready) {
            esp_err_t ret = cg_mlx90640_init(&mlx, CG_I2C_MLX_PORT);
            if (ret != ESP_OK) {
                cg_app_data_set_sensor_error(CG_SENSOR_STATUS_MLX90640, "mlx90640_init", ret);
                vTaskDelay(pdMS_TO_TICKS(THERMAL_INIT_RETRY_DELAY_MS));
                continue;
            }

            cg_occupancy_detector_init(&detector);
            cg_app_data_clear_sensor_error(CG_SENSOR_STATUS_MLX90640);
            ready = true;
        }

        static thermal_frame_t frame;
        memset(&frame, 0, sizeof(frame));
        esp_err_t ret = cg_mlx90640_read_thermal_frame(&mlx, &frame);
        if (ret != ESP_OK || !frame.valid) {
            cg_app_data_set_sensor_error(CG_SENSOR_STATUS_MLX90640, "mlx90640_read", ret);
            ready = false;
            vTaskDelay(pdMS_TO_TICKS(THERMAL_INIT_RETRY_DELAY_MS));
            continue;
        }

        cg_app_data_update_thermal(&frame);

        static occupancy_frame_t occupancy;
        memset(&occupancy, 0, sizeof(occupancy));
        ret = cg_occupancy_detector_update(&detector, &frame, &occupancy);
        if (ret == ESP_OK) {
            if (occupancy.valid) {
                cg_app_data_update_occupancy(&occupancy);
            }
            cg_app_data_clear_sensor_error(CG_SENSOR_STATUS_MLX90640);
        } else {
            cg_app_data_set_sensor_error(CG_SENSOR_STATUS_MLX90640, "occupancy_update", ret);
        }

        vTaskDelay(pdMS_TO_TICKS(CG_MLX90640_DEFAULT_PERIOD_MS));
    }
}

esp_err_t cg_thermal_service_start(void)
{
    BaseType_t ok = xTaskCreate(thermal_occupancy_task,
                                "thermal_occupancy",
                                THERMAL_TASK_STACK_SIZE,
                                NULL,
                                THERMAL_TASK_PRIORITY,
                                NULL);

    return ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
