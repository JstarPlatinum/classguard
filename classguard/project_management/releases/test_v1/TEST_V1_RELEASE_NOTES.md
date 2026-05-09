# ClassGuard Test V1 Freeze

Freeze time: 2026-05-09 15:23:44 +08:00

Git base revision at freeze: `d552d22`

## Scope

This freeze marks the first verified local Web + ESP32 WiFi upload test version.

Included source areas:

- `feature/web_dashboard/server/`
- `main/`
- `test/wifi_upload_fixed_data_test/`

Runtime data is not treated as source for this freeze:

- `feature/web_dashboard/server/data/telemetry.db`
- `build/`
- `.venv/`

## Verified Functions

- Local FastAPI server receives telemetry through `POST /api/telemetry`.
- SQLite stores incoming telemetry and supports latest/history queries.
- Web dashboard displays latest values and history charts.
- `mock_sender.py` can simulate ESP32 telemetry upload.
- ESP32 test program connects to WiFi and uploads fixed JSON every 2 seconds.
- ESP32 upload JSON, device id, and token match the local Web server.
- Root ESP-IDF project builds with the WiFi upload test as `main/main.c`.

## Run Web Server

```powershell
cd g:\python\chuangke\classguard\feature\web_dashboard\server
g:\python\chuangke\classguard\.venv\Scripts\python.exe -m uvicorn app:app --host 0.0.0.0 --port 8000
```

Dashboard:

```text
http://127.0.0.1:8000
```

## Run Mock Sender

```powershell
cd g:\python\chuangke\classguard\feature\web_dashboard\server
g:\python\chuangke\classguard\.venv\Scripts\python.exe mock_sender.py
```

## ESP32 Build

Before flashing, edit:

```text
main/wifi_upload_config.h
```

Then build from repo root:

```powershell
idf.py build
idf.py flash monitor
```

Expected serial output:

```text
[wifi] got ip: 192.168.x.x
[http] #0 POST http://<PC-IP>:8000/api/telemetry
[http] #0 status=200
```

## Source Integrity

Use `SHA256SUMS.txt` in this folder to compare whether a file still matches this frozen test v1 source state.
