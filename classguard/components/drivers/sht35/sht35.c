/*
 * SHT35 driver v1 data interface:
 *   Call cg_sht35_read_single_shot(...) to receive the final SHT35 output in
 *   environment_frame_t: sht35_timestamp_ms, sht35_temperature_c,
 *   sht35_humidity_rh, sht35_valid.
 */
#include "sht35.h"

#include "app_config.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2c_bus.h"
#include "pin_map.h"

#define SHT35_CMD_SINGLE_HIGH_REPEATABILITY 0x2400U

static uint16_t read_be16(const uint8_t *bytes)
{
    return ((uint16_t)bytes[0] << 8) | bytes[1];
}

uint8_t cg_sht35_crc8(const uint8_t *data, size_t length)
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

static esp_err_t sht35_write_command(cg_sht35_t *dev, uint16_t command)
{
    uint8_t bytes[2] = {(uint8_t)(command >> 8), (uint8_t)(command & 0xFF)};
    return cg_i2c_write(dev->port, dev->address, bytes, sizeof(bytes), pdMS_TO_TICKS(CG_I2C_TIMEOUT_MS));
}

const char *cg_sht35_driver_version(void)
{
    return CG_SHT35_DRIVER_VERSION_STRING;
}

esp_err_t cg_sht35_init(cg_sht35_t *dev, i2c_port_t port, uint8_t address)
{
    if (dev == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    dev->port = port;
    dev->address = (address == 0) ? CG_ADDR_SHT35_A : address;
    dev->state = CG_DRIVER_STATE_UNINITIALIZED;

    esp_err_t ret = cg_i2c_bus_init_env();
    if (ret != ESP_OK) {
        return ret;
    }

    ret = cg_sht35_probe(dev);
    if (ret != ESP_OK) {
        return ret;
    }

    dev->state = CG_DRIVER_STATE_READY;
    return ESP_OK;
}

esp_err_t cg_sht35_probe(cg_sht35_t *dev)
{
    if (dev == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return cg_i2c_bus_probe(dev->port, dev->address, pdMS_TO_TICKS(CG_I2C_TIMEOUT_MS));
}

esp_err_t cg_sht35_read_single_shot(cg_sht35_t *dev, environment_frame_t *frame)
{
    if (dev == NULL || frame == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t bytes[6];
    esp_err_t ret = sht35_write_command(dev, SHT35_CMD_SINGLE_HIGH_REPEATABILITY);
    if (ret != ESP_OK) {
        return ret;
    }

    vTaskDelay(pdMS_TO_TICKS(20));
    ret = cg_i2c_read(dev->port, dev->address, bytes, sizeof(bytes), pdMS_TO_TICKS(CG_I2C_TIMEOUT_MS));
    if (ret != ESP_OK) {
        return ret;
    }

    if (cg_sht35_crc8(&bytes[0], 2) != bytes[2] || cg_sht35_crc8(&bytes[3], 2) != bytes[5]) {
        frame->sht35_valid = false;
        return ESP_ERR_INVALID_CRC;
    }

    uint16_t raw_temp = read_be16(&bytes[0]);
    uint16_t raw_humidity = read_be16(&bytes[3]);
    frame->sht35_timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
    frame->sht35_temperature_c = -45.0f + 175.0f * ((float)raw_temp / 65535.0f);
    frame->sht35_humidity_rh = 100.0f * ((float)raw_humidity / 65535.0f);
    frame->sht35_valid = true;
    return ESP_OK;
}
