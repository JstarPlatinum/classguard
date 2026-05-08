#pragma once

#include <stddef.h>
#include <stdint.h>
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    i2c_port_t port;
    gpio_num_t sda_pin;
    gpio_num_t scl_pin;
    uint32_t frequency_hz;
} cg_i2c_bus_config_t;

esp_err_t cg_i2c_bus_init(const cg_i2c_bus_config_t *config);
esp_err_t cg_i2c_bus_init_mlx(void);
esp_err_t cg_i2c_bus_init_env(void);
esp_err_t cg_i2c_bus_probe(i2c_port_t port, uint8_t address, TickType_t timeout_ticks);
esp_err_t cg_i2c_write(i2c_port_t port, uint8_t address, const uint8_t *data, size_t length, TickType_t timeout_ticks);
esp_err_t cg_i2c_read(i2c_port_t port, uint8_t address, uint8_t *data, size_t length, TickType_t timeout_ticks);
esp_err_t cg_i2c_write_read(i2c_port_t port, uint8_t address, const uint8_t *write_data, size_t write_length, uint8_t *read_data, size_t read_length, TickType_t timeout_ticks);
esp_err_t cg_i2c_read_reg16(i2c_port_t port, uint8_t address, uint16_t reg, uint8_t *data, size_t length, TickType_t timeout_ticks);
esp_err_t cg_i2c_write_reg16(i2c_port_t port, uint8_t address, uint16_t reg, const uint8_t *data, size_t length, TickType_t timeout_ticks);

#ifdef __cplusplus
}
#endif
