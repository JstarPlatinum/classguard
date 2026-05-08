#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "driver/gpio.h"
#include "app_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CG_DRIVER_STATE_UNINITIALIZED = 0,
    CG_DRIVER_STATE_READY,
    CG_DRIVER_STATE_BUSY,
    CG_DRIVER_STATE_FAULT,
    CG_DRIVER_STATE_UNSUPPORTED,
} cg_driver_state_t;

/*
 * Environment data keeps SCD41 and SHT35 temperature/humidity readings in
 * separate fields. Do not reuse one sensor's temperature/humidity as the
 * other sensor's calibrated output.
 */
typedef struct {
    uint32_t timestamp_ms;
    float pixels[CG_MLX90640_PIXEL_COUNT];
    float min_temp_c;
    float max_temp_c;
    float avg_temp_c;
    uint16_t hotspot_index;
    bool valid;
} thermal_frame_t;

typedef struct {
    uint32_t scd41_timestamp_ms;
    uint16_t scd41_co2_ppm;
    float scd41_temperature_c;
    float scd41_humidity_rh;
    uint32_t sht35_timestamp_ms;
    float sht35_temperature_c;
    float sht35_humidity_rh;
    bool scd41_valid;
    bool sht35_valid;
} environment_frame_t;

typedef struct {
    uint32_t timestamp_ms;
    uint16_t pm1_0_cf1;
    uint16_t pm2_5_cf1;
    uint16_t pm10_cf1;
    uint16_t pm1_0_atm;
    uint16_t pm2_5_atm;
    uint16_t pm10_atm;
    uint16_t particles_0_3um;
    uint16_t particles_0_5um;
    uint16_t particles_1_0um;
    uint16_t particles_2_5um;
    uint16_t particles_5_0um;
    uint16_t particles_10um;
    uint32_t frames_read;
    uint32_t checksum_errors;
    bool valid;
} pm_frame_t;

typedef enum {
    CG_BEAM_SENSOR_1 = 1,
    CG_BEAM_SENSOR_2 = 2,
} cg_beam_sensor_id_t;

typedef enum {
    CG_BEAM_EDGE_FALLING = 0,
    CG_BEAM_EDGE_RISING,
} cg_beam_edge_t;

typedef struct {
    uint32_t timestamp_us;
    cg_beam_sensor_id_t sensor_id;
    cg_beam_edge_t edge;
    int level;
    bool valid;
} beam_event_t;

typedef struct {
    gpio_num_t r1;
    gpio_num_t g1;
    gpio_num_t b1;
    gpio_num_t r2;
    gpio_num_t g2;
    gpio_num_t b2;
    gpio_num_t a;
    gpio_num_t b;
    gpio_num_t c;
    gpio_num_t d;
    gpio_num_t e;
    gpio_num_t lat;
    gpio_num_t clk;
    gpio_num_t oe;
} hub75_pin_config_t;

#ifdef __cplusplus
}
#endif
