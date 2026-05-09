#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "data_types.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CG_SENSOR_STATUS_SHT35 0x01U
#define CG_SENSOR_STATUS_SCD41 0x02U
#define CG_SENSOR_STATUS_PMS5003 0x04U

#define CG_APP_STATUS_MESSAGE_MAX_LEN 192U

typedef struct {
    environment_frame_t environment;
    pm_frame_t pm;
    uint32_t sensor_error_mask;
    char error_message[CG_APP_STATUS_MESSAGE_MAX_LEN];
} cg_app_sensor_snapshot_t;

void cg_app_data_init(void);
bool cg_app_data_get_latest(cg_app_sensor_snapshot_t *out_snapshot);

void cg_app_data_update_sht35(const environment_frame_t *frame);
void cg_app_data_update_scd41(const environment_frame_t *frame);
void cg_app_data_update_pms5003(const pm_frame_t *frame);

void cg_app_data_set_sensor_error(uint32_t sensor_bit, const char *sensor_name, esp_err_t error);
void cg_app_data_clear_sensor_error(uint32_t sensor_bit);

#ifdef __cplusplus
}
#endif
