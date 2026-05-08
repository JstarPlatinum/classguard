#include "mlx90640.h"

#include <string.h>
#include "app_config.h"
#include "esp_check.h"
#include "i2c_bus.h"
#include "pin_map.h"

static const char *TAG = "cg_mlx90640";

esp_err_t cg_mlx90640_init(cg_mlx90640_t *dev, i2c_port_t port)
{
    if (dev == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    dev->port = port;
    dev->address = CG_ADDR_MLX90640;
    dev->state = CG_DRIVER_STATE_UNINITIALIZED;

    ESP_RETURN_ON_ERROR(cg_i2c_bus_init_mlx(), TAG, "mlx i2c init failed");
    ESP_RETURN_ON_ERROR(cg_mlx90640_probe(dev), TAG, "mlx probe failed");
    dev->state = CG_DRIVER_STATE_READY;
    return ESP_OK;
}

esp_err_t cg_mlx90640_probe(cg_mlx90640_t *dev)
{
    if (dev == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return cg_i2c_bus_probe(dev->port, dev->address, pdMS_TO_TICKS(CG_I2C_TIMEOUT_MS));
}

esp_err_t cg_mlx90640_read_register(cg_mlx90640_t *dev, uint16_t reg, uint16_t *value)
{
    if (dev == NULL || value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t bytes[2];
    ESP_RETURN_ON_ERROR(cg_i2c_read_reg16(dev->port, dev->address, reg, bytes, sizeof(bytes), pdMS_TO_TICKS(CG_I2C_TIMEOUT_MS)), TAG, "mlx read register failed");
    *value = ((uint16_t)bytes[0] << 8) | bytes[1];
    return ESP_OK;
}

esp_err_t cg_mlx90640_read_words(cg_mlx90640_t *dev, uint16_t start_reg, uint16_t *words, size_t word_count)
{
    if (dev == NULL || words == NULL || word_count == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t offset = 0;
    while (offset < word_count) {
        size_t chunk_words = word_count - offset;
        if (chunk_words > 16) {
            chunk_words = 16;
        }

        uint8_t bytes[32];
        ESP_RETURN_ON_ERROR(cg_i2c_read_reg16(dev->port, dev->address, start_reg + offset, bytes, chunk_words * 2, pdMS_TO_TICKS(CG_I2C_TIMEOUT_MS)), TAG, "mlx read words failed");
        for (size_t i = 0; i < chunk_words; ++i) {
            words[offset + i] = ((uint16_t)bytes[i * 2] << 8) | bytes[i * 2 + 1];
        }
        offset += chunk_words;
    }

    return ESP_OK;
}

esp_err_t cg_mlx90640_read_eeprom(cg_mlx90640_t *dev, uint16_t *eeprom_words, size_t word_count)
{
    if (word_count < CG_MLX90640_EEPROM_WORDS) {
        return ESP_ERR_INVALID_SIZE;
    }
    return cg_mlx90640_read_words(dev, CG_MLX90640_EEPROM_START, eeprom_words, CG_MLX90640_EEPROM_WORDS);
}

esp_err_t cg_mlx90640_read_frame_raw(cg_mlx90640_t *dev, uint16_t *frame_words, size_t word_count)
{
    if (word_count < CG_MLX90640_FRAME_WORDS) {
        return ESP_ERR_INVALID_SIZE;
    }
    return cg_mlx90640_read_words(dev, CG_MLX90640_FRAME_START, frame_words, CG_MLX90640_FRAME_WORDS);
}

esp_err_t cg_mlx90640_calculate_temperatures(const uint16_t *eeprom_words, const uint16_t *frame_words, thermal_frame_t *frame)
{
    (void)eeprom_words;
    (void)frame_words;
    if (frame != NULL) {
        memset(frame, 0, sizeof(*frame));
        frame->valid = false;
    }
    return ESP_ERR_NOT_SUPPORTED;
}
