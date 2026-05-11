#include "app_data.h"
#include "app_config.h"
#include "esp_err.h"
#include "fixed_sensor_data.h"
#include "nvs_flash.h"
#include "sensor_service.h"
#include "telemetry_upload.h"
#include "thermal_service.h"


void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    cg_app_data_init();
#if CG_USE_FIXED_SENSOR_DATA
    cg_fixed_sensor_data_load_to_app_data();
#else
    ESP_ERROR_CHECK(cg_sensor_service_start());
    ESP_ERROR_CHECK(cg_thermal_service_start());
#endif
    ESP_ERROR_CHECK(cg_telemetry_upload_start());
}
