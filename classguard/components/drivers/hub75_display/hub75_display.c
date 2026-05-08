#include "hub75_display.h"

#include <string.h>
#include "app_config.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pin_map.h"

static const char *TAG = "cg_hub75";

static const hub75_pin_config_t s_default_pins = {
    .r1 = CG_PIN_HUB75_R1,
    .g1 = CG_PIN_HUB75_G1,
    .b1 = CG_PIN_HUB75_B1,
    .r2 = CG_PIN_HUB75_R2,
    .g2 = CG_PIN_HUB75_G2,
    .b2 = CG_PIN_HUB75_B2,
    .a = CG_PIN_HUB75_A,
    .b = CG_PIN_HUB75_B,
    .c = CG_PIN_HUB75_C,
    .d = CG_PIN_HUB75_D,
    .e = CG_PIN_HUB75_E,
    .lat = CG_PIN_HUB75_LAT,
    .clk = CG_PIN_HUB75_CLK,
    .oe = CG_PIN_HUB75_OE,
};

static const gpio_num_t s_all_pins[] = {
    CG_PIN_HUB75_R1, CG_PIN_HUB75_G1, CG_PIN_HUB75_B1,
    CG_PIN_HUB75_R2, CG_PIN_HUB75_G2, CG_PIN_HUB75_B2,
    CG_PIN_HUB75_A, CG_PIN_HUB75_B, CG_PIN_HUB75_C,
    CG_PIN_HUB75_D, CG_PIN_HUB75_E, CG_PIN_HUB75_LAT,
    CG_PIN_HUB75_CLK, CG_PIN_HUB75_OE,
};

static esp_err_t set_pin(gpio_num_t pin, int level)
{
    return gpio_set_level(pin, level);
}

static esp_err_t pulse_pin(gpio_num_t pin)
{
    ESP_RETURN_ON_ERROR(set_pin(pin, 1), TAG, "pulse high failed");
    return set_pin(pin, 0);
}

static esp_err_t set_row_address(cg_hub75_display_t *display, uint8_t row)
{
    ESP_RETURN_ON_ERROR(set_pin(display->pins.a, (row >> 0) & 0x01), TAG, "row a failed");
    ESP_RETURN_ON_ERROR(set_pin(display->pins.b, (row >> 1) & 0x01), TAG, "row b failed");
    ESP_RETURN_ON_ERROR(set_pin(display->pins.c, (row >> 2) & 0x01), TAG, "row c failed");
    ESP_RETURN_ON_ERROR(set_pin(display->pins.d, (row >> 3) & 0x01), TAG, "row d failed");
    ESP_RETURN_ON_ERROR(set_pin(display->pins.e, (row >> 4) & 0x01), TAG, "row e failed");
    return ESP_OK;
}

static esp_err_t set_color_bits(cg_hub75_display_t *display, bool red, bool green, bool blue)
{
    ESP_RETURN_ON_ERROR(set_pin(display->pins.r1, red), TAG, "r1 failed");
    ESP_RETURN_ON_ERROR(set_pin(display->pins.g1, green), TAG, "g1 failed");
    ESP_RETURN_ON_ERROR(set_pin(display->pins.b1, blue), TAG, "b1 failed");
    ESP_RETURN_ON_ERROR(set_pin(display->pins.r2, red), TAG, "r2 failed");
    ESP_RETURN_ON_ERROR(set_pin(display->pins.g2, green), TAG, "g2 failed");
    ESP_RETURN_ON_ERROR(set_pin(display->pins.b2, blue), TAG, "b2 failed");
    return ESP_OK;
}

esp_err_t cg_hub75_display_init(cg_hub75_display_t *display)
{
    if (display == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(display, 0, sizeof(*display));
    display->pins = s_default_pins;
    display->width = CG_HUB75_WIDTH;
    display->height = CG_HUB75_HEIGHT;
    display->brightness = CG_HUB75_DEFAULT_BRIGHTNESS;
    display->state = CG_DRIVER_STATE_UNINITIALIZED;

    uint64_t pin_mask = 0;
    for (size_t i = 0; i < sizeof(s_all_pins) / sizeof(s_all_pins[0]); ++i) {
        pin_mask |= 1ULL << s_all_pins[i];
    }

    gpio_config_t cfg = {
        .pin_bit_mask = pin_mask,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&cfg), TAG, "hub75 gpio config failed");
    ESP_RETURN_ON_ERROR(cg_hub75_display_clear(display), TAG, "hub75 clear failed");
    display->state = CG_DRIVER_STATE_READY;
    return ESP_OK;
}

esp_err_t cg_hub75_display_clear(cg_hub75_display_t *display)
{
    if (display == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_RETURN_ON_ERROR(cg_hub75_display_enable_output(display, false), TAG, "disable output failed");
    for (size_t i = 0; i < sizeof(s_all_pins) / sizeof(s_all_pins[0]); ++i) {
        ESP_RETURN_ON_ERROR(set_pin(s_all_pins[i], 0), TAG, "clear pin failed");
    }
    return set_pin(display->pins.oe, 1);
}

esp_err_t cg_hub75_display_enable_output(cg_hub75_display_t *display, bool enable)
{
    if (display == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return set_pin(display->pins.oe, enable ? 0 : 1);
}

esp_err_t cg_hub75_display_run_gpio_test(cg_hub75_display_t *display, uint32_t cycles)
{
    if (display == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (cycles == 0) {
        cycles = 1;
    }

    ESP_RETURN_ON_ERROR(cg_hub75_display_enable_output(display, false), TAG, "disable output failed");
    for (uint32_t cycle = 0; cycle < cycles; ++cycle) {
        for (size_t i = 0; i < sizeof(s_all_pins) / sizeof(s_all_pins[0]); ++i) {
            ESP_RETURN_ON_ERROR(set_pin(s_all_pins[i], 1), TAG, "gpio test high failed");
            vTaskDelay(pdMS_TO_TICKS(20));
            ESP_RETURN_ON_ERROR(set_pin(s_all_pins[i], 0), TAG, "gpio test low failed");
        }
    }
    return ESP_OK;
}

esp_err_t cg_hub75_display_show_color_bars(cg_hub75_display_t *display)
{
    if (display == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_RETURN_ON_ERROR(cg_hub75_display_enable_output(display, true), TAG, "enable output failed");
    for (uint8_t row = 0; row < 16; ++row) {
        ESP_RETURN_ON_ERROR(set_row_address(display, row), TAG, "set row failed");
        for (uint16_t col = 0; col < display->width; ++col) {
            uint8_t bar = (uint8_t)((col * 6U) / display->width);
            bool red = (bar == 0 || bar == 3 || bar == 5);
            bool green = (bar == 1 || bar == 3 || bar == 4);
            bool blue = (bar == 2 || bar == 4 || bar == 5);
            ESP_RETURN_ON_ERROR(set_color_bits(display, red, green, blue), TAG, "set color failed");
            ESP_RETURN_ON_ERROR(pulse_pin(display->pins.clk), TAG, "clock pulse failed");
        }
        ESP_RETURN_ON_ERROR(pulse_pin(display->pins.lat), TAG, "latch pulse failed");
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    return ESP_OK;
}

esp_err_t cg_hub75_display_show_framebuffer(cg_hub75_display_t *display, const uint16_t *rgb565, size_t pixel_count)
{
    (void)display;
    (void)rgb565;
    (void)pixel_count;
    return ESP_ERR_NOT_SUPPORTED;
}
