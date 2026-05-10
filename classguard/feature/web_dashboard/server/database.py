import json
import sqlite3
from datetime import datetime
from pathlib import Path
from typing import Any, Dict, List, Optional

import aiosqlite

from config import DATA_DIR, DATABASE_DIR_MAX_BYTES, DATABASE_FILE_MAX_BYTES, DATABASE_PREFIX
from models import TelemetryIn


LEVEL_LABELS = {
    "excellent": "优秀",
    "good": "良好",
    "fair": "一般",
    "poor": "较差",
    "severe": "严重",
    "no_data": "无数据",
}

REASON_LABELS = {
    "co2": "CO2偏高",
    "pm2_5": "PM2.5偏高",
    "pm10": "PM10偏高",
    "pm1_0": "PM1.0偏高",
    "temperature": "温度不适",
    "humidity": "湿度不适",
    "occupancy": "占有率较高",
}

REDLINE_LABELS = {
    "co2_gt_1000": "CO2 > 1000 ppm",
    "co2_gt_1500": "CO2 > 1500 ppm",
    "co2_gt_2000": "CO2 > 2000 ppm",
    "co2_gt_3000": "CO2 > 3000 ppm",
    "pm25_gt_75": "PM2.5 > 75 ug/m3",
    "pm25_gt_150": "PM2.5 > 150 ug/m3",
    "pm10_gt_150": "PM10 > 150 ug/m3",
    "pm10_gt_300": "PM10 > 300 ug/m3",
    "pm1_gt_50": "PM1.0 > 50 ug/m3",
    "temperature_gt_30": "温度 > 30 C",
    "temperature_gt_32": "温度 > 32 C",
    "humidity_gt_80": "湿度 > 80%RH",
    "humidity_gt_85": "湿度 > 85%RH",
    "humidity_lt_25": "湿度 < 25%RH",
    "humidity_lt_20": "湿度 < 20%RH",
    "occupancy_ge_65": "占有率 >= 65%",
    "occupancy_ge_80": "占有率 >= 80%",
    "occupancy_ge_90": "占有率 >= 90%",
    "occupancy_ge_95": "占有率 >= 95%",
}

ACTION_MESSAGES = {
    "no_data": "暂无有效空气质量数据。",
    "keep_monitoring": "各项指标整体稳定，可以维持当前运行状态。",
    "ventilation": "当前主要问题是CO2偏高，建议立即开窗或开启新风，并适当降低占有率。",
    "early_ventilation": "当前占有率较高，CO2可能快速上升，建议提前开启新风或保持通风。",
    "filtered_ventilation": "CO2偏高且室外颗粒物可能较高，建议优先使用带过滤的新风系统。",
    "purify_air": "当前主要问题是颗粒物偏高，建议开启空气净化器，减少扬尘和人员活动。",
    "cooling": "当前温度偏高，建议降温并配合通风。",
    "dehumidify": "当前湿度偏高，存在潮湿和霉菌风险，建议除湿并检查水汽来源。",
    "improve_by_reason": "存在部分异常指标，建议根据主要影响因素进行处理。",
}


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
    sht35_temperature_c REAL,
    sht35_humidity_percent REAL,
    pm1_0 REAL,
    pm2_5 REAL,
    pm10 REAL,
    mlx_temp_min_c REAL,
    mlx_temp_max_c REAL,
    mlx_temp_avg_c REAL,
    mlx_occupied INTEGER,
    mlx_occupancy_ratio REAL,
    mlx_occupancy_heat_score REAL,
    mlx_occupancy_score REAL,
    mlx_occupancy_state INTEGER,
    mlx_max_delta REAL,
    mlx_valid_pixels INTEGER,
    mlx_max_region_area INTEGER,
    aq_weighted_score REAL,
    aq_score REAL,
    aq_level TEXT,
    aq_action TEXT,
    aq_message TEXT,
    aq_redlines TEXT,
    aq_main_reasons TEXT,
    sensor_ok INTEGER,
    error_code INTEGER,
    error_message TEXT,
    raw_json TEXT NOT NULL
);
"""


SCHEMA_MIGRATIONS = {
    "sht35_temperature_c": "ALTER TABLE telemetry ADD COLUMN sht35_temperature_c REAL",
    "sht35_humidity_percent": "ALTER TABLE telemetry ADD COLUMN sht35_humidity_percent REAL",
    "mlx_occupied": "ALTER TABLE telemetry ADD COLUMN mlx_occupied INTEGER",
    "mlx_occupancy_ratio": "ALTER TABLE telemetry ADD COLUMN mlx_occupancy_ratio REAL",
    "mlx_occupancy_heat_score": "ALTER TABLE telemetry ADD COLUMN mlx_occupancy_heat_score REAL",
    "mlx_occupancy_score": "ALTER TABLE telemetry ADD COLUMN mlx_occupancy_score REAL",
    "mlx_occupancy_state": "ALTER TABLE telemetry ADD COLUMN mlx_occupancy_state INTEGER",
    "mlx_max_delta": "ALTER TABLE telemetry ADD COLUMN mlx_max_delta REAL",
    "mlx_valid_pixels": "ALTER TABLE telemetry ADD COLUMN mlx_valid_pixels INTEGER",
    "mlx_max_region_area": "ALTER TABLE telemetry ADD COLUMN mlx_max_region_area INTEGER",
    "aq_weighted_score": "ALTER TABLE telemetry ADD COLUMN aq_weighted_score REAL",
    "aq_score": "ALTER TABLE telemetry ADD COLUMN aq_score REAL",
    "aq_level": "ALTER TABLE telemetry ADD COLUMN aq_level TEXT",
    "aq_action": "ALTER TABLE telemetry ADD COLUMN aq_action TEXT",
    "aq_message": "ALTER TABLE telemetry ADD COLUMN aq_message TEXT",
    "aq_redlines": "ALTER TABLE telemetry ADD COLUMN aq_redlines TEXT",
    "aq_main_reasons": "ALTER TABLE telemetry ADD COLUMN aq_main_reasons TEXT",
}


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
        cursor = await db.execute("PRAGMA table_info(telemetry)")
        existing_columns = {row[1] for row in await cursor.fetchall()}
        for column, sql in SCHEMA_MIGRATIONS.items():
            if column not in existing_columns:
                await db.execute(sql)
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
    data["mlx_occupied"] = bool(data["mlx_occupied"]) if data.get("mlx_occupied") is not None else None
    for key in ("aq_redlines", "aq_main_reasons"):
        try:
            data[key] = json.loads(data[key]) if data.get(key) else []
        except (TypeError, json.JSONDecodeError):
            data[key] = []
    return data


def _labels(codes: List[str], mapping: Dict[str, str]) -> List[str]:
    return [mapping.get(code, code) for code in codes]


def _build_aq_message(air_quality) -> str:
    if not air_quality:
        return ""
    if air_quality.message:
        return air_quality.message

    level_code = air_quality.level_code or air_quality.level or "no_data"
    level = LEVEL_LABELS.get(level_code, air_quality.level or level_code)
    score = f"{air_quality.score:.0f}" if air_quality.score is not None else "--"
    action = air_quality.action or "no_data"
    base = f"综合评分 {score}，空气质量{level}。"
    message = ACTION_MESSAGES.get(action, ACTION_MESSAGES["improve_by_reason"])

    if action == "improve_by_reason" and air_quality.main_reasons:
        reasons = "、".join(_labels(air_quality.main_reasons, REASON_LABELS))
        message = f"主要影响因素为：{reasons}。建议根据异常项进行处理。"
    return base + message


def _extract(payload: TelemetryIn) -> Dict[str, Any]:
    wifi = payload.wifi
    sensors = payload.sensors
    scd41 = sensors.scd41 if sensors else None
    sht35 = sensors.sht35 if sensors else None
    pms5003 = sensors.pms5003 if sensors else None
    mlx90640 = sensors.mlx90640 if sensors else None
    air_quality = payload.air_quality
    aq_redline_codes = air_quality.redlines if air_quality else []
    aq_reason_codes = air_quality.main_reasons if air_quality else []
    aq_level_code = (air_quality.level_code or air_quality.level) if air_quality else None
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
        "sht35_temperature_c": sht35.temperature_c if sht35 else None,
        "sht35_humidity_percent": sht35.humidity_percent if sht35 else None,
        "pm1_0": pms5003.pm1_0 if pms5003 else None,
        "pm2_5": pms5003.pm2_5 if pms5003 else None,
        "pm10": pms5003.pm10 if pms5003 else None,
        "mlx_temp_min_c": mlx90640.temp_min_c if mlx90640 else None,
        "mlx_temp_max_c": mlx90640.temp_max_c if mlx90640 else None,
        "mlx_temp_avg_c": mlx90640.temp_avg_c if mlx90640 else None,
        "mlx_occupied": 1 if (mlx90640 and mlx90640.occupied is True) else 0 if (mlx90640 and mlx90640.occupied is False) else None,
        "mlx_occupancy_ratio": mlx90640.occupancy_ratio if mlx90640 else None,
        "mlx_occupancy_heat_score": mlx90640.occupancy_heat_score if mlx90640 else None,
        "mlx_occupancy_score": mlx90640.occupancy_score if mlx90640 else None,
        "mlx_occupancy_state": mlx90640.state if mlx90640 else None,
        "mlx_max_delta": mlx90640.max_delta if mlx90640 else None,
        "mlx_valid_pixels": mlx90640.valid_pixels if mlx90640 else None,
        "mlx_max_region_area": mlx90640.max_region_area if mlx90640 else None,
        "aq_weighted_score": air_quality.weighted_score if air_quality else None,
        "aq_score": air_quality.score if air_quality else None,
        "aq_level": LEVEL_LABELS.get(aq_level_code, air_quality.level if air_quality else None),
        "aq_action": air_quality.action if air_quality else None,
        "aq_message": _build_aq_message(air_quality),
        "aq_redlines": json.dumps(_labels(aq_redline_codes, REDLINE_LABELS), ensure_ascii=False),
        "aq_main_reasons": json.dumps(_labels(aq_reason_codes, REASON_LABELS), ensure_ascii=False),
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
                sht35_temperature_c, sht35_humidity_percent,
                pm1_0, pm2_5, pm10,
                mlx_temp_min_c, mlx_temp_max_c, mlx_temp_avg_c,
                mlx_occupied, mlx_occupancy_ratio, mlx_occupancy_heat_score,
                mlx_occupancy_score, mlx_occupancy_state, mlx_max_delta,
                mlx_valid_pixels, mlx_max_region_area,
                aq_weighted_score, aq_score, aq_level, aq_action, aq_message,
                aq_redlines, aq_main_reasons,
                sensor_ok, error_code, error_message, raw_json
            )
            VALUES (
                :received_at, :device_id, :firmware, :uptime_ms, :wifi_rssi, :wifi_ip,
                :co2_ppm, :temperature_c, :humidity_percent,
                :sht35_temperature_c, :sht35_humidity_percent,
                :pm1_0, :pm2_5, :pm10,
                :mlx_temp_min_c, :mlx_temp_max_c, :mlx_temp_avg_c,
                :mlx_occupied, :mlx_occupancy_ratio, :mlx_occupancy_heat_score,
                :mlx_occupancy_score, :mlx_occupancy_state, :mlx_max_delta,
                :mlx_valid_pixels, :mlx_max_region_area,
                :aq_weighted_score, :aq_score, :aq_level, :aq_action, :aq_message,
                :aq_redlines, :aq_main_reasons,
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
