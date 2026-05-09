# ClassGuard Local Web Dashboard

This folder contains the first-stage local web prototype:

- `app.py`: FastAPI server
- `database.py`: SQLite init, insert, and query helpers
- `models.py`: telemetry JSON validation models
- `static/`: browser dashboard files
- `mock_sender.py`: PC-side ESP32 upload simulator
- `data/telemetry*.db`: generated automatically after the server starts

## Install

```powershell
cd feature\web_dashboard\server
python -m pip install -r requirements.txt
```

## Start Server

```powershell
cd g:\python\chuangke\classguard\feature\web_dashboard\server
g:\python\chuangke\classguard\.venv\Scripts\python.exe -m uvicorn app:app --host 0.0.0.0 --port 8000
```

```powershell
uvicorn app:app --host 0.0.0.0 --port 8000
```

```powershell test_recieve_data
cd g:\python\chuangke\classguard\feature\web_dashboard\server
g:\python\chuangke\classguard\.venv\Scripts\python.exe mock_sender.py
```

```powershell
Stop-Process -Id 22324
```

Open the dashboard:

```text
http://127.0.0.1:8000
```

When ESP32 is added later, use the PC WLAN IPv4 address instead of `127.0.0.1`.
For example:

```text
http://192.168.0.67:8000/api/telemetry
```

## Send Mock Data

Open a second terminal:

```powershell
cd feature\web_dashboard\server
python mock_sender.py
```

Send only 10 packets for a quick test:

```powershell
python mock_sender.py --count 10 --interval 0.5
```

The server expects this request header:

```http
X-Device-Token: classguard_test_token_001
```

The allowed first device id is:

```text
esp32s3_node_001
```

## Database Storage Policy

The server stores telemetry in SQLite files under `data/`.

Current defaults in `config.py`:

```python
DATABASE_FILE_MAX_BYTES = 100 * 1024 * 1024
DATABASE_DIR_MAX_BYTES = int(1.2 * 1024 * 1024 * 1024)
```

Behavior:

- A single database file is used until it reaches about 100 MB.
- After that, the next write creates a new file such as `telemetry_20260509_120101.db`.
- `/api/latest` and `/api/history` read across all retained database files.
- When the whole `data` folder grows beyond 1.2 GB, the oldest database file is deleted first.
- The database currently being written is not deleted during cleanup.

## API

- `POST /api/telemetry`: receive telemetry JSON
- `GET /api/latest`: latest record
- `GET /api/history?limit=120`: recent records
- `GET /events`: realtime SSE stream
- `GET /`: dashboard page
