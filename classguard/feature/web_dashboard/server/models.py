from typing import List, Optional

from pydantic import BaseModel, Field


class WifiInfo(BaseModel):
    rssi: Optional[int] = None
    ip: Optional[str] = None


class Scd41Data(BaseModel):
    co2_ppm: Optional[float] = Field(default=None, ge=0)
    temperature_c: Optional[float] = None
    humidity_percent: Optional[float] = Field(default=None, ge=0, le=100)


class Sht35Data(BaseModel):
    temperature_c: Optional[float] = None
    humidity_percent: Optional[float] = Field(default=None, ge=0, le=100)


class Pms5003Data(BaseModel):
    pm1_0: Optional[float] = Field(default=None, ge=0)
    pm2_5: Optional[float] = Field(default=None, ge=0)
    pm10: Optional[float] = Field(default=None, ge=0)


class Mlx90640Data(BaseModel):
    frame_rate: Optional[float] = Field(default=None, ge=0)
    temp_min_c: Optional[float] = None
    temp_max_c: Optional[float] = None
    temp_avg_c: Optional[float] = None
    occupied: Optional[bool] = None
    occupancy_ratio: Optional[float] = Field(default=None, ge=0)
    occupancy_heat_score: Optional[float] = Field(default=None, ge=0)
    occupancy_score: Optional[float] = Field(default=None, ge=0)
    state: Optional[int] = None
    max_delta: Optional[float] = None
    valid_pixels: Optional[int] = Field(default=None, ge=0)
    max_region_area: Optional[int] = Field(default=None, ge=0)


class SensorGroup(BaseModel):
    scd41: Optional[Scd41Data] = None
    sht35: Optional[Sht35Data] = None
    pms5003: Optional[Pms5003Data] = None
    mlx90640: Optional[Mlx90640Data] = None


class DeviceStatus(BaseModel):
    sensor_ok: bool = True
    error_code: int = 0
    error_message: str = ""


class AirQualityMetricValues(BaseModel):
    co2: Optional[float] = None
    pm1_0: Optional[float] = None
    pm2_5: Optional[float] = None
    pm10: Optional[float] = None
    temperature: Optional[float] = None
    humidity: Optional[float] = None
    occupancy: Optional[float] = None


class AirQualityData(BaseModel):
    weighted_score: Optional[float] = Field(default=None, ge=0, le=100)
    score: Optional[float] = Field(default=None, ge=0, le=100)
    level_code: Optional[str] = None
    level: Optional[str] = None
    message: Optional[str] = None
    action: Optional[str] = None
    redlines: List[str] = Field(default_factory=list)
    main_reasons: List[str] = Field(default_factory=list)
    scores: Optional[AirQualityMetricValues] = None
    weights: Optional[AirQualityMetricValues] = None


class TelemetryIn(BaseModel):
    device_id: str
    firmware: Optional[str] = None
    timestamp: Optional[float] = 0
    uptime_ms: Optional[int] = Field(default=None, ge=0)
    wifi: Optional[WifiInfo] = None
    sensors: Optional[SensorGroup] = None
    air_quality: Optional[AirQualityData] = None
    status: Optional[DeviceStatus] = None
