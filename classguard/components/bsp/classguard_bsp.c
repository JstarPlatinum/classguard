#include "classguard_bsp.h"

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pin_map.h"

static const hub75_pin_config_t s_hub75_pins = {
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

static esp_err_t configure_output(gpio_num_t pin, int level)
{
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << pin,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&cfg), "cg_bsp", "gpio_config output failed");
    return gpio_set_level(pin, level);
}

static esp_err_t configure_input_pullup(gpio_num_t pin)
{
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << pin,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    return gpio_config(&cfg);
}

esp_err_t cg_bsp_init_safe_gpio(void)
{
    const gpio_num_t hub75_outputs[] = {
        CG_PIN_HUB75_R1, CG_PIN_HUB75_G1, CG_PIN_HUB75_B1,
        CG_PIN_HUB75_R2, CG_PIN_HUB75_G2, CG_PIN_HUB75_B2,
        CG_PIN_HUB75_A, CG_PIN_HUB75_B, CG_PIN_HUB75_C,
        CG_PIN_HUB75_D, CG_PIN_HUB75_E, CG_PIN_HUB75_LAT,
        CG_PIN_HUB75_CLK,
    };

    for (size_t i = 0; i < sizeof(hub75_outputs) / sizeof(hub75_outputs[0]); ++i) {
        ESP_RETURN_ON_ERROR(configure_output(hub75_outputs[i], 0), "cg_bsp", "hub75 output init failed");
    }

    ESP_RETURN_ON_ERROR(configure_output(CG_PIN_HUB75_OE, 1), "cg_bsp", "hub75 oe init failed");
    ESP_RETURN_ON_ERROR(configure_output(CG_PIN_PMS_SET, 1), "cg_bsp", "pms set init failed");
    ESP_RETURN_ON_ERROR(configure_output(CG_PIN_PMS_RESET, 1), "cg_bsp", "pms reset init failed");
    ESP_RETURN_ON_ERROR(configure_input_pullup(CG_PIN_BEAM_1), "cg_bsp", "beam 1 init failed");
    ESP_RETURN_ON_ERROR(configure_input_pullup(CG_PIN_BEAM_2), "cg_bsp", "beam 2 init failed");

    return ESP_OK;
}

esp_err_t cg_bsp_init(void)
{
    return cg_bsp_init_safe_gpio();
}

esp_err_t cg_bsp_set_pms_active(bool active)
{
    return gpio_set_level(CG_PIN_PMS_SET, active ? 1 : 0);
}

esp_err_t cg_bsp_reset_pms(uint32_t reset_ms)
{
    ESP_RETURN_ON_ERROR(gpio_set_level(CG_PIN_PMS_RESET, 0), "cg_bsp", "pms reset low failed");
    vTaskDelay(pdMS_TO_TICKS(reset_ms));
    return gpio_set_level(CG_PIN_PMS_RESET, 1);
}

const hub75_pin_config_t *cg_bsp_get_hub75_pin_config(void)
{
    return &s_hub75_pins;
}

