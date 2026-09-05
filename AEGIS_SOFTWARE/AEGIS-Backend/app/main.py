from datetime import datetime, timezone
from typing import Optional

from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import HTMLResponse, RedirectResponse

from app.api.routes import _normalize_incident_type, manager, router
from app.db.database import init_db
from app.schemas.telemetry import IncidentLog
from app.services import incident_store
from app.services.simulator import TelemetrySimulator
from app.services.video_service import VideoStreamingService

app = FastAPI(
    title="A.E.G.I.S. Central Dashboard Server",
    version="1.0.0",
)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

app.include_router(router)

simulator: Optional[TelemetrySimulator] = None
video_service: Optional[VideoStreamingService] = None


@app.on_event("startup")
async def startup_event() -> None:
    """Create the simulator and video streaming service once the application is ready."""
    global simulator, video_service
    init_db()
    if simulator is None:
        simulator = TelemetrySimulator(manager)
        app.state.simulator = simulator
    if video_service is None:
        video_service = VideoStreamingService()
        app.state.video_service = video_service
    video_service.start()
    # Do not auto-start the simulator on backend startup. This preserves static test scenario payloads
    # until the dashboard user explicitly advances to the next test scenario.


@app.get("/")
async def root() -> dict:
    """Health check for the backend service."""
    return {
        "status": "online",
        "system": "A.E.G.I.S. Backend active",
        "video_stream_url": "/api/v1/video/stream/Guardian",
        "video_viewer_url": "/video",
    }


@app.get("/video", response_class=HTMLResponse)
async def video_viewer_root() -> HTMLResponse:
    """Direct route for browser access to the AEGIS Live Video Surveillance Station."""
    from app.api.routes import render_video_station_html
    return HTMLResponse(content=render_video_station_html("Guardian"))


@app.post("/api/v1/incidents/report")
async def report_incident(payload: dict) -> dict:
    """Broadcast an incident report to all connected clients and record it."""
    bot_id = payload.get("bot_id", "Guardian")
    raw_type = payload.get("type", "INTRUDER")
    incident_type = _normalize_incident_type(raw_type)
    severity = payload.get("severity", "CRITICAL")
    message = payload.get("message", "Security alert reported by hardware")

    incident = {
        "id": f"INC-{bot_id[:3].upper()}-{datetime.now(timezone.utc).strftime('%Y%m%d%H%M%S')}",
        "title": f"{str(raw_type).replace('_', ' ').title()} Alert from {bot_id}",
        "severity": severity,
        "type": incident_type,
        "message": message,
        "bot_id": bot_id if bot_id in {"Pathfinder", "Guardian", "Warden"} else "Guardian",
        "timestamp": datetime.now(timezone.utc),
        "active": True,
    }

    if simulator is not None:
        simulator.record_incident(IncidentLog(**incident))
        incident_store.save_incident(incident)
        simulator.update_bot_from_telemetry(bot_id, "telemetry", {"status": "ALERT"})

    broadcast_payload = {
        "type": "ALERT",
        "incident": IncidentLog(**incident).model_dump(mode="json"),
        "payload": payload,
        "timestamp": datetime.now(timezone.utc).isoformat(),
    }
    await manager.broadcast(broadcast_payload)
    return {"status": "success", "received": payload, "incident_id": incident["id"]}