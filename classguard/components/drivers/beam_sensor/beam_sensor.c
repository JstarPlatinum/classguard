#include "beam_sensor.h"

#include <string.h>
#include "app_config.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "pin_map.h"

static const char *TAG = "cg_beam";
static cg_beam_sensor_t *s_beam_dev;
static int64_t s_last_event_us[2];

static cg_beam_sensor_id_t sensor_id_from_pin(gpio_num_t pin)
{
    return (pin == CG_PIN_BEAM_1) ? CG_BEAM_SENSOR_1 : CG_BEAM_SENSOR_2;
}

static void IRAM_ATTR beam_isr_handler(void *arg)
{
    cg_beam_sensor_t *dev = s_beam_dev;
    if (dev == NULL || dev->event_queue == NULL) {
        return;
    }

    gpio_num_t pin = (gpio_num_t)(intptr_t)arg;
    cg_beam_sensor_id_t sensor_id = sensor_id_from_pin(pin);
    size_t index = (sensor_id == CG_BEAM_SENSOR_1) ? 0 : 1;
    int64_t now_us = esp_timer_get_time();
    if ((now_us - s_last_event_us[index]) < dev->debounce_us) {
        return;
    }
    s_last_event_us[index] = now_us;

    int level = gpio_get_level(pin);
    beam_event_t event = {
        .timestamp_us = (uint32_t)now_us,
        .sensor_id = sensor_id,
        .edge = level ? CG_BEAM_EDGE_RISING : CG_BEAM_EDGE_FALLING,
        .level = level,
        .valid = true,
    };

    BaseType_t higher_priority_task_woken = pdFALSE;
    xQueueSendFromISR(dev->event_queue, &event, &higher_priority_task_woken);
    if (higher_priority_task_woken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

static esp_err_t configure_beam_pin(gpio_num_t pin)
{
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << pin,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    return gpio_config(&cfg);
}

esp_err_t cg_beam_sensor_init(cg_beam_sensor_t *dev)
{
    if (dev == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(dev, 0, sizeof(*dev));
    dev->sensor1_pin = CG_PIN_BEAM_1;
    dev->sensor2_pin = CG_PIN_BEAM_2;
    dev->debounce_us = CG_BEAM_DEBOUNCE_US;
    dev->active_low = true;
    dev->state = CG_DRIVER_STATE_UNINITIALIZED;
    dev->event_queue = xQueueCreate(CG_BEAM_EVENT_QUEUE_LENGTH, sizeof(beam_event_t));
    if (dev->event_queue == NULL) {
        return ESP_ERR_NO_MEM;
    }

    ESP_RETURN_ON_ERROR(configure_beam_pin(dev->sensor1_pin), TAG, "beam 1 config failed");
    ESP_RETURN_ON_ERROR(configure_beam_pin(dev->sensor2_pin), TAG, "beam 2 config failed");

    esp_err_t ret = gpio_install_isr_service(0);
    if (ret != ESP_ERR_INVALID_STATE) {
        ESP_RETURN_ON_ERROR(ret, TAG, "isr service install failed");
    }

    s_beam_dev = dev;
    s_last_event_us[0] = 0;
    s_last_event_us[1] = 0;
    ESP_RETURN_ON_ERROR(gpio_isr_handler_add(dev->sensor1_pin, beam_isr_handler, (void *)(intptr_t)dev->sensor1_pin), TAG, "beam 1 isr add failed");
    ESP_RETURN_ON_ERROR(gpio_isr_handler_add(dev->sensor2_pin, beam_isr_handler, (void *)(intptr_t)dev->sensor2_pin), TAG, "beam 2 isr add failed");

    dev->state = CG_DRIVER_STATE_READY;
    return ESP_OK;
}

esp_err_t cg_beam_sensor_deinit(cg_beam_sensor_t *dev)
{
    if (dev == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    gpio_isr_handler_remove(dev->sensor1_pin);
    gpio_isr_handler_remove(dev->sensor2_pin);
    if (dev->event_queue != NULL) {
        vQueueDelete(dev->event_queue);
        dev->event_queue = NULL;
    }
    if (s_beam_dev == dev) {
        s_beam_dev = NULL;
    }
    dev->state = CG_DRIVER_STATE_UNINITIALIZED;
    return ESP_OK;
}

bool cg_beam_sensor_read_event(cg_beam_sensor_t *dev, beam_event_t *event, TickType_t timeout_ticks)
{
    if (dev == NULL || event == NULL || dev->event_queue == NULL) {
        return false;
    }
    return xQueueReceive(dev->event_queue, event, timeout_ticks) == pdTRUE;
}
