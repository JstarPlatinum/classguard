#pragma once

#include <stddef.h>
#include <stdint.h>
#include "driver/i2c.h"
#include "esp_err.h"
#include "data_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    i2c_port_t port;
    uint8_t address;
    cg_driver_state_t state;
} cg_sht35_t;

esp_err_t cg_sht35_init(cg_sht35_t *dev, i2c_port_t port, uint8_t address);
esp_err_t cg_sht35_probe(cg_sht35_t *dev);
esp_err_t cg_sht35_read_single_shot(cg_sht35_t *dev, environment_frame_t *frame);
uint8_t cg_sht35_crc8(const uint8_t *data, size_t length);

#ifdef __cplusplus
}
#endif
