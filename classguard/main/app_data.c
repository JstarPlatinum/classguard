#include "app_data.h"

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define SENSOR_ERROR_SLOT_COUNT 3U
#define SENSOR_ERROR_TEXT_MAX_LEN 64U

typedef struct {
    uint32_t bit;
    char text[SENSOR_ERROR_TEXT_MAX_LEN];
} sensor_error_slot_t;

static SemaphoreHandle_t s_data_mutex;
static cg_app_sensor_snapshot_t s_latest;
static sensor_error_slot_t s_sensor_errors[SENSOR_ERROR_SLOT_COUNT] = {
    {.bit = CG_SENSOR_STATUS_SHT35},
    {.bit = CG_SENSOR_STATUS_SCD41},
    {.bit = CG_SENSOR_STATUS_PMS5003},
};

static void lock_data(void)
{
    if (s_data_mutex != NULL) {
        (void)xSemaphoreTake(s_data_mutex, portMAX_DELAY);
    }
}

static void unlock_data(void)
{
    if (s_data_mutex != NULL) {
        (void)xSemaphoreGive(s_data_mutex);
    }
}

static sensor_error_slot_t *find_error_slot(uint32_t sensor_bit)
{
    for (size_t i = 0; i < SENSOR_ERROR_SLOT_COUNT; ++i) {
        if (s_sensor_errors[i].bit == sensor_bit) {
            return &s_sensor_errors[i];
        }
    }
    return NULL;
}

static void rebuild_status_message_locked(void)
{
    s_latest.error_message[0] = '\0';

    for (size_t i = 0; i < SENSOR_ERROR_SLOT_COUNT; ++i) {
        if ((s_latest.sensor_error_mask & s_sensor_errors[i].bit) == 0 || s_sensor_errors[i].text[0] == '\0') {
            continue;
        }

        size_t used = strlen(s_latest.error_message);
        if (used > 0 && used + 2 < sizeof(s_latest.error_message)) {
            strlcat(s_latest.error_message, "; ", sizeof(s_latest.error_message));
        }
        strlcat(s_latest.error_message, s_sensor_errors[i].text, sizeof(s_latest.error_message));
    }
}

void cg_app_data_init(void)
{
    memset(&s_latest, 0, sizeof(s_latest));
    for (size_t i = 0; i < SENSOR_ERROR_SLOT_COUNT; ++i) {
        s_sensor_errors[i].text[0] = '\0';
    }

    if (s_data_mutex == NULL) {
        s_data_mutex = xSemaphoreCreateMutex();
    }
}

bool cg_app_data_get_latest(cg_app_sensor_snapshot_t *out_snapshot)
{
    if (out_snapshot == NULL) {
        return false;
    }

    lock_data();
    *out_snapshot = s_latest;
    unlock_data();
    return true;
}

void cg_app_data_update_sht35(const environment_frame_t *frame)
{
    if (frame == NULL || !frame->sht35_valid) {
        return;
    }

    lock_data();
    s_latest.environment.sht35_timestamp_ms = frame->sht35_timestamp_ms;
    s_latest.environment.sht35_temperature_c = frame->sht35_temperature_c;
    s_latest.environment.sht35_humidity_rh = frame->sht35_humidity_rh;
    s_latest.environment.sht35_valid = true;
    unlock_data();
}

void cg_app_data_update_scd41(const environment_frame_t *frame)
{
    if (frame == NULL || !frame->scd41_valid) {
        return;
    }

    lock_data();
    s_latest.environment.scd41_timestamp_ms = frame->scd41_timestamp_ms;
    s_latest.environment.scd41_co2_ppm = frame->scd41_co2_ppm;
    s_latest.environment.scd41_temperature_c = frame->scd41_temperature_c;
    s_latest.environment.scd41_humidity_rh = frame->scd41_humidity_rh;
    s_latest.environment.scd41_valid = true;
    unlock_data();
}

void cg_app_data_update_pms5003(const pm_frame_t *frame)
{
    if (frame == NULL || !frame->valid) {
        return;
    }

    lock_data();
    s_latest.pm = *frame;
    unlock_data();
}

void cg_app_data_set_sensor_error(uint32_t sensor_bit, const char *sensor_name, esp_err_t error)
{
    lock_data();
    s_latest.sensor_error_mask |= sensor_bit;

    sensor_error_slot_t *slot = find_error_slot(sensor_bit);
    if (slot != NULL) {
        (void)snprintf(slot->text,
                       sizeof(slot->text),
                       "%s:%s",
                       sensor_name != NULL ? sensor_name : "sensor",
                       esp_err_to_name(error));
    }

    rebuild_status_message_locked();
    unlock_data();
}

void cg_app_data_clear_sensor_error(uint32_t sensor_bit)
{
    lock_data();
    s_latest.sensor_error_mask &= ~sensor_bit;

    sensor_error_slot_t *slot = find_error_slot(sensor_bit);
    if (slot != NULL) {
        slot->text[0] = '\0';
    }

    rebuild_status_message_locked();
    unlock_data();
}
