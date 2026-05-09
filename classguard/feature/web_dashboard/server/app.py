import asyncio
import json
from datetime import datetime
from pathlib import Path
from typing import Set

from fastapi import FastAPI, Header, HTTPException, Query
from fastapi.responses import FileResponse, StreamingResponse
from fastapi.staticfiles import StaticFiles

from config import ALLOWED_DEVICE_IDS, DEVICE_TOKEN, HISTORY_DEFAULT_LIMIT, HISTORY_MAX_LIMIT
from database import get_history, get_latest, init_db, insert_telemetry
from models import TelemetryIn


BASE_DIR = Path(__file__).resolve().parent
STATIC_DIR = BASE_DIR / "static"

app = FastAPI(title="ClassGuard Local Web Dashboard")
app.mount("/static", StaticFiles(directory=STATIC_DIR), name="static")

_subscribers: Set[asyncio.Queue] = set()


@app.on_event("startup")
async def on_startup() -> None:
    await init_db()


@app.get("/")
async def index() -> FileResponse:
    return FileResponse(STATIC_DIR / "index.html")


@app.post("/api/telemetry")
async def receive_telemetry(
    payload: TelemetryIn,
    x_device_token: str = Header(default="", alias="X-Device-Token"),
):
    if x_device_token != DEVICE_TOKEN:
        raise HTTPException(status_code=401, detail="Invalid device token")
    if payload.device_id not in ALLOWED_DEVICE_IDS:
        raise HTTPException(status_code=403, detail="Device is not allowed")

    saved = await insert_telemetry(payload, datetime.now().astimezone())
    await _publish(saved)
    return {"ok": True, "id": saved["id"], "received_at": saved["received_at"]}


@app.get("/api/latest")
async def latest():
    return {"data": await get_latest()}


@app.get("/api/history")
async def history(limit: int = Query(default=HISTORY_DEFAULT_LIMIT, ge=1, le=HISTORY_MAX_LIMIT)):
    return {"data": await get_history(limit)}


@app.get("/events")
async def events():
    queue: asyncio.Queue = asyncio.Queue(maxsize=20)
    _subscribers.add(queue)

    async def event_stream():
        try:
            yield "event: connected\ndata: {}\n\n"
            while True:
                item = await queue.get()
                yield f"data: {json.dumps(item, ensure_ascii=False)}\n\n"
        except asyncio.CancelledError:
            raise
        finally:
            _subscribers.discard(queue)

    return StreamingResponse(event_stream(), media_type="text/event-stream")


async def _publish(item) -> None:
    stale = []
    for queue in _subscribers:
        try:
            queue.put_nowait(item)
        except asyncio.QueueFull:
            stale.append(queue)
    for queue in stale:
        _subscribers.discard(queue)
