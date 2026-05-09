# ESP32-S3 WiFi Fixed Upload Test

This is a standalone ESP-IDF test project. It connects ESP32-S3 to WiFi and sends one fixed telemetry JSON packet every 2 seconds to the local FastAPI server.

## Configure

Edit:

```text
main/wifi_upload_config.h
```

Set:

```c
#define WIFI_UPLOAD_SSID "YOUR_WIFI_SSID"
#define WIFI_UPLOAD_PASSWORD "YOUR_WIFI_PASSWORD"
#define WIFI_UPLOAD_SERVER_URL "http://192.168.0.67:8000/api/telemetry"
```

Use the PC WLAN IPv4 address in `WIFI_UPLOAD_SERVER_URL`. Do not use `127.0.0.1`.

The default token and device id match the local web dashboard:

```c
#define WIFI_UPLOAD_DEVICE_ID "esp32s3_node_001"
#define WIFI_UPLOAD_DEVICE_TOKEN "classguard_test_token_001"
```

## Build And Flash

Run from this folder:

```powershell
idf.py set-target esp32s3
idf.py build
idf.py flash monitor
```

Expected serial output includes:

```text
[wifi] got ip: 192.168.x.x
[http] #0 POST http://192.168.0.67:8000/api/telemetry
[http] #0 status=200
```

## Troubleshooting

- `status=401`: `X-Device-Token` does not match the server config.
- `status=403`: `device_id` is not in the server allow list.
- `status=422`: JSON format does not match the FastAPI model.
- HTTP connection errors: check PC IP, firewall, same WiFi, and whether FastAPI listens on `0.0.0.0:8000`.
- No WiFi IP: check SSID/password and whether the WiFi supports ESP32 2.4 GHz connection.
