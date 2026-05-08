#include "pms5003.h"

#include <string.h>
#include "app_config.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pin_map.h"

static const char *TAG = "cg_pms5003";

static uint16_t read_be16(const uint8_t *bytes)
{
    return ((uint16_t)bytes[0] << 8) | bytes[1];
}

static esp_err_t configure_output(gpio_num_t pin, int level)
{
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << pin,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&cfg), TAG, "control pin config failed");
    return gpio_set_level(pin, level);
}

bool cg_pms5003_parse_frame_bytes(const uint8_t *bytes, size_t length, pm_frame_t *frame)
{
    if (bytes == NULL || frame == NULL || length < CG_PMS_FRAME_LENGTH) {
        return false;
    }

    if (bytes[0] != CG_PMS_HEADER_1 || bytes[1] != CG_PMS_HEADER_2) {
        return false;
    }

    uint16_t payload_length = read_be16(&bytes[2]);
    if (payload_length != CG_PMS_FRAME_PAYLOAD_LENGTH) {
        return false;
    }

    uint16_t checksum = 0;
    for (size_t i = 0; i < CG_PMS_FRAME_LENGTH - 2; ++i) {
        checksum = (uint16_t)(checksum + bytes[i]);
    }

    if (checksum != read_be16(&bytes[CG_PMS_FRAME_LENGTH - 2])) {
        return false;
    }

    frame->timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
    frame->pm1_0_cf1 = read_be16(&bytes[4]);
    frame->pm2_5_cf1 = read_be16(&bytes[6]);
    frame->pm10_cf1 = read_be16(&bytes[8]);
    frame->pm1_0_atm = read_be16(&bytes[10]);
    frame->pm2_5_atm = read_be16(&bytes[12]);
    frame->pm10_atm = read_be16(&bytes[14]);
    frame->particles_0_3um = read_be16(&bytes[16]);
    frame->particles_0_5um = read_be16(&bytes[18]);
    frame->particles_1_0um = read_be16(&bytes[20]);
    frame->particles_2_5um = read_be16(&bytes[22]);
    frame->particles_5_0um = read_be16(&bytes[24]);
    frame->particles_10um = read_be16(&bytes[26]);
    frame->valid = true;
    return true;
}

esp_err_t cg_pms5003_init(cg_pms5003_t *dev)
{
    if (dev == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(dev, 0, sizeof(*dev));
    dev->uart_port = CG_UART_PMS_PORT;
    dev->rx_pin = CG_PIN_PMS_RX_FROM_SENSOR;
    dev->tx_pin = CG_PIN_PMS_TX_TO_SENSOR;
    dev->set_pin = CG_PIN_PMS_SET;
    dev->reset_pin = CG_PIN_PMS_RESET;
    dev->state = CG_DRIVER_STATE_UNINITIALIZED;

    ESP_RETURN_ON_ERROR(configure_output(dev->set_pin, 1), TAG, "set pin init failed");
    ESP_RETURN_ON_ERROR(configure_output(dev->reset_pin, 1), TAG, "reset pin init failed");

    uart_config_t uart_config = {
        .baud_rate = CG_PMS_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };

    ESP_RETURN_ON_ERROR(uart_param_config(dev->uart_port, &uart_config), TAG, "uart param config failed");
    ESP_RETURN_ON_ERROR(uart_set_pin(dev->uart_port, dev->tx_pin, dev->rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE), TAG, "uart set pin failed");
    esp_err_t ret = uart_driver_install(dev->uart_port, CG_PMS_UART_RX_BUFFER_SIZE, 0, 0, NULL, 0);
    if (ret != ESP_ERR_INVALID_STATE) {
        ESP_RETURN_ON_ERROR(ret, TAG, "uart driver install failed");
    }

    dev->state = CG_DRIVER_STATE_READY;
    return ESP_OK;
}

esp_err_t cg_pms5003_set_active(cg_pms5003_t *dev, bool active)
{
    if (dev == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return gpio_set_level(dev->set_pin, active ? 1 : 0);
}

esp_err_t cg_pms5003_reset(cg_pms5003_t *dev, uint32_t reset_ms)
{
    if (dev == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    ESP_RETURN_ON_ERROR(gpio_set_level(dev->reset_pin, 0), TAG, "reset low failed");
    vTaskDelay(pdMS_TO_TICKS(reset_ms));
    return gpio_set_level(dev->reset_pin, 1);
}

esp_err_t cg_pms5003_read_frame(cg_pms5003_t *dev, pm_frame_t *frame, TickType_t timeout_ticks)
{
    if (dev == NULL || frame == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t bytes[CG_PMS_FRAME_LENGTH];
    size_t index = 0;
    int64_t start_us = esp_timer_get_time();
    int64_t timeout_us = (int64_t)timeout_ticks * portTICK_PERIOD_MS * 1000LL;
    if (timeout_us <= 0) {
        timeout_us = 1000LL;
    }

    while ((esp_timer_get_time() - start_us) < timeout_us) {
        uint8_t byte = 0;
        int read = uart_read_bytes(dev->uart_port, &byte, 1, pdMS_TO_TICKS(20));
        if (read <= 0) {
            continue;
        }

        if (index == 0 && byte != CG_PMS_HEADER_1) {
            continue;
        }
        if (index == 1 && byte != CG_PMS_HEADER_2) {
            index = 0;
            continue;
        }

        bytes[index++] = byte;
        if (index == CG_PMS_FRAME_LENGTH) {
            bool parsed = cg_pms5003_parse_frame_bytes(bytes, sizeof(bytes), frame);
            if (parsed) {
                dev->frames_read++;
                frame->frames_read = dev->frames_read;
                frame->checksum_errors = dev->checksum_errors;
                return ESP_OK;
            }
            dev->checksum_errors++;
            index = 0;
        }
    }

    frame->valid = false;
    return ESP_ERR_TIMEOUT;
}
