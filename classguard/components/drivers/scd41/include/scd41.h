/*
 * SCD41 driver v1 data interface:
 *   cg_scd41_init(...) prepares the sensor on the shared environment I2C bus.
 *   Start periodic mode with cg_scd41_start_periodic_measurement(...), then
 *   call cg_scd41_read_measurement(...) to write scd41_timestamp_ms,
 *   scd41_co2_ppm, scd41_temperature_c, scd41_humidity_rh, and scd41_valid
 *   into environment_frame_t.
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

#define CG_SCD41_DRIVER_VERSION_MAJOR 1
#define CG_SCD41_DRIVER_VERSION_MINOR 0
#define CG_SCD41_DRIVER_VERSION_PATCH 0
#define CG_SCD41_DRIVER_VERSION_STRING "v1.0.0"

typedef struct {
    i2c_port_t port;
    uint8_t address;
    cg_driver_state_t state;
} cg_scd41_t;

esp_err_t cg_scd41_init(cg_scd41_t *dev, i2c_port_t port);
esp_err_t cg_scd41_probe(cg_scd41_t *dev);
esp_err_t cg_scd41_start_periodic_measurement(cg_scd41_t *dev);
esp_err_t cg_scd41_stop_periodic_measurement(cg_scd41_t *dev);
esp_err_t cg_scd41_read_measurement(cg_scd41_t *dev, environment_frame_t *frame);
uint8_t cg_scd41_crc8(const uint8_t *data, size_t length);
const char *cg_scd41_driver_version(void);

#ifdef __cplusplus
}
#endif
