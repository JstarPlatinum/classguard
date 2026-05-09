import json
import sqlite3
from datetime import datetime
from pathlib import Path
from typing import Any, Dict, List, Optional

import aiosqlite

from config import DATA_DIR, DATABASE_DIR_MAX_BYTES, DATABASE_FILE_MAX_BYTES, DATABASE_PREFIX
from models import TelemetryIn


CREATE_TABLE_SQL = """
CREATE TABLE IF NOT EXISTS telemetry (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    received_at TEXT NOT NULL,
    device_id TEXT NOT NULL,
    firmware TEXT,
    uptime_ms INTEGER,
    wifi_rssi INTEGER,
    wifi_ip TEXT,
    co2_ppm REAL,
    temperature_c REAL,
    humidity_percent REAL,
    pm1_0 REAL,
    pm2_5 REAL,
    pm10 REAL,
    mlx_temp_min_c REAL,
    mlx_temp_max_c REAL,
    mlx_temp_avg_c REAL,
    sensor_ok INTEGER,
    error_code INTEGER,
    error_message TEXT,
    raw_json TEXT NOT NULL
);
"""


def _db_files() -> List[Path]:
    DATA_DIR.mkdir(parents=True, exist_ok=True)
    return sorted(
        DATA_DIR.glob(f"{DATABASE_PREFIX}*.db"),
        key=lambda path: (path.stat().st_mtime, path.name),
    )


def _new_database_path(received_at: Optional[datetime] = None) -> Path:
    stamp_source = received_at or datetime.now().astimezone()
    stamp = stamp_source.strftime("%Y%m%d_%H%M%S")
    path = DATA_DIR / f"{DATABASE_PREFIX}_{stamp}.db"
    suffix = 1
    while path.exists():
        path = DATA_DIR / f"{DATABASE_PREFIX}_{stamp}_{suffix:02d}.db"
        suffix += 1
    return path


def _folder_size(path: Path) -> int:
    if not path.exists():
        return 0
    return sum(file.stat().st_size for file in path.rglob("*") if file.is_file())


def _select_active_database(received_at: Optional[datetime] = None) -> Path:
    files = _db_files()
    if not files:
        return _new_database_path(received_at)

    active = files[-1]
    if active.stat().st_size >= DATABASE_FILE_MAX_BYTES:
        return _new_database_path(received_at)
    return active


async def _ensure_database(path: Path) -> None:
    DATA_DIR.mkdir(parents=True, exist_ok=True)
    async with aiosqlite.connect(path) as db:
        await db.execute(CREATE_TABLE_SQL)
        await db.commit()


async def _cleanup_old_databases(active_path: Path) -> None:
    files = _db_files()
    if _folder_size(DATA_DIR) <= DATABASE_DIR_MAX_BYTES:
        return

    for path in files:
        if path == active_path:
            continue
        try:
            path.unlink()
        except OSError:
            continue
        if _folder_size(DATA_DIR) <= DATABASE_DIR_MAX_BYTES:
            break


def _model_to_dict(model: TelemetryIn) -> Dict[str, Any]:
    if hasattr(model, "model_dump"):
        return model.model_dump()
    return model.dict()


def _row_to_dict(row: sqlite3.Row) -> Dict[str, Any]:
    data = dict(row)
    try:
        data["raw"] = json.loads(data.pop("raw_json"))
    except (TypeError, json.JSONDecodeError):
        data["raw"] = None
    data["sensor_ok"] = bool(data["sensor_ok"]) if data["sensor_ok"] is not None else None
    return data


def _extract(payload: TelemetryIn) -> Dict[str, Any]:
    wifi = payload.wifi
    sensors = payload.sensors
    scd41 = sensors.scd41 if sensors else None
    pms5003 = sensors.pms5003 if sensors else None
    mlx90640 = sensors.mlx90640 if sensors else None
    status = payload.status

    return {
        "device_id": payload.device_id,
        "firmware": payload.firmware,
        "uptime_ms": payload.uptime_ms,
        "wifi_rssi": wifi.rssi if wifi else None,
        "wifi_ip": wifi.ip if wifi else None,
        "co2_ppm": scd41.co2_ppm if scd41 else None,
        "temperature_c": scd41.temperature_c if scd41 else None,
        "humidity_percent": scd41.humidity_percent if scd41 else None,
        "pm1_0": pms5003.pm1_0 if pms5003 else None,
        "pm2_5": pms5003.pm2_5 if pms5003 else None,
        "pm10": pms5003.pm10 if pms5003 else None,
        "mlx_temp_min_c": mlx90640.temp_min_c if mlx90640 else None,
        "mlx_temp_max_c": mlx90640.temp_max_c if mlx90640 else None,
        "mlx_temp_avg_c": mlx90640.temp_avg_c if mlx90640 else None,
        "sensor_ok": 1 if (status is None or status.sensor_ok) else 0,
        "error_code": status.error_code if status else 0,
        "error_message": status.error_message if status else "",
    }


async def init_db(path: Optional[Path] = None) -> None:
    target = path or _select_active_database()
    await _ensure_database(target)
    await _cleanup_old_databases(target)


async def insert_telemetry(payload: TelemetryIn, received_at: datetime) -> Dict[str, Any]:
    db_path = _select_active_database(received_at)
    await _ensure_database(db_path)

    raw = json.dumps(_model_to_dict(payload), ensure_ascii=False)
    values = _extract(payload)
    values["received_at"] = received_at.isoformat(timespec="seconds")
    values["raw_json"] = raw

    async with aiosqlite.connect(db_path) as db:
        db.row_factory = sqlite3.Row
        cursor = await db.execute(
            """
            INSERT INTO telemetry (
                received_at, device_id, firmware, uptime_ms, wifi_rssi, wifi_ip,
                co2_ppm, temperature_c, humidity_percent,
                pm1_0, pm2_5, pm10,
                mlx_temp_min_c, mlx_temp_max_c, mlx_temp_avg_c,
                sensor_ok, error_code, error_message, raw_json
            )
            VALUES (
                :received_at, :device_id, :firmware, :uptime_ms, :wifi_rssi, :wifi_ip,
                :co2_ppm, :temperature_c, :humidity_percent,
                :pm1_0, :pm2_5, :pm10,
                :mlx_temp_min_c, :mlx_temp_max_c, :mlx_temp_avg_c,
                :sensor_ok, :error_code, :error_message, :raw_json
            )
            """,
            values,
        )
        await db.commit()
        row_id = cursor.lastrowid
        cursor = await db.execute("SELECT * FROM telemetry WHERE id = ?", (row_id,))
        row = await cursor.fetchone()
        saved = _row_to_dict(row)
        saved["database_file"] = db_path.name

    await _cleanup_old_databases(db_path)
    return saved


async def get_latest() -> Optional[Dict[str, Any]]:
    files = list(reversed(_db_files()))
    for db_path in files:
        async with aiosqlite.connect(db_path) as db:
            db.row_factory = sqlite3.Row
            cursor = await db.execute("SELECT * FROM telemetry ORDER BY id DESC LIMIT 1")
            row = await cursor.fetchone()
            if row:
                data = _row_to_dict(row)
                data["database_file"] = db_path.name
                return data
    return None


async def get_history(limit: int) -> List[Dict[str, Any]]:
    records: List[Dict[str, Any]] = []
    for db_path in reversed(_db_files()):
        if len(records) >= limit:
            break
        async with aiosqlite.connect(db_path) as db:
            db.row_factory = sqlite3.Row
            cursor = await db.execute(
                "SELECT * FROM telemetry ORDER BY received_at DESC, id DESC LIMIT ?",
                (limit - len(records),),
            )
            rows = await cursor.fetchall()
            for row in rows:
                data = _row_to_dict(row)
                data["database_file"] = db_path.name
                records.append(data)

    records.sort(key=lambda item: (item["received_at"], item["id"]))
    return records[-limit:]
