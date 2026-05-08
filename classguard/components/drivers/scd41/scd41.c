#include "scd41.h"

#include <stdbool.h>
#include "app_config.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2c_bus.h"
#include "pin_map.h"

#define SCD41_CMD_START_PERIODIC 0x21B1U
#define SCD41_CMD_READ_MEASUREMENT 0xEC05U
#define SCD41_CMD_STOP_PERIODIC 0x3F86U

static const char *TAG = "cg_scd41";

static uint16_t read_be16(const uint8_t *bytes)
{
    return ((uint16_t)bytes[0] << 8) | bytes[1];
}

static esp_err_t scd41_write_command(cg_scd41_t *dev, uint16_t command)
{
    uint8_t bytes[2] = {(uint8_t)(command >> 8), (uint8_t)(command & 0xFF)};
    return cg_i2c_write(dev->port, dev->address, bytes, sizeof(bytes), pdMS_TO_TICKS(CG_I2C_TIMEOUT_MS));
}

uint8_t cg_scd41_crc8(const uint8_t *data, size_t length)
{
    uint8_t crc = 0xFF;
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

esp_err_t cg_scd41_init(cg_scd41_t *dev, i2c_port_t port)
{
    if (dev == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    dev->port = port;
    dev->address = CG_ADDR_SCD41;
    dev->state = CG_DRIVER_STATE_UNINITIALIZED;

    ESP_RETURN_ON_ERROR(cg_i2c_bus_init_env(), TAG, "env i2c init failed");
    ESP_RETURN_ON_ERROR(cg_scd41_probe(dev), TAG, "scd41 probe failed");
    dev->state = CG_DRIVER_STATE_READY;
    return ESP_OK;
}

esp_err_t cg_scd41_probe(cg_scd41_t *dev)
{
    if (dev == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return cg_i2c_bus_probe(dev->port, dev->address, pdMS_TO_TICKS(CG_I2C_TIMEOUT_MS));
}

esp_err_t cg_scd41_start_periodic_measurement(cg_scd41_t *dev)
{
    if (dev == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return scd41_write_command(dev, SCD41_CMD_START_PERIODIC);
}

esp_err_t cg_scd41_stop_periodic_measurement(cg_scd41_t *dev)
{
    if (dev == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    ESP_RETURN_ON_ERROR(scd41_write_command(dev, SCD41_CMD_STOP_PERIODIC), TAG, "stop periodic failed");
    vTaskDelay(pdMS_TO_TICKS(500));
    return ESP_OK;
}

esp_err_t cg_scd41_read_measurement(cg_scd41_t *dev, environment_frame_t *frame)
{
    if (dev == NULL || frame == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t bytes[9];
    ESP_RETURN_ON_ERROR(scd41_write_command(dev, SCD41_CMD_READ_MEASUREMENT), TAG, "read command failed");
    vTaskDelay(pdMS_TO_TICKS(1));
    ESP_RETURN_ON_ERROR(cg_i2c_read(dev->port, dev->address, bytes, sizeof(bytes), pdMS_TO_TICKS(CG_I2C_TIMEOUT_MS)), TAG, "read measurement failed");

    if (cg_scd41_crc8(&bytes[0], 2) != bytes[2] || cg_scd41_crc8(&bytes[3], 2) != bytes[5] || cg_scd41_crc8(&bytes[6], 2) != bytes[8]) {
        frame->scd41_valid = false;
        return ESP_ERR_INVALID_CRC;
    }

    uint16_t raw_co2 = read_be16(&bytes[0]);
    uint16_t raw_temp = read_be16(&bytes[3]);
    uint16_t raw_humidity = read_be16(&bytes[6]);

    frame->timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
    frame->co2_ppm = raw_co2;
    frame->temperature_c = -45.0f + 175.0f * ((float)raw_temp / 65535.0f);
    frame->humidity_rh = 100.0f * ((float)raw_humidity / 65535.0f);
    frame->scd41_valid = true;
    return ESP_OK;
}
