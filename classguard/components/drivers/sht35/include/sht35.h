/*
 * SHT35 driver v1 data interface:
 *   cg_sht35_init(...) prepares the sensor on the shared environment I2C bus.
 *   cg_sht35_read_single_shot(...) writes sht35_timestamp_ms,
 *   sht35_temperature_c, sht35_humidity_rh, and sht35_valid into
 *   environment_frame_t.
 */
#pragma once

#include <stddef.h>
#include <stdint.h>
#include "driver/i2c.h"
#include "esp_err.h"
#include "data_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CG_SHT35_DRIVER_VERSION_MAJOR 1
#define CG_SHT35_DRIVER_VERSION_MINOR 0
#define CG_SHT35_DRIVER_VERSION_PATCH 0
#define CG_SHT35_DRIVER_VERSION_STRING "v1.0.0"

typedef struct {
    i2c_port_t port;
    uint8_t address;
    cg_driver_state_t state;
} cg_sht35_t;

esp_err_t cg_sht35_init(cg_sht35_t *dev, i2c_port_t port, uint8_t address);
esp_err_t cg_sht35_probe(cg_sht35_t *dev);
esp_err_t cg_sht35_read_single_shot(cg_sht35_t *dev, environment_frame_t *frame);
uint8_t cg_sht35_crc8(const uint8_t *data, size_t length);
const char *cg_sht35_driver_version(void);

#ifdef __cplusplus
}
#endif
