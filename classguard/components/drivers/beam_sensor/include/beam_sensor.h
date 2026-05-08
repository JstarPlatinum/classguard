#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "driver/gpio.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "data_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    gpio_num_t sensor1_pin;
    gpio_num_t sensor2_pin;
    uint32_t debounce_us;
    bool active_low;
    QueueHandle_t event_queue;
    cg_driver_state_t state;
} cg_beam_sensor_t;

esp_err_t cg_beam_sensor_init(cg_beam_sensor_t *dev);
esp_err_t cg_beam_sensor_deinit(cg_beam_sensor_t *dev);
bool cg_beam_sensor_read_event(cg_beam_sensor_t *dev, beam_event_t *event, TickType_t timeout_ticks);

#ifdef __cplusplus
}
#endif
