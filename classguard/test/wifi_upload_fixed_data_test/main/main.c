#include <stdio.h>
#include <string.h>

#include "esp_err.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "wifi_upload_config.h"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static EventGroupHandle_t s_wifi_event_group;
static esp_netif_t *s_sta_netif;
static int s_retry_count;

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        printf("[wifi] STA start, connecting to SSID: %s\n", WIFI_UPLOAD_SSID);
        esp_wifi_connect();
        return;
    }

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t *)event_data;
        printf("[wifi] disconnected, reason=%d, retry=%d\n", event->reason, s_retry_count);
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);

        if (s_retry_count < 10) {
            s_retry_count++;
            esp_wifi_connect();
            printf("[wifi] reconnecting...\n");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            printf("[wifi] connect failed after retries\n");
        }
        return;
    }

    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        s_retry_count = 0;
        printf("[wifi] got ip: " IPSTR "\n", IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static esp_err_t wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();
    if (s_wifi_event_group == NULL) {
        printf("[wifi] failed to create event group\n");
        return ESP_ERR_NO_MEM;
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    s_sta_netif = esp_netif_create_default_wifi_sta();
    if (s_sta_netif == NULL) {
        printf("[wifi] failed to create default STA netif\n");
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

    printf("[wifi] waiting for connection, timeout=%d ms\n", WIFI_CONNECT_TIMEOUT_MS);
    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE,
        pdFALSE,
        pdMS_TO_TICKS(WIFI_CONNECT_TIMEOUT_MS));

    if (bits & WIFI_CONNECTED_BIT) {
        printf("[wifi] connected\n");
        return ESP_OK;
    }

    printf("[wifi] connection timeout or failed\n");
    return ESP_FAIL;
}

static void get_local_ip(char *buffer, size_t buffer_len)
{
    esp_netif_ip_info_t ip_info;
    esp_err_t err = esp_netif_get_ip_info(s_sta_netif, &ip_info);
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
    if (err != ESP_OK) {
        return -127;
    }
    return ap_info.rssi;
}

static esp_err_t post_fixed_telemetry(uint32_t sequence)
{
    char ip_addr[16];
    get_local_ip(ip_addr, sizeof(ip_addr));

    int rssi = get_wifi_rssi();
    uint32_t uptime_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);

    char body[1024];
    int body_len = snprintf(
        body,
        sizeof(body),
        "{"
        "\"device_id\":\"%s\","
        "\"firmware\":\"wifi-upload-test-v0.1.0\","
        "\"timestamp\":0,"
        "\"uptime_ms\":%lu,"
        "\"wifi\":{\"rssi\":%d,\"ip\":\"%s\"},"
        "\"sensors\":{"
        "\"scd41\":{\"co2_ppm\":600,\"temperature_c\":25.0,\"humidity_percent\":50.0},"
        "\"pms5003\":{\"pm1_0\":8,\"pm2_5\":12,\"pm10\":18},"
        "\"mlx90640\":{\"frame_rate\":16,\"temp_min_c\":23.5,\"temp_max_c\":30.2,\"temp_avg_c\":26.1}"
        "},"
        "\"status\":{\"sensor_ok\":true,\"error_code\":0,\"error_message\":\"\"}"
        "}",
        WIFI_UPLOAD_DEVICE_ID,
        (unsigned long)uptime_ms,
        rssi,
        ip_addr);

    if (body_len < 0 || body_len >= (int)sizeof(body)) {
        printf("[http] JSON body buffer too small\n");
        return ESP_ERR_NO_MEM;
    }

    esp_http_client_config_t config = {
        .url = WIFI_UPLOAD_SERVER_URL,
        .method = HTTP_METHOD_POST,
        .timeout_ms = WIFI_UPLOAD_HTTP_TIMEOUT_MS,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        printf("[http] esp_http_client_init failed\n");
        return ESP_FAIL;
    }

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "X-Device-Token", WIFI_UPLOAD_DEVICE_TOKEN);
    esp_http_client_set_post_field(client, body, body_len);

    printf("[http] #%lu POST %s\n", (unsigned long)sequence, WIFI_UPLOAD_SERVER_URL);
    printf("[http] body: %s\n", body);

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        int content_length = esp_http_client_get_content_length(client);
        printf("[http] #%lu status=%d content_length=%d\n",
               (unsigned long)sequence,
               status_code,
               content_length);
        if (status_code != 200) {
            printf("[http] server returned non-200 status, check token/device_id/body\n");
        }
    } else {
        printf("[http] #%lu request failed: %s\n", (unsigned long)sequence, esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    return err;
}

static void upload_task(void *arg)
{
    (void)arg;
    uint32_t sequence = 0;

    while (1) {
        EventBits_t bits = xEventGroupGetBits(s_wifi_event_group);
        if ((bits & WIFI_CONNECTED_BIT) == 0) {
            printf("[upload] WiFi not connected, skip this period\n");
            vTaskDelay(pdMS_TO_TICKS(WIFI_UPLOAD_PERIOD_MS));
            continue;
        }

        post_fixed_telemetry(sequence++);
        vTaskDelay(pdMS_TO_TICKS(WIFI_UPLOAD_PERIOD_MS));
    }
}

void app_main(void)
{
    printf("\n=== ClassGuard ESP32-S3 WiFi fixed upload test ===\n");
    printf("[config] server: %s\n", WIFI_UPLOAD_SERVER_URL);
    printf("[config] device_id: %s\n", WIFI_UPLOAD_DEVICE_ID);
    printf("[config] period: %d ms\n", WIFI_UPLOAD_PERIOD_MS);

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        printf("[nvs] erase old NVS and retry init\n");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    esp_log_level_set("wifi", ESP_LOG_WARN);
    esp_log_level_set("esp_http_client", ESP_LOG_INFO);

    ret = wifi_init_sta();
    if (ret != ESP_OK) {
        printf("[main] WiFi initial connection failed, upload task will keep waiting\n");
    }

    xTaskCreate(upload_task, "upload_task", 8192, NULL, 5, NULL);
}
