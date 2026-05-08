#pragma once

#include <stddef.h>
#include <stdint.h>
#include "driver/i2c.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "data_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    i2c_port_t port;
    uint8_t address;
    cg_driver_state_t state;
} cg_mlx90640_t;

esp_err_t cg_mlx90640_init(cg_mlx90640_t *dev, i2c_port_t port);
esp_err_t cg_mlx90640_probe(cg_mlx90640_t *dev);
esp_err_t cg_mlx90640_read_register(cg_mlx90640_t *dev, uint16_t reg, uint16_t *value);
esp_err_t cg_mlx90640_read_words(cg_mlx90640_t *dev, uint16_t start_reg, uint16_t *words, size_t word_count);
esp_err_t cg_mlx90640_read_eeprom(cg_mlx90640_t *dev, uint16_t *eeprom_words, size_t word_count);
esp_err_t cg_mlx90640_read_frame_raw(cg_mlx90640_t *dev, uint16_t *frame_words, size_t word_count);
esp_err_t cg_mlx90640_calculate_temperatures(const uint16_t *eeprom_words, const uint16_t *frame_words, thermal_frame_t *frame);

#ifdef __cplusplus
}
#endif
