import argparse
import math
import random
import time
from typing import Dict

import requests


DEVICE_ID = "esp32s3_node_001"
DEVICE_TOKEN = "classguard_test_token_001"


def build_payload(index: int, uptime_ms: int) -> Dict:
    wave = math.sin(index / 8)
    co2 = 620 + wave * 90 + random.uniform(-18, 18)
    temperature = 25.0 + math.sin(index / 14) * 1.4 + random.uniform(-0.2, 0.2)
    humidity = 52.0 + math.cos(index / 11) * 4 + random.uniform(-0.8, 0.8)
    pm25 = 14 + max(0, math.sin(index / 10)) * 18 + random.uniform(-2, 2)
    mlx_avg = temperature + 1.0 + random.uniform(-0.4, 0.4)

    return {
        "device_id": DEVICE_ID,
        "firmware": "mock-v0.1.0",
        "timestamp": 0,
        "uptime_ms": uptime_ms,
        "wifi": {
            "rssi": int(-48 + random.uniform(-5, 4)),
            "ip": "192.168.0.120",
        },
        "sensors": {
            "scd41": {
                "co2_ppm": round(co2, 1),
                "temperature_c": round(temperature, 2),
                "humidity_percent": round(humidity, 2),
            },
            "pms5003": {
                "pm1_0": round(max(1, pm25 * 0.55 + random.uniform(-1, 1)), 1),
                "pm2_5": round(max(1, pm25), 1),
                "pm10": round(max(1, pm25 * 1.45 + random.uniform(-2, 2)), 1),
            },
            "mlx90640": {
                "frame_rate": 16,
                "temp_min_c": round(mlx_avg - random.uniform(1.5, 2.4), 2),
                "temp_max_c": round(mlx_avg + random.uniform(2.2, 4.4), 2),
                "temp_avg_c": round(mlx_avg, 2),
            },
        },
        "status": {
            "sensor_ok": True,
            "error_code": 0,
            "error_message": "",
        },
    }


def main() -> None:
    parser = argparse.ArgumentParser(description="Send mock ESP32-S3 telemetry to local server.")
    parser.add_argument("--url", default="http://127.0.0.1:8000/api/telemetry")
    parser.add_argument("--interval", type=float, default=2.0)
    parser.add_argument("--count", type=int, default=0, help="Stop after N packets. Use 0 to run forever.")
    args = parser.parse_args()

    headers = {
        "Content-Type": "application/json",
        "X-Device-Token": DEVICE_TOKEN,
    }
    started = time.monotonic()
    index = 0

    print(f"Sending mock telemetry to {args.url}")
    while True:
        uptime_ms = int((time.monotonic() - started) * 1000)
        payload = build_payload(index, uptime_ms)
        try:
            response = requests.post(args.url, json=payload, headers=headers, timeout=5)
            print(
                f"#{index:04d} status={response.status_code} "
                f"co2={payload['sensors']['scd41']['co2_ppm']} "
                f"pm2.5={payload['sensors']['pms5003']['pm2_5']}"
            )
        except requests.RequestException as exc:
            print(f"#{index:04d} failed: {exc}")

        index += 1
        if args.count and index >= args.count:
            break
        time.sleep(args.interval)


if __name__ == "__main__":
    main()
