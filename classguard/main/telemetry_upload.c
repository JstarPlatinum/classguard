#include "telemetry_upload.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "app_data.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "wifi_upload_config.h"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
#define UPLOAD_TASK_STACK_SIZE 8192U
#define UPLOAD_TASK_PRIORITY 5U
#define TELEMETRY_BODY_SIZE 1792U
#define VALUE_TEXT_SIZE 24U

static const char *TAG = "cg_upload";
static EventGroupHandle_t s_wifi_event_group;
static esp_netif_t *s_sta_netif;
static int s_retry_count;

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        return;
    }

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);

        if (s_retry_count < 10) {
            s_retry_count++;
            esp_wifi_connect();
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        return;
    }

    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        s_retry_count = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static esp_err_t wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();
    if (s_wifi_event_group == NULL) {
        return ESP_ERR_NO_MEM;
    }

    ESP_ERROR_CHECK(esp_netif_init());
    esp_err_t ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        return ret;
    }

    s_sta_netif = esp_netif_create_default_wifi_sta();
    if (s_sta_netif == NULL) {
        return ESP_FAIL;
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_config = {0};
    strlcpy((char *)wifi_config.sta.ssid, WIFI_UPLOAD_SSID, sizeof(wifi_config.sta.ssid));
    strlcpy((char *)wifi_config.sta.password, WIFI_UPLOAD_PASSWORD, sizeof(wifi_config.sta.password));
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE,
        pdFALSE,
        pdMS_TO_TICKS(WIFI_CONNECT_TIMEOUT_MS));

    return (bits & WIFI_CONNECTED_BIT) ? ESP_OK : ESP_ERR_TIMEOUT;
}

static void get_local_ip(char *buffer, size_t buffer_len)
{
    esp_netif_ip_info_t ip_info;
    esp_err_t err = (s_sta_netif != NULL) ? esp_netif_get_ip_info(s_sta_netif, &ip_info) : ESP_FAIL;
    if (err != ESP_OK) {
        snprintf(buffer, buffer_len, "0.0.0.0");
        return;
    }

    snprintf(buffer, buffer_len, IPSTR, IP2STR(&ip_info.ip));
}

static int get_wifi_rssi(void)
{
    wifi_ap_record_t ap_info;
    esp_err_t err = esp_wifi_sta_get_ap_info(&ap_info);
    return (err == ESP_OK) ? ap_info.rssi : -127;
}

static void format_uint_or_null(char *dst, size_t dst_len, bool valid, uint32_t value)
{
    if (valid) {
        (void)snprintf(dst, dst_len, "%lu", (unsigned long)value);
    } else {
        strlcpy(dst, "null", dst_len);
    }
}

static void format_float_or_null(char *dst, size_t dst_len, bool valid, float value, int decimals)
{
    if (valid) {
        (void)snprintf(dst, dst_len, "%.*f", decimals, (double)value);
    } else {
        strlcpy(dst, "null", dst_len);
    }
}

static esp_err_t build_telemetry_json(char *body, size_t body_size, const cg_app_sensor_snapshot_t *snapshot)
{
    char ip_addr[16];
    char scd41_co2[VALUE_TEXT_SIZE];
    char scd41_temp[VALUE_TEXT_SIZE];
    char scd41_humidity[VALUE_TEXT_SIZE];
    char sht35_temp[VALUE_TEXT_SIZE];
    char sht35_humidity[VALUE_TEXT_SIZE];
    char pm1_0[VALUE_TEXT_SIZE];
    char pm2_5[VALUE_TEXT_SIZE];
    char pm10[VALUE_TEXT_SIZE];
    char mlx_min[VALUE_TEXT_SIZE];
    char mlx_max[VALUE_TEXT_SIZE];
    char mlx_avg[VALUE_TEXT_SIZE];
    char occupancy_ratio[VALUE_TEXT_SIZE];
    char occupancy_heat_score[VALUE_TEXT_SIZE];
    char occupancy_score[VALUE_TEXT_SIZE];
    char occupancy_max_delta[VALUE_TEXT_SIZE];

    bool scd41_valid = snapshot->environment.scd41_valid;
    bool sht35_valid = snapshot->environment.sht35_valid;
    bool pms_valid = snapshot->pm.valid;
    bool thermal_valid = snapshot->thermal.valid;
    bool occupancy_valid = snapshot->occupancy.valid;

    get_local_ip(ip_addr, sizeof(ip_addr));
    format_uint_or_null(scd41_co2, sizeof(scd41_co2), scd41_valid, snapshot->environment.scd41_co2_ppm);
    format_float_or_null(scd41_temp, sizeof(scd41_temp), scd41_valid, snapshot->environment.scd41_temperature_c, 2);
    format_float_or_null(scd41_humidity, sizeof(scd41_humidity), scd41_valid, snapshot->environment.scd41_humidity_rh, 2);
    format_float_or_null(sht35_temp, sizeof(sht35_temp), sht35_valid, snapshot->environment.sht35_temperature_c, 2);
    format_float_or_null(sht35_humidity, sizeof(sht35_humidity), sht35_valid, snapshot->environment.sht35_humidity_rh, 2);
    format_uint_or_null(pm1_0, sizeof(pm1_0), pms_valid, snapshot->pm.pm1_0_atm);
    format_uint_or_null(pm2_5, sizeof(pm2_5), pms_valid, snapshot->pm.pm2_5_atm);
    format_uint_or_null(pm10, sizeof(pm10), pms_valid, snapshot->pm.pm10_atm);
    format_float_or_null(mlx_min, sizeof(mlx_min), thermal_valid, snapshot->thermal.min_temp_c, 2);
    format_float_or_null(mlx_max, sizeof(mlx_max), thermal_valid, snapshot->thermal.max_temp_c, 2);
    format_float_or_null(mlx_avg, sizeof(mlx_avg), thermal_valid, snapshot->thermal.avg_temp_c, 2);
    format_float_or_null(occupancy_ratio, sizeof(occupancy_ratio), occupancy_valid, snapshot->occupancy.occupancy_ratio, 4);
    format_float_or_null(occupancy_heat_score,
                         sizeof(occupancy_heat_score),
                         occupancy_valid,
                         snapshot->occupancy.occupancy_heat_score,
                         4);
    format_float_or_null(occupancy_score, sizeof(occupancy_score), occupancy_valid, snapshot->occupancy.occupancy_score, 4);
    format_float_or_null(occupancy_max_delta, sizeof(occupancy_max_delta), occupancy_valid, snapshot->occupancy.max_delta, 2);

    uint32_t uptime_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
    bool sensor_ok = snapshot->sensor_error_mask == 0;

    int body_len = snprintf(
        body,
        body_size,
        "{"
        "\"device_id\":\"%s\","
        "\"firmware\":\"classguard-formal-v0.1.0\","
        "\"timestamp\":0,"
        "\"uptime_ms\":%lu,"
        "\"wifi\":{\"rssi\":%d,\"ip\":\"%s\"},"
        "\"sensors\":{"
        "\"scd41\":{\"co2_ppm\":%s,\"temperature_c\":%s,\"humidity_percent\":%s},"
        "\"sht35\":{\"temperature_c\":%s,\"humidity_percent\":%s},"
        "\"pms5003\":{\"pm1_0\":%s,\"pm2_5\":%s,\"pm10\":%s},"
        "\"mlx90640\":{"
        "\"temp_min_c\":%s,\"temp_max_c\":%s,\"temp_avg_c\":%s,"
        "\"occupied\":%s,\"occupancy_ratio\":%s,\"occupancy_heat_score\":%s,\"occupancy_score\":%s,"
        "\"state\":%d,\"max_delta\":%s,\"valid_pixels\":%u,\"max_region_area\":%u"
        "}"
        "},"
        "\"status\":{\"sensor_ok\":%s,\"error_code\":%lu,\"error_message\":\"%s\"}"
        "}",
        WIFI_UPLOAD_DEVICE_ID,
        (unsigned long)uptime_ms,
        get_wifi_rssi(),
        ip_addr,
        scd41_co2,
        scd41_temp,
        scd41_humidity,
        sht35_temp,
        sht35_humidity,
        pm1_0,
        pm2_5,
        pm10,
        mlx_min,
        mlx_max,
        mlx_avg,
        occupancy_valid ? (snapshot->occupancy.occupied ? "true" : "false") : "null",
        occupancy_ratio,
        occupancy_heat_score,
        occupancy_score,
        occupancy_valid ? (int)snapshot->occupancy.state : -1,
        occupancy_max_delta,
        occupancy_valid ? (unsigned int)snapshot->occupancy.valid_pixels : 0U,
        occupancy_valid ? (unsigned int)snapshot->occupancy.max_region_area : 0U,
        sensor_ok ? "true" : "false",
        (unsigned long)snapshot->sensor_error_mask,
        snapshot->error_message);

    return (body_len >= 0 && body_len < (int)body_size) ? ESP_OK : ESP_ERR_NO_MEM;
}

static esp_err_t post_telemetry(const cg_app_sensor_snapshot_t *snapshot)
{
    char body[TELEMETRY_BODY_SIZE];
    esp_err_t ret = build_telemetry_json(body, sizeof(body), snapshot);
    if (ret != ESP_OK) {
        return ret;
    }

    esp_http_client_config_t config = {
        .url = WIFI_UPLOAD_SERVER_URL,
        .method = HTTP_METHOD_POST,
        .timeout_ms = WIFI_UPLOAD_HTTP_TIMEOUT_MS,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        return ESP_FAIL;
    }

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "X-Device-Token", WIFI_UPLOAD_DEVICE_TOKEN);
    esp_http_client_set_post_field(client, body, strlen(body));

    ret = esp_http_client_perform(client);
    if (ret == ESP_OK && esp_http_client_get_status_code(client) != 200) {
        ret = ESP_FAIL;
    }

    esp_http_client_cleanup(client);
    return ret;
}

static void upload_task(void *arg)
{
    (void)arg;

    for (;;) {
        EventBits_t bits = xEventGroupGetBits(s_wifi_event_group);
        if ((bits & WIFI_CONNECTED_BIT) != 0) {
            cg_app_sensor_snapshot_t snapshot;
            if (cg_app_data_get_latest(&snapshot)) {
                esp_err_t ret = post_telemetry(&snapshot);
                if (ret != ESP_OK) {
                    ESP_LOGW(TAG, "telemetry upload failed: %s", esp_err_to_name(ret));
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(WIFI_UPLOAD_PERIOD_MS));
    }
}

esp_err_t cg_telemetry_upload_start(void)
{
    esp_log_level_set("wifi", ESP_LOG_WARN);
    esp_log_level_set("esp_http_client", ESP_LOG_WARN);

    esp_err_t ret = wifi_init_sta();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "initial WiFi connection failed: %s", esp_err_to_name(ret));
    }

    BaseType_t ok = xTaskCreate(upload_task,
                                "telemetry_upload",
                                UPLOAD_TASK_STACK_SIZE,
                                NULL,
                                UPLOAD_TASK_PRIORITY,
                                NULL);
    return ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
