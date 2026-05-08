#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "data_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uart_port_t uart_port;
    gpio_num_t rx_pin;
    gpio_num_t tx_pin;
    gpio_num_t set_pin;
    gpio_num_t reset_pin;
    uint32_t frames_read;
    uint32_t checksum_errors;
    cg_driver_state_t state;
} cg_pms5003_t;

esp_err_t cg_pms5003_init(cg_pms5003_t *dev);
esp_err_t cg_pms5003_set_active(cg_pms5003_t *dev, bool active);
esp_err_t cg_pms5003_reset(cg_pms5003_t *dev, uint32_t reset_ms);
esp_err_t cg_pms5003_read_frame(cg_pms5003_t *dev, pm_frame_t *frame, TickType_t timeout_ticks);
bool cg_pms5003_parse_frame_bytes(const uint8_t *bytes, size_t length, pm_frame_t *frame);

#ifdef __cplusplus
}
#endif
