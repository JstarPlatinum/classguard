#pragma once

/*
 * Formal telemetry upload configuration.
 *
 * SERVER_URL must use the PC WLAN IPv4 address, not 127.0.0.1.
 * Example: http://192.168.0.67:8000/api/telemetry
 */
#define WIFI_UPLOAD_SSID "your SSID"
#define WIFI_UPLOAD_PASSWORD "Your Password"

#define WIFI_UPLOAD_SERVER_URL "http://192.168.0.67:8000/api/telemetry" //your ip
#define WIFI_UPLOAD_DEVICE_ID "esp32s3_node_001"
#define WIFI_UPLOAD_DEVICE_TOKEN "classguard_test_token_001"

#define WIFI_UPLOAD_PERIOD_MS 2000
#define WIFI_CONNECT_TIMEOUT_MS 20000
#define WIFI_UPLOAD_HTTP_TIMEOUT_MS 5000
