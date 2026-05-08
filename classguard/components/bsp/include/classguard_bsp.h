#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "data_types.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t cg_bsp_init(void);
esp_err_t cg_bsp_init_safe_gpio(void);
esp_err_t cg_bsp_set_pms_active(bool active);
esp_err_t cg_bsp_reset_pms(uint32_t reset_ms);
const hub75_pin_config_t *cg_bsp_get_hub75_pin_config(void);

#ifdef __cplusplus
}
#endif
