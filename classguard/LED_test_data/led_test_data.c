/*
 * LED display fixed test data interface:
 *   This file provides stable ESP32-side SHT35, SCD41, and PMS5003 data for
 *   LED matrix display development. The output structs and field names match
 *   the real sensor drivers, so the LED layer can later switch data sources
 *   without changing display-side field access.
 */
#include "led_test_data.h"

#include <string.h>

#define LED_TEST_TIMESTAMP_MS 123456U

#define LED_TEST_SHT35_TEMPERATURE_C 25.10f
#define LED_TEST_SHT35_HUMIDITY_RH 49.20f

#define LED_TEST_SCD41_CO2_PPM 715U
#define LED_TEST_SCD41_TEMPERATURE_C 25.40f
#define LED_TEST_SCD41_HUMIDITY_RH 48.70f

#define LED_TEST_PMS_PM1_0_CF1 5U
#define LED_TEST_PMS_PM2_5_CF1 12U
#define LED_TEST_PMS_PM10_CF1 18U
#define LED_TEST_PMS_PM1_0_ATM 6U
#define LED_TEST_PMS_PM2_5_ATM 13U
#define LED_TEST_PMS_PM10_ATM 20U
#define LED_TEST_PMS_PARTICLES_0_3UM 1560U
#define LED_TEST_PMS_PARTICLES_0_5UM 480U
#define LED_TEST_PMS_PARTICLES_1_0UM 92U
#define LED_TEST_PMS_PARTICLES_2_5UM 16U
#define LED_TEST_PMS_PARTICLES_5_0UM 4U
#define LED_TEST_PMS_PARTICLES_10UM 1U

void led_test_data_get_sht35(environment_frame_t *out_frame)
{
    if (out_frame == NULL) {
        return;
    }

    memset(out_frame, 0, sizeof(*out_frame));
    out_frame->sht35_timestamp_ms = LED_TEST_TIMESTAMP_MS;
    out_frame->sht35_temperature_c = LED_TEST_SHT35_TEMPERATURE_C;
    out_frame->sht35_humidity_rh = LED_TEST_SHT35_HUMIDITY_RH;
    out_frame->sht35_valid = true;
}

void led_test_data_get_scd41(environment_frame_t *out_frame)
{
    if (out_frame == NULL) {
        return;
    }

    memset(out_frame, 0, sizeof(*out_frame));
    out_frame->scd41_timestamp_ms = LED_TEST_TIMESTAMP_MS;
    out_frame->scd41_co2_ppm = LED_TEST_SCD41_CO2_PPM;
    out_frame->scd41_temperature_c = LED_TEST_SCD41_TEMPERATURE_C;
    out_frame->scd41_humidity_rh = LED_TEST_SCD41_HUMIDITY_RH;
    out_frame->scd41_valid = true;
}

void led_test_data_get_environment(environment_frame_t *out_frame)
{
    if (out_frame == NULL) {
        return;
    }

    memset(out_frame, 0, sizeof(*out_frame));
    out_frame->sht35_timestamp_ms = LED_TEST_TIMESTAMP_MS;
    out_frame->sht35_temperature_c = LED_TEST_SHT35_TEMPERATURE_C;
    out_frame->sht35_humidity_rh = LED_TEST_SHT35_HUMIDITY_RH;
    out_frame->sht35_valid = true;

    out_frame->scd41_timestamp_ms = LED_TEST_TIMESTAMP_MS;
    out_frame->scd41_co2_ppm = LED_TEST_SCD41_CO2_PPM;
    out_frame->scd41_temperature_c = LED_TEST_SCD41_TEMPERATURE_C;
    out_frame->scd41_humidity_rh = LED_TEST_SCD41_HUMIDITY_RH;
    out_frame->scd41_valid = true;
}

void led_test_data_get_pms5003(pm_frame_t *out_frame)
{
    if (out_frame == NULL) {
        return;
    }

    memset(out_frame, 0, sizeof(*out_frame));
    out_frame->timestamp_ms = LED_TEST_TIMESTAMP_MS;
    out_frame->pm1_0_cf1 = LED_TEST_PMS_PM1_0_CF1;
    out_frame->pm2_5_cf1 = LED_TEST_PMS_PM2_5_CF1;
    out_frame->pm10_cf1 = LED_TEST_PMS_PM10_CF1;
    out_frame->pm1_0_atm = LED_TEST_PMS_PM1_0_ATM;
    out_frame->pm2_5_atm = LED_TEST_PMS_PM2_5_ATM;
    out_frame->pm10_atm = LED_TEST_PMS_PM10_ATM;
    out_frame->particles_0_3um = LED_TEST_PMS_PARTICLES_0_3UM;
    out_frame->particles_0_5um = LED_TEST_PMS_PARTICLES_0_5UM;
    out_frame->particles_1_0um = LED_TEST_PMS_PARTICLES_1_0UM;
    out_frame->particles_2_5um = LED_TEST_PMS_PARTICLES_2_5UM;
    out_frame->particles_5_0um = LED_TEST_PMS_PARTICLES_5_0UM;
    out_frame->particles_10um = LED_TEST_PMS_PARTICLES_10UM;
    out_frame->frames_read = 1;
    out_frame->checksum_errors = 0;
    out_frame->valid = true;
}

void led_test_data_get_all(led_test_data_frame_t *out_frame)
{
    if (out_frame == NULL) {
        return;
    }

    memset(out_frame, 0, sizeof(*out_frame));
    led_test_data_get_environment(&out_frame->environment);
    led_test_data_get_pms5003(&out_frame->pms5003);
}
