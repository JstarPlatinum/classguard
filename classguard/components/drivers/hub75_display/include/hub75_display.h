#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "data_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    hub75_pin_config_t pins;
    uint16_t width;
    uint16_t height;
    uint8_t brightness;
    cg_driver_state_t state;
} cg_hub75_display_t;

esp_err_t cg_hub75_display_init(cg_hub75_display_t *display);
esp_err_t cg_hub75_display_clear(cg_hub75_display_t *display);
esp_err_t cg_hub75_display_enable_output(cg_hub75_display_t *display, bool enable);
esp_err_t cg_hub75_display_run_gpio_test(cg_hub75_display_t *display, uint32_t cycles);
esp_err_t cg_hub75_display_show_color_bars(cg_hub75_display_t *display);
esp_err_t cg_hub75_display_show_framebuffer(cg_hub75_display_t *display, const uint16_t *rgb565, size_t pixel_count);

#ifdef __cplusplus
}
#endif
