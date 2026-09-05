from datetime import datetime, timezone
from typing import Any, Dict, List, Optional

from fastapi import (
    APIRouter,
    Depends,
    File,
    Form,
    HTTPException,
    Query,
    Request,
    Response,
    UploadFile,
    WebSocket,
    WebSocketDisconnect,
)
from fastapi.responses import HTMLResponse, StreamingResponse
from pydantic import BaseModel

from app.schemas.telemetry import BotStatus, IncidentLog, MapPacket, VisionDetection
from app.services import incident_store
from app.services.pdf_export import build_incidents_pdf
from app.services.simulator import canonicalize_bot_id

router = APIRouter(prefix="/api/v1", tags=["telemetry"])

EMERGENCY_SERVICES = ["FIRE", "POLICE", "AMBULANCE"]

# The Warden firmware reports incidents using its own vocabulary (e.g.
# "FIRE_DETECTED", "GAS_DETECTED" - see incident_type_str() in
# warden_main.cpp) rather than the dashboard's canonical
# INTRUDER/FIRE/GAS_LEAK/MEDICAL_HELP set. Without translating these first,
# every unrecognized hardware type name silently fell back to "INTRUDER".
_HARDWARE_INCIDENT_TYPE_ALIASES = {
    "FIRE_DETECTED": "FIRE",
    "SMOKE_DETECTED": "FIRE",
    "FIRE_SMOKE_DETECTED": "FIRE",
    "SYSTEM_CLEAR": "FIRE",
    "GAS_DETECTED": "GAS_LEAK",
}


def _normalize_incident_type(raw_type: Any) -> str:
    normalized = str(raw_type or "").strip().upper()
    normalized = _HARDWARE_INCIDENT_TYPE_ALIASES.get(normalized, normalized)
    return normalized if normalized in {"INTRUDER", "FIRE", "GAS_LEAK", "MEDICAL_HELP"} else "INTRUDER"


def _classify_vision_label(label: str) -> tuple[str, str]:
    """Map a vision-detection label to (incident_type, emergency dispatch type).

    Vision threats aren't always intruders — the camera model also flags
    fire/smoke/gas frames, which must not be dispatched or logged as INTRUDER.
    """
    lowered = str(label or "").lower()
    if "fire" in lowered or "smoke" in lowered or "flame" in lowered:
        return "FIRE", "FIRE_AND_GAS"
    if "gas" in lowered:
        return "GAS_LEAK", "FIRE_AND_GAS"
    return "INTRUDER", "POLICE_AND_AMBULANCE"


def _make_incident(bot_id: str, title: str, incident_type: str, message: str, severity: str = "CRITICAL") -> Dict[str, Any]:
    incident = IncidentLog(
        id=f"INC-{bot_id[:3].upper()}-{datetime.now(timezone.utc).strftime('%Y%m%d%H%M%S')}",
        title=title,
        severity=severity,
        type=incident_type,
        message=message,
        bot_id=bot_id,
        timestamp=datetime.now(timezone.utc),
        active=True,
    )
    return incident.model_dump(mode="json")


async def _broadcast_emergency_status(
    bot_id: str,
    emergency_type: str,
    source: str,
    simulator: Any | None = None,
    incident_type: str | None = None,
) -> None:
    payload = {
        "type": "EMERGENCY_STATUS",
        "timestamp": datetime.now(timezone.utc).isoformat(),
        "bot_id": bot_id,
        "emergency_type": emergency_type,
        "mode": "AUTOMATIC" if source == "automatic" else "MANUAL",
        "services_notified": EMERGENCY_SERVICES,
        "triggered": True,
        "description": f"{source.capitalize()} emergency dispatch from {bot_id}.",
    }

    await manager.broadcast(payload)
    if simulator is not None:
        resolved_type = incident_type or (
            "MEDICAL_HELP" if source == "manual" else ("FIRE" if emergency_type == "FIRE_AND_GAS" else "INTRUDER")
        )
        incident = _make_incident(
            bot_id,
            f"{emergency_type.replace('_', ' ').title()} dispatched",
            resolved_type,
            f"Automated dispatch payload sent for {bot_id} ({emergency_type}).",
        )
        simulator.record_incident(IncidentLog(**incident))
        incident_store.save_incident(incident)

    print(f"[AEGIS] Emergency dispatch simulated: {payload}")


async def _evaluate_automatic_emergency(bot: Dict[str, Any], simulator: Any | None = None) -> bool:
    hazard = bot.get("hazard_data") or {}
    gas_alert = hazard.get("gas_alert") is True
    fire_detected = hazard.get("fire_detected") is True
    wake_word = bot.get("wake_word_triggered") is True
    vision = bot.get("vision_detections") or []
    is_threat = any(isinstance(det, dict) and det.get("is_threat") for det in vision)

    if not (gas_alert or fire_detected or wake_word or is_threat):
        return False

    # Fire and gas hazards are dispatched separately from intruder/medical
    # threats, so any hazard reading (not just both at once) must be
    # classified and logged as FIRE/GAS_LEAK rather than falling through to
    # the intruder category.
    if fire_detected or gas_alert:
        emergency_type = "FIRE_AND_GAS"
        incident_type = "FIRE" if fire_detected else "GAS_LEAK"
    else:
        emergency_type = "POLICE_AND_AMBULANCE"
        incident_type = "INTRUDER"

    await _broadcast_emergency_status(
        bot.get("bot_id", "Unknown"), emergency_type, "automatic", simulator, incident_type=incident_type
    )
    return True


class TelemetryIngest(BaseModel):
    """Incoming telemetry payload from an ESP32 board or simulator."""

    bot_id: str
    kind: str
    payload: Dict[str, Any]


class VideoConfig(BaseModel):
    """Configuration payload to update bot IP and camera port."""

    bot_id: str
    ip_address: str
    port: int = 80


class ConnectionManager:
    """Manages live WebSocket clients connected to the dashboard."""

    def __init__(self) -> None:
        self.active_connections: List[WebSocket] = []

    async def connect(self, websocket: WebSocket) -> None:
        await websocket.accept()
        self.active_connections.append(websocket)

    def disconnect(self, websocket: WebSocket) -> None:
        if websocket in self.active_connections:
            self.active_connections.remove(websocket)

    async def broadcast(self, message: Dict[str, Any]) -> None:
        dead_connections: List[WebSocket] = []
        for connection in self.active_connections:
            try:
                await connection.send_json(message)
            except Exception:
                dead_connections.append(connection)
        for connection in dead_connections:
            self.disconnect(connection)


manager = ConnectionManager()

TEST_SCENARIOS = [
    {
        "name": "Normal Patrol",
        "automatic_emergency_called": False,
        "emergency_type": None,
        "bots": [
            {
                "bot_id": "Pathfinder",
                "status": "MAPPING",
                "battery_pct": 92,
                "system_info": {"free_heap": 132000, "psram": 256000},
                "wifi_rssi": -58,
                "ip_address": "192.168.0.101",
                "timestamp": datetime.now(timezone.utc).isoformat(),
                "map_packet": {
                    "total_snaps": 10,
                    "snap_index": 3,
                    "x_coord": 0.18,
                    "y_coord": 0.50,
                    "ultrasonic_distances_cm": [48, 47, 45, 43, 41, 39, 38, 36, 35, 34, 33, 34, 36, 38, 41, 43, 45, 47, 49],
                    "has_door": True,
                },
                "vision_detections": [
                    {
                        "label": "class_door",
                        "confidence": 92.3,
                        "is_threat": False,
                        "owner_id": None,
                        "bbox": {"x": 19.5, "y": 24.7, "w": 15.2, "h": 38.1},
                    },
                ],
            },
            {
                "bot_id": "Guardian",
                "status": "PATROL",
                "battery_pct": 84,
                "system_info": {"free_heap": 118000, "psram": 250000},
                "wifi_rssi": -61,
                "ip_address": "192.168.0.102",
                "timestamp": datetime.now(timezone.utc).isoformat(),
                "wake_word_triggered": False,
                "wake_word_label": "Hi ESP",
                "first_aid_status": {"box_attached": True, "delivered": False},
                "vision_detections": [],
            },
            {
                "bot_id": "Warden",
                "status": "PATROL",
                "battery_pct": 76,
                "system_info": {"free_heap": 124000, "psram": 262000},
                "wifi_rssi": -64,
                "ip_address": "192.168.0.103",
                "timestamp": datetime.now(timezone.utc).isoformat(),
                "hazard_data": {
                    "gas_ppm": 210.0,
                    "gas_alert": False,
                    "fire_detected": False,
                    "temperature_c": 28.4,
                },
                "door_sweep_status": {"in_progress": False, "current_room_checking": None},
                "vision_detections": [
                    {
                        "label": "Equipment",
                        "confidence": 84.1,
                        "is_threat": False,
                        "owner_id": None,
                        "bbox": {"x": 10.0, "y": 15.0, "w": 40.0, "h": 40.0},
                    },
                ],
            },
        ],
        "incidents": [],
    },
    {
        "name": "Critical Gas & Fire Hazard",
        "automatic_emergency_called": True,
        "emergency_type": "FIRE_AND_GAS",
        "bots": [
            {
                "bot_id": "Pathfinder",
                "status": "MAPPING",
                "battery_pct": 89,
                "system_info": {"free_heap": 126000, "psram": 256000},
                "wifi_rssi": -59,
                "ip_address": "192.168.0.101",
                "timestamp": datetime.now(timezone.utc).isoformat(),
                "map_packet": {
                    "total_snaps": 10,
                    "snap_index": 5,
                    "x_coord": 0.31,
                    "y_coord": 0.55,
                    "ultrasonic_distances_cm": [46, 45, 44, 43, 42, 40, 40, 38, 37, 36, 37, 39, 40, 42, 43, 45, 47, 49, 52],
                    "has_door": True,
                },
                "vision_detections": [
                    {
                        "label": "Elevator",
                        "confidence": 88.1,
                        "is_threat": False,
                        "owner_id": None,
                        "bbox": {"x": 22.1, "y": 20.3, "w": 18.0, "h": 38.5},
                    },
                ],
            },
            {
                "bot_id": "Guardian",
                "status": "PATROL",
                "battery_pct": 78,
                "system_info": {"free_heap": 114500, "psram": 249500},
                "wifi_rssi": -63,
                "ip_address": "192.168.0.102",
                "timestamp": datetime.now(timezone.utc).isoformat(),
                "wake_word_triggered": False,
                "wake_word_label": "Hi ESP",
                "first_aid_status": {"box_attached": True, "delivered": False},
                "vision_detections": [],
            },
            {
                "bot_id": "Warden",
                "status": "ALERT",
                "battery_pct": 69,
                "system_info": {"free_heap": 119000, "psram": 260000},
                "wifi_rssi": -70,
                "ip_address": "192.168.0.103",
                "timestamp": datetime.now(timezone.utc).isoformat(),
                "hazard_data": {
                    "gas_ppm": 1850.0,
                    "gas_alert": True,
                    "fire_detected": True,
                    "temperature_c": 58.8,
                },
                "door_sweep_status": {"in_progress": True, "current_room_checking": "Storage"},
                "vision_detections": [
                    {
                        "label": "Flame",
                        "confidence": 96.5,
                        "is_threat": True,
                        "owner_id": None,
                        "bbox": {"x": 8.0, "y": 12.0, "w": 35.0, "h": 45.0},
                    },
                ],
            },
        ],
        "incidents": [
            {
                "id": "INC-FIRE-001",
                "title": "Fire and gas hazard detected",
                "severity": "CRITICAL",
                "type": "FIRE",
                "message": "Warden detected a severe gas leak with active flames.",
                "bot_id": "Warden",
                "timestamp": datetime.now(timezone.utc).isoformat(),
                "active": True,
            },
        ],
    },
    {
        "name": "Intruder Threat & Medical Emergency",
        "automatic_emergency_called": True,
        "emergency_type": "POLICE_AND_AMBULANCE",
        "bots": [
            {
                "bot_id": "Pathfinder",
                "status": "PATROL",
                "battery_pct": 87,
                "system_info": {"free_heap": 129000, "psram": 256000},
                "wifi_rssi": -60,
                "ip_address": "192.168.0.101",
                "timestamp": datetime.now(timezone.utc).isoformat(),
                "map_packet": {
                    "total_snaps": 10,
                    "snap_index": 7,
                    "x_coord": 0.52,
                    "y_coord": 0.38,
                    "ultrasonic_distances_cm": [50, 50, 48, 46, 45, 44, 43, 42, 41, 41, 41, 43, 44, 45, 47, 48, 50, 52, 54],
                    "has_door": False,
                },
                "vision_detections": [
                    {
                        "label": "class_door",
                        "confidence": 79.0,
                        "is_threat": False,
                        "owner_id": None,
                        "bbox": {"x": 16.2, "y": 27.1, "w": 14.8, "h": 36.2},
                    },
                ],
            },
            {
                "bot_id": "Guardian",
                "status": "ALERT",
                "battery_pct": 73,
                "system_info": {"free_heap": 112000, "psram": 248000},
                "wifi_rssi": -66,
                "ip_address": "192.168.0.102",
                "timestamp": datetime.now(timezone.utc).isoformat(),
                "wake_word_triggered": True,
                "wake_word_label": "Help",
                "first_aid_status": {"box_attached": False, "delivered": False},
                "vision_detections": [
                    {
                        "label": "INTRUDER - WEAPON DETECTED",
                        "confidence": 94.7,
                        "is_threat": True,
                        "owner_id": 13,
                        "bbox": {"x": 18.0, "y": 14.0, "w": 36.0, "h": 50.0},
                    },
                ],
            },
            {
                "bot_id": "Warden",
                "status": "PATROL",
                "battery_pct": 70,
                "system_info": {"free_heap": 121000, "psram": 261000},
                "wifi_rssi": -62,
                "ip_address": "192.168.0.103",
                "timestamp": datetime.now(timezone.utc).isoformat(),
                "hazard_data": {
                    "gas_ppm": 230.0,
                    "gas_alert": False,
                    "fire_detected": False,
                    "temperature_c": 29.1,
                },
                "door_sweep_status": {"in_progress": False, "current_room_checking": None},
                "vision_detections": [
                    {
                        "label": "Clear",
                        "confidence": 92.0,
                        "is_threat": False,
                        "owner_id": None,
                        "bbox": {"x": 11.0, "y": 18.0, "w": 38.0, "h": 42.0},
                    },
                ],
            },
        ],
        "incidents": [
            {
                "id": "INC-INTRU-001",
                "title": "Intruder threat detected",
                "severity": "HIGH",
                "type": "INTRUDER",
                "message": "Guardian identified an armed intruder and wake-word help signal.",
                "bot_id": "Guardian",
                "timestamp": datetime.now(timezone.utc).isoformat(),
                "active": True,
            },
        ],
    },
    {
        "name": "Hardware Failure / Offline",
        "automatic_emergency_called": False,
        "emergency_type": None,
        "bots": [
            {
                "bot_id": "Pathfinder",
                "status": "OFFLINE",
                "battery_pct": 0,
                "system_info": {"free_heap": 0, "psram": 0},
                "wifi_rssi": -128,
                "ip_address": "0.0.0.0",
                "timestamp": datetime.now(timezone.utc).isoformat(),
                "map_packet": {
                    "total_snaps": 0,
                    "snap_index": 0,
                    "x_coord": 0.0,
                    "y_coord": 0.0,
                    "ultrasonic_distances_cm": [0] * 19,
                    "has_door": False,
                },
                "vision_detections": [],
            },
            {
                "bot_id": "Guardian",
                "status": "PATROL",
                "battery_pct": 65,
                "system_info": {"free_heap": 107000, "psram": 247000},
                "wifi_rssi": -67,
                "ip_address": "192.168.0.102",
                "timestamp": datetime.now(timezone.utc).isoformat(),
                "wake_word_triggered": False,
                "wake_word_label": "Hi ESP",
                "first_aid_status": {"box_attached": True, "delivered": False},
                "vision_detections": [],
            },
            {
                "bot_id": "Warden",
                "status": "PATROL",
                "battery_pct": 72,
                "system_info": {"free_heap": 120000, "psram": 260500},
                "wifi_rssi": -66,
                "ip_address": "192.168.0.103",
                "timestamp": datetime.now(timezone.utc).isoformat(),
                "hazard_data": {
                    "gas_ppm": 220.0,
                    "gas_alert": False,
                    "fire_detected": False,
                    "temperature_c": 28.6,
                },
                "door_sweep_status": {"in_progress": False, "current_room_checking": None},
                "vision_detections": [
                    {
                        "label": "Normal",
                        "confidence": 80.0,
                        "is_threat": False,
                        "owner_id": None,
                        "bbox": {"x": 12.0, "y": 18.0, "w": 32.0, "h": 40.0},
                    },
                ],
            },
        ],
        "incidents": [
            {
                "id": "INC-OFFLINE-001",
                "title": "Pathfinder offline",
                "severity": "MEDIUM",
                "type": "INTRUDER",
                "message": "Pathfinder lost connection and is not responding.",
                "bot_id": "Pathfinder",
                "timestamp": datetime.now(timezone.utc).isoformat(),
                "active": True,
            },
        ],
    },
]

current_scenario_index = 0


def get_current_scenario() -> Dict[str, Any]:
    scenario = TEST_SCENARIOS[current_scenario_index]
    payload = {
        "type": "TELEMETRY_FRAME",
        "scenario_index": current_scenario_index + 1,
        "scenario_count": len(TEST_SCENARIOS),
        "scenario_name": scenario["name"],
        "bots": scenario["bots"],
        "incidents": scenario.get("incidents", []),
        "automatic_emergency_called": scenario.get("automatic_emergency_called", False),
        "emergency_type": scenario.get("emergency_type"),
    }
    return {
        "scenario_index": current_scenario_index + 1,
        "scenario_count": len(TEST_SCENARIOS),
        "scenario_name": scenario["name"],
        "payload": payload,
    }


async def broadcast_current_scenario() -> None:
    await manager.broadcast(get_current_scenario()["payload"])


def get_simulator(request: Request) -> Any:
    """Resolve the running simulator from the FastAPI app state."""
    sim = getattr(request.app.state, "simulator", None)
    if sim is None:
        raise HTTPException(status_code=503, detail="Simulator not initialized")
    return sim


def get_video_service(request: Request) -> Any:
    """Resolve the video streaming engine from the FastAPI app state."""
    video_svc = getattr(request.app.state, "video_service", None)
    if video_svc is None:
        raise HTTPException(status_code=503, detail="Video service not initialized")
    return video_svc


@router.get("/bots", response_model=List[BotStatus])
async def get_bots(simulator: Any = Depends(get_simulator)) -> List[BotStatus]:
    """Return the latest cached bot status snapshot."""
    return [BotStatus(**bot) for bot in simulator.get_bots()]


@router.get("/bots/{bot_id}")
async def get_single_bot(
    bot_id: str,
    simulator: Any = Depends(get_simulator),
    video_service: Any = Depends(get_video_service),
) -> Dict[str, Any]:
    """Return comprehensive live status for a specific bot."""
    norm_id = bot_id.strip().capitalize()
    bots = simulator.get_bots()
    matched = next((b for b in bots if b.get("bot_id", "").lower() == bot_id.lower()), None)
    if matched is None:
        raise HTTPException(status_code=404, detail=f"Bot '{bot_id}' not found")
    
    # Enrich with video streaming status and live IP
    video_diag = video_service.get_status().get("bots", {}).get(norm_id, {})
    return {
        "bot": matched,
        "video": video_diag,
        "timestamp": datetime.now(timezone.utc).isoformat(),
    }


@router.get("/bots/{bot_id}/detections")
async def get_bot_detections(
    bot_id: str,
    simulator: Any = Depends(get_simulator),
    video_service: Any = Depends(get_video_service),
) -> Dict[str, Any]:
    """Fetch real-time Edge Impulse vision detections from cached telemetry or live ESP32 query."""
    norm_id = bot_id.strip().capitalize()
    
    # 1. Try querying the physical ESP32 directly if reachable
    live_dets = await video_service.fetch_bot_detections(norm_id)
    if live_dets is not None:
        return {"source": "direct_esp32", "bot_id": norm_id, "data": live_dets}

    # 2. Fallback to cached simulator/ingest telemetry
    bots = simulator.get_bots()
    matched = next((b for b in bots if b.get("bot_id", "").lower() == bot_id.lower()), None)
    detections = matched.get("vision_detections", []) if matched else []
    return {
        "source": "cached_telemetry",
        "bot_id": norm_id,
        "detections": detections,
    }


@router.get("/bots/{bot_id}/map")
async def get_bot_map(
    bot_id: str,
    simulator: Any = Depends(get_simulator),
) -> Dict[str, Any]:
    """Fetch the latest SLAM occupancy grid map packet for Pathfinder."""
    bots = simulator.get_bots()
    matched = next((b for b in bots if b.get("bot_id", "").lower() == bot_id.lower()), None)
    if matched is None:
        raise HTTPException(status_code=404, detail=f"Bot '{bot_id}' not found")
    return {
        "bot_id": matched.get("bot_id"),
        "map_packet": matched.get("map_packet"),
        "timestamp": datetime.now(timezone.utc).isoformat(),
    }


@router.post("/bots/{bot_id}/control")
async def control_bot(
    bot_id: str,
    cmd: Optional[str] = None,
    mode: Optional[str] = None,
    video_service: Any = Depends(get_video_service),
    simulator: Any = Depends(get_simulator),
) -> Dict[str, Any]:
    """
    Send remote drive commands (FWD, REV, LEFT, RIGHT, STOP) or mode switch (auto, manual)
    directly to the physical ESP32 bot or update simulated state.
    """
    norm_id = bot_id.strip().capitalize()
    result = await video_service.send_bot_control(norm_id, cmd=cmd, mode=mode)
    
    if mode:
        simulator.update_bot_from_telemetry(norm_id, "telemetry", {"status": "MAPPING" if mode == "auto" else "PATROL"})

    return {
        "status": "success",
        "bot_id": norm_id,
        "cmd_sent": cmd,
        "mode_sent": mode,
        "esp32_response": result,
    }


@router.get("/incidents", response_model=List[IncidentLog])
async def get_incidents(simulator: Any = Depends(get_simulator)) -> List[IncidentLog]:
    """Return historical and active incident logs."""
    return [IncidentLog(**incident) for incident in simulator.get_incidents()]


@router.get("/incidents/history")
async def get_incident_history(
    bot_id: Optional[str] = Query(None, description="Filter to one bot, e.g. 'Guardian'"),
    severity: Optional[str] = Query(None, description="LOW | MEDIUM | HIGH | CRITICAL"),
    incident_type: Optional[str] = Query(None, description="INTRUDER | FIRE | GAS_LEAK | MEDICAL_HELP"),
    start: Optional[datetime] = Query(None, description="Only incidents at/after this time (ISO 8601)"),
    end: Optional[datetime] = Query(None, description="Only incidents at/before this time (ISO 8601)"),
    limit: int = Query(200, ge=1, le=2000),
) -> Dict[str, Any]:
    """Full persistent incident/notification history from the database.

    Unlike GET /incidents (the live rolling window kept in memory, capped at
    15 and lost on restart), this reads every incident ever recorded.
    """
    records = incident_store.list_incidents(
        bot_id=bot_id, severity=severity, type_=incident_type, start=start, end=end, limit=limit
    )
    return {"count": len(records), "incidents": records}


@router.get("/incidents/pdf")
async def export_incidents_pdf(
    bot_id: Optional[str] = Query(None, description="Filter to one bot, e.g. 'Guardian'"),
    severity: Optional[str] = Query(None, description="LOW | MEDIUM | HIGH | CRITICAL"),
    incident_type: Optional[str] = Query(None, description="INTRUDER | FIRE | GAS_LEAK | MEDICAL_HELP"),
    start: Optional[datetime] = Query(None, description="Only incidents at/after this time (ISO 8601)"),
    end: Optional[datetime] = Query(None, description="Only incidents at/before this time (ISO 8601)"),
    limit: int = Query(500, ge=1, le=5000),
) -> Response:
    """Export persisted incidents/notifications as a downloadable PDF report."""
    records = incident_store.list_incidents(
        bot_id=bot_id, severity=severity, type_=incident_type, start=start, end=end, limit=limit
    )
    pdf_bytes = build_incidents_pdf(records)
    filename = f"aegis_incidents_{datetime.now(timezone.utc).strftime('%Y%m%d_%H%M%S')}.pdf"
    return Response(
        content=pdf_bytes,
        media_type="application/pdf",
        headers={"Content-Disposition": f'attachment; filename="{filename}"'},
    )


@router.post("/telemetry/ingest")
async def ingest_telemetry(
    payload: TelemetryIngest,
    simulator: Any = Depends(get_simulator),
    video_service: Any = Depends(get_video_service),
) -> Dict[str, Any]:
    """Accept real telemetry from ESP32 boards when hardware is available."""
    canonical_id = canonicalize_bot_id(payload.bot_id)

    try:
        if payload.kind == "map_packet":
            MapPacket(**payload.payload)
        elif payload.kind == "vision_detection":
            VisionDetection(**payload.payload)
    except Exception as e:
        print(f"[AEGIS Telemetry Warning] Ingest payload schema mismatch: {e}")

    # Dynamically extract and register the bot's IP address for live video streaming
    if isinstance(payload.payload, dict):
        bot_ip = payload.payload.get("ip_address")
        camera_port = payload.payload.get("camera_port", 80)  # Default to 80 if not provided
        if bot_ip:
            video_service.register_bot_ip(canonical_id, bot_ip, camera_port)

    # Update simulator bot state
    simulator.update_bot_from_telemetry(canonical_id, payload.kind, payload.payload)

    # If vision detection is a threat, record incident and broadcast emergency
    if payload.kind == "vision_detection" and payload.payload.get("is_threat"):
        label = payload.payload.get("label", "Intruder / Threat")
        confidence = payload.payload.get("confidence", 90.0)
        incident_type, emergency_type = _classify_vision_label(label)
        incident = _make_incident(
            canonical_id,
            f"Threat Detected: {label}",
            incident_type,
            f"{canonical_id} detected {label} ({confidence:.1f}% confidence)",
            severity="CRITICAL",
        )
        simulator.record_incident(IncidentLog(**incident))
        incident_store.save_incident(incident)
        await _broadcast_emergency_status(canonical_id, emergency_type, "automatic", simulator, incident_type=incident_type)

    await manager.broadcast(
        {
            "type": "INGESTED_TELEMETRY",
            "timestamp": datetime.now(timezone.utc).isoformat(),
            "bot_id": canonical_id,
            "kind": payload.kind,
            "payload": payload.payload,
        }
    )
    return {"status": "accepted", "bot_id": canonical_id, "kind": payload.kind}


@router.post("/incidents/report")
async def report_incident_router(payload: dict, simulator: Any = Depends(get_simulator)) -> Dict[str, Any]:
    """Broadcast and record incident report from hardware to all connected clients."""
    bot_id = canonicalize_bot_id(payload.get("bot_id", "Guardian"))
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
        "bot_id": bot_id,
        "timestamp": datetime.now(timezone.utc),
        "active": True,
    }

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


@router.post("/emergency/manual-call")
async def manual_emergency_call(simulator: Any = Depends(get_simulator)) -> Dict[str, Any]:
    """Trigger a manual emergency dispatch from the dashboard."""
    await _broadcast_emergency_status("manual", "POLICE_AND_AMBULANCE", "manual", simulator)
    return {"status": "ok", "message": "Manual emergency dispatch simulated"}


@router.post("/test/next-scenario")
async def next_test_scenario(simulator: Any = Depends(get_simulator)) -> Dict[str, Any]:
    """Advance to the next test scenario and broadcast its telemetry payload."""
    global current_scenario_index
    current_scenario_index = (current_scenario_index + 1) % len(TEST_SCENARIOS)
    scenario = TEST_SCENARIOS[current_scenario_index]
    payload = get_current_scenario()["payload"]
    await manager.broadcast(payload)

    for bot in scenario["bots"]:
        await _evaluate_automatic_emergency(bot, simulator)

    return {
        "status": "ok",
        "scenario_index": current_scenario_index + 1,
        "scenario_name": scenario["name"],
        "payload": payload,
    }


@router.get("/test/current-scenario")
async def current_test_scenario() -> Dict[str, Any]:
    """Return the currently active test scenario and its payload."""
    return get_current_scenario()


@router.websocket("/ws/telemetry")
async def websocket_endpoint(websocket: WebSocket) -> None:
    """Stream live telemetry frames to connected Expo clients."""
    await manager.connect(websocket)
    await websocket.send_json(get_current_scenario()["payload"])
    try:
        while True:
            await websocket.receive_text()
    except WebSocketDisconnect:
        manager.disconnect(websocket)


# ============================================================
# AEGIS LIVE VIDEO STREAMING ENDPOINTS
# ============================================================

@router.get("/video/stream/{bot_id}")
async def get_live_video_stream(
    bot_id: str,
    video_service: Any = Depends(get_video_service),
) -> StreamingResponse:
    """
    Stream live multipart MJPEG video from the ESP32-S3 camera or animated standby HUD.
    Clients (browsers, dashboards, VLC, OpenCV) can render this directly.
    """
    return StreamingResponse(
        video_service.stream_video(bot_id),
        media_type="multipart/x-mixed-replace; boundary=frame",
        headers={
            "Cache-Control": "no-cache, no-store, must-revalidate, pre-check=0, post-check=0, max-age=0",
            "Pragma": "no-cache",
            "Expires": "0",
            "Connection": "close",
        },
    )


@router.get("/video/snapshot/{bot_id}")
async def get_video_snapshot(
    bot_id: str,
    video_service: Any = Depends(get_video_service),
) -> Response:
    """Return the latest single JPEG snapshot frame from the bot camera."""
    frame_bytes = video_service.get_snapshot(bot_id)
    return Response(
        content=frame_bytes,
        media_type="image/jpeg",
        headers={
            "Cache-Control": "no-cache, no-store, must-revalidate",
            "Content-Disposition": f'inline; filename="aegis_{bot_id}_snapshot.jpg"',
        },
    )


@router.post("/video/frame")
async def upload_video_frame(
    request: Request,
    bot_id: Optional[str] = Form(None),
    frame: Optional[UploadFile] = File(None),
    video_service: Any = Depends(get_video_service),
) -> Dict[str, Any]:
    """
    Receive a video frame pushed directly from ESP32 hardware
    (supports both multipart/form-data and raw JPEG binary POSTs).
    """
    target_bot = bot_id or "Guardian"
    frame_data: bytes = b""

    content_type = request.headers.get("content-type", "")
    if "multipart/form-data" in content_type and frame is not None:
        frame_data = await frame.read()
    else:
        frame_data = await request.body()
        bot_header = request.headers.get("x-bot-id")
        if bot_header:
            target_bot = bot_header

    if not frame_data:
        raise HTTPException(status_code=400, detail="Empty frame data received")

    video_service.ingest_frame(target_bot, frame_data)

    await manager.broadcast({
        "type": "VIDEO_FRAME",
        "timestamp": datetime.now(timezone.utc).isoformat(),
        "bot_id": target_bot,
        "frame_size": len(frame_data),
        "message": f"Frame ingested from {target_bot}",
    })

    return {
        "status": "success",
        "bot_id": target_bot,
        "frame_size": len(frame_data),
        "timestamp": datetime.now(timezone.utc).isoformat(),
    }


@router.get("/video/status")
async def get_video_status(
    video_service: Any = Depends(get_video_service),
) -> Dict[str, Any]:
    """Return diagnostic status of all video streams and bot IP mappings."""
    return video_service.get_status()


@router.post("/video/config")
async def configure_video_endpoint(
    config: VideoConfig,
    video_service: Any = Depends(get_video_service),
) -> Dict[str, Any]:
    """Manually configure or override a bot's IP address and camera port."""
    video_service.set_bot_ip(config.bot_id, config.ip_address, config.port)
    return {
        "status": "success",
        "bot_id": config.bot_id,
        "ip_address": config.ip_address,
        "port": config.port,
        "stream_url": f"http://{config.ip_address}:{config.port}/stream",
    }


@router.get("/video/view/{bot_id}", response_class=HTMLResponse)
async def video_station_view(bot_id: str) -> HTMLResponse:
    """HTML5 Live Surveillance Station for a specific bot."""
    return HTMLResponse(content=render_video_station_html(bot_id))


def render_video_station_html(selected_bot: str = "Guardian") -> str:
    """Render a futuristic AEGIS Live Video Surveillance HTML5 Station."""
    bots = ["Guardian", "Pathfinder", "Warden"]
    if selected_bot not in bots:
        selected_bot = "Guardian"

    return f"""<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>AEGIS Surveillance Station // {selected_bot}</title>
    <link rel="preconnect" href="https://fonts.googleapis.com">
    <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
    <link href="https://fonts.googleapis.com/css2?family=Chakra+Petch:wght@400;600;700&family=JetBrains+Mono:wght@400;500;700&display=swap" rel="stylesheet">
    <style>
        :root {{
            --bg-dark: #070b12;
            --panel-bg: rgba(13, 20, 36, 0.85);
            --panel-border: rgba(0, 210, 255, 0.25);
            --cyan-primary: #00d2ff;
            --cyan-glow: rgba(0, 210, 255, 0.4);
            --amber-alert: #ff9d00;
            --red-threat: #ff3366;
            --green-ok: #00ff88;
            --text-main: #e2e8f0;
            --text-dim: #8ba2b9;
        }}

        * {{
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }}

        body {{
            background: var(--bg-dark);
            color: var(--text-main);
            font-family: 'Chakra Petch', sans-serif;
            min-height: 100vh;
            display: flex;
            flex-direction: column;
            background-image: 
                radial-gradient(circle at 50% 0%, rgba(0, 210, 255, 0.08) 0%, transparent 60%),
                linear-gradient(to right, rgba(255,255,255,0.02) 1px, transparent 1px),
                linear-gradient(to bottom, rgba(255,255,255,0.02) 1px, transparent 1px);
            background-size: 100% 100%, 30px 30px, 30px 30px;
        }}

        /* Header */
        header {{
            display: flex;
            justify-content: space-between;
            align-items: center;
            padding: 16px 28px;
            background: rgba(10, 16, 28, 0.9);
            border-bottom: 1px solid var(--panel-border);
            backdrop-filter: blur(12px);
        }}

        .brand {{
            display: flex;
            align-items: center;
            gap: 12px;
        }}

        .brand-logo {{
            width: 32px;
            height: 32px;
            background: linear-gradient(135deg, var(--cyan-primary), #0066ff);
            border-radius: 6px;
            display: flex;
            align-items: center;
            justify-content: center;
            font-weight: 700;
            color: #fff;
            box-shadow: 0 0 16px var(--cyan-glow);
        }}

        .brand-title {{
            font-size: 20px;
            font-weight: 700;
            letter-spacing: 2px;
            color: #fff;
        }}

        .brand-subtitle {{
            font-size: 11px;
            letter-spacing: 1px;
            color: var(--cyan-primary);
            text-transform: uppercase;
        }}

        .system-status {{
            display: flex;
            align-items: center;
            gap: 20px;
            font-family: 'JetBrains Mono', monospace;
            font-size: 12px;
        }}

        .status-badge {{
            display: flex;
            align-items: center;
            gap: 8px;
            background: rgba(0, 255, 136, 0.1);
            color: var(--green-ok);
            border: 1px solid rgba(0, 255, 136, 0.3);
            padding: 4px 12px;
            border-radius: 4px;
        }}

        .status-dot {{
            width: 8px;
            height: 8px;
            background: currentColor;
            border-radius: 50%;
            animation: pulse 1.5s infinite;
        }}

        @keyframes pulse {{
            0%, 100% {{ opacity: 1; transform: scale(1); }}
            50% {{ opacity: 0.4; transform: scale(0.8); }}
        }}

        /* Main Container */
        .container {{
            flex: 1;
            display: grid;
            grid-template-columns: 1fr 340px;
            gap: 24px;
            padding: 24px 28px;
            max-width: 1600px;
            margin: 0 auto;
            width: 100%;
        }}

        @media (max-width: 1024px) {{
            .container {{
                grid-template-columns: 1fr;
            }}
        }}

        /* Video Stage */
        .video-stage {{
            display: flex;
            flex-direction: column;
            gap: 16px;
        }}

        .bot-nav {{
            display: flex;
            gap: 10px;
            background: rgba(13, 20, 36, 0.6);
            padding: 6px;
            border-radius: 8px;
            border: 1px solid var(--panel-border);
            width: fit-content;
        }}

        .bot-tab {{
            display: flex;
            align-items: center;
            gap: 8px;
            padding: 8px 18px;
            background: transparent;
            color: var(--text-dim);
            border: 1px solid transparent;
            border-radius: 6px;
            font-family: 'Chakra Petch', sans-serif;
            font-size: 14px;
            font-weight: 600;
            text-decoration: none;
            cursor: pointer;
            transition: all 0.2s ease;
        }}

        .bot-tab:hover {{
            color: var(--text-main);
            background: rgba(255, 255, 255, 0.05);
        }}

        .bot-tab.active {{
            background: rgba(0, 210, 255, 0.15);
            color: var(--cyan-primary);
            border-color: var(--cyan-primary);
            box-shadow: 0 0 12px var(--cyan-glow);
        }}

        .video-monitor {{
            position: relative;
            background: #000;
            border-radius: 12px;
            overflow: hidden;
            border: 1px solid var(--panel-border);
            box-shadow: 0 10px 40px rgba(0, 0, 0, 0.6);
            display: flex;
            justify-content: center;
            align-items: center;
            min-height: 480px;
        }}

        .video-feed {{
            width: 100%;
            height: auto;
            max-height: 600px;
            object-fit: contain;
            display: block;
            image-rendering: pixelated;
        }}

        /* HUD Overlays */
        .hud-overlay {{
            position: absolute;
            inset: 0;
            pointer-events: none;
            display: flex;
            flex-direction: column;
            justify-content: space-between;
            padding: 16px;
        }}

        .hud-header {{
            display: flex;
            justify-content: space-between;
            align-items: center;
        }}

        .hud-tag {{
            background: rgba(7, 11, 18, 0.85);
            border: 1px solid var(--cyan-primary);
            color: var(--cyan-primary);
            font-family: 'JetBrains Mono', monospace;
            font-size: 11px;
            padding: 4px 10px;
            border-radius: 4px;
            letter-spacing: 1px;
            backdrop-filter: blur(4px);
        }}

        .hud-footer {{
            display: flex;
            justify-content: space-between;
            align-items: flex-end;
            font-family: 'JetBrains Mono', monospace;
            font-size: 11px;
            color: #fff;
            text-shadow: 0 1px 4px rgba(0, 0, 0, 0.9);
        }}

        /* Scanline Overlay */
        .scanlines {{
            position: absolute;
            inset: 0;
            background: linear-gradient(
                rgba(18, 16, 16, 0) 50%, 
                rgba(0, 0, 0, 0.25) 50%
            );
            background-size: 100% 4px;
            pointer-events: none;
            opacity: 0.6;
        }}

        /* Controls Toolbar */
        .controls-bar {{
            display: flex;
            flex-wrap: wrap;
            gap: 12px;
            background: var(--panel-bg);
            border: 1px solid var(--panel-border);
            padding: 14px 18px;
            border-radius: 10px;
            backdrop-filter: blur(10px);
        }}

        .btn {{
            display: inline-flex;
            align-items: center;
            gap: 8px;
            background: rgba(0, 210, 255, 0.1);
            color: var(--cyan-primary);
            border: 1px solid var(--cyan-primary);
            padding: 8px 16px;
            border-radius: 6px;
            font-family: 'Chakra Petch', sans-serif;
            font-size: 13px;
            font-weight: 600;
            cursor: pointer;
            text-decoration: none;
            transition: all 0.2s;
        }}

        .btn:hover {{
            background: var(--cyan-primary);
            color: #000;
            box-shadow: 0 0 16px var(--cyan-glow);
        }}

        .btn-secondary {{
            background: rgba(255, 255, 255, 0.05);
            color: var(--text-main);
            border-color: rgba(255, 255, 255, 0.2);
        }}

        .btn-secondary:hover {{
            background: rgba(255, 255, 255, 0.15);
            color: #fff;
            box-shadow: none;
        }}

        /* Sidebar Stats Panel */
        .sidebar {{
            display: flex;
            flex-direction: column;
            gap: 16px;
        }}

        .card {{
            background: var(--panel-bg);
            border: 1px solid var(--panel-border);
            border-radius: 10px;
            padding: 18px;
            backdrop-filter: blur(10px);
        }}

        .card-title {{
            font-size: 14px;
            font-weight: 700;
            letter-spacing: 1.5px;
            color: var(--cyan-primary);
            margin-bottom: 14px;
            display: flex;
            align-items: center;
            justify-content: space-between;
            border-bottom: 1px solid rgba(0, 210, 255, 0.15);
            padding-bottom: 8px;
            text-transform: uppercase;
        }}

        .stat-list {{
            display: flex;
            flex-direction: column;
            gap: 10px;
            font-family: 'JetBrains Mono', monospace;
            font-size: 12px;
        }}

        .stat-item {{
            display: flex;
            justify-content: space-between;
            padding: 6px 0;
            border-bottom: 1px dashed rgba(255, 255, 255, 0.05);
        }}

        .stat-label {{
            color: var(--text-dim);
        }}

        .stat-value {{
            color: #fff;
            font-weight: 500;
        }}

        .alert-box {{
            padding: 10px 14px;
            border-radius: 6px;
            font-size: 12px;
            line-height: 1.4;
            margin-top: 8px;
            border-left: 3px solid;
        }}

        .alert-ok {{
            background: rgba(0, 255, 136, 0.1);
            border-color: var(--green-ok);
            color: #a3f7bf;
        }}

        .ip-input-group {{
            display: flex;
            gap: 8px;
            margin-top: 12px;
        }}

        .ip-input {{
            flex: 1;
            background: rgba(0, 0, 0, 0.5);
            border: 1px solid var(--panel-border);
            color: #fff;
            padding: 6px 10px;
            border-radius: 4px;
            font-family: 'JetBrains Mono', monospace;
            font-size: 12px;
        }}

        .ip-input:focus {{
            outline: none;
            border-color: var(--cyan-primary);
        }}
    </style>
</head>
<body>

    <!-- Header -->
    <header>
        <div class="brand">
            <div class="brand-logo">A</div>
            <div>
                <div class="brand-title">A.E.G.I.S.</div>
                <div class="brand-subtitle">Automated Emergency & Guardian Intelligence Swarm</div>
            </div>
        </div>

        <div class="system-status">
            <div class="status-badge">
                <span class="status-dot"></span>
                <span>STREAM SERVER LIVE</span>
            </div>
            <div id="live-clock">--:--:-- UTC</div>
        </div>
    </header>

    <!-- Main Content -->
    <div class="container">
        <!-- Video Stream Stage -->
        <div class="video-stage">
            <!-- Bot Switcher Tabs -->
            <div class="bot-nav">
                <a href="/api/v1/video/view/Guardian" class="bot-tab {'active' if selected_bot == 'Guardian' else ''}">
                    <span>🛡️ Guardian (ESP32-S3 Master)</span>
                </a>
                <a href="/api/v1/video/view/Pathfinder" class="bot-tab {'active' if selected_bot == 'Pathfinder' else ''}">
                    <span>🗺️ Pathfinder</span>
                </a>
                <a href="/api/v1/video/view/Warden" class="bot-tab {'active' if selected_bot == 'Warden' else ''}">
                    <span>⚠️ Warden</span>
                </a>
            </div>

            <!-- Monitor Container -->
            <div class="video-monitor" id="video-monitor">
                <img 
                    id="stream-img" 
                    class="video-feed" 
                    src="/api/v1/video/stream/{selected_bot}" 
                    alt="Live Video Stream from {selected_bot}"
                    onerror="handleStreamError(this)"
                />
                
                <div class="scanlines"></div>

                <!-- HUD Overlays -->
                <div class="hud-overlay">
                    <div class="hud-header">
                        <div class="hud-tag">REC // LIVE FEED</div>
                        <div class="hud-tag" id="hud-bot-tag">{selected_bot.upper()} // VGA 640x480</div>
                    </div>
                    <div class="hud-footer">
                        <div>LATENCY: &lt;80ms | 20.0 FPS</div>
                        <div id="hud-timestamp">--:--:--</div>
                    </div>
                </div>
            </div>

            <!-- Controls Toolbar -->
            <div class="controls-bar">
                <button class="btn" onclick="captureSnapshot('{selected_bot}')">
                    📸 Capture Snapshot
                </button>
                <button class="btn btn-secondary" onclick="reconnectStream()">
                    🔄 Reconnect Stream
                </button>
                <button class="btn btn-secondary" onclick="toggleFullscreen()">
                    ⛶ Fullscreen
                </button>
                <button class="btn btn-secondary" onclick="copyStreamUrl('{selected_bot}')">
                    📋 Copy Stream URL
                </button>
                <a href="/api/v1/video/stream/{selected_bot}" target="_blank" class="btn btn-secondary">
                    🌐 Open Raw MJPEG
                </a>
            </div>
        </div>

        <!-- Sidebar Telemetry & Config -->
        <div class="sidebar">
            <!-- Hardware Diagnostics -->
            <div class="card">
                <div class="card-title">
                    <span>📡 Target Hardware</span>
                    <span id="bot-status-pill" style="font-size: 11px; color: var(--cyan-primary);">ONLINE</span>
                </div>
                <div class="stat-list">
                    <div class="stat-item">
                        <span class="stat-label">Bot Identifier</span>
                        <span class="stat-value" id="stat-bot-id">{selected_bot}</span>
                    </div>
                    <div class="stat-item">
                        <span class="stat-label">Target Camera IP</span>
                        <span class="stat-value" id="stat-ip">Detecting...</span>
                    </div>
                    <div class="stat-item">
                        <span class="stat-label">Stream Port</span>
                        <span class="stat-value">80 (/stream)</span>
                    </div>
                    <div class="stat-item">
                        <span class="stat-label">Frame Resolution</span>
                        <span class="stat-value">VGA 640x480 (Sharp)</span>
                    </div>
                    <div class="stat-item">
                        <span class="stat-label">Frame Encoding</span>
                        <span class="stat-value">MJPEG / JPEG</span>
                    </div>
                    <div class="stat-item">
                        <span class="stat-label">Total Frames</span>
                        <span class="stat-value" id="stat-frames">0</span>
                    </div>
                </div>

                <!-- Manual IP Override -->
                <div style="margin-top: 16px;">
                    <label style="font-size: 11px; color: var(--text-dim); text-transform: uppercase;">Manual ESP32 IP Override</label>
                    <div class="ip-input-group">
                        <input type="text" id="manual-ip-input" class="ip-input" placeholder="e.g. 10.75.11.83" />
                        <button class="btn" style="padding: 6px 12px;" onclick="saveManualIp('{selected_bot}')">Save</button>
                    </div>
                </div>
            </div>

            <!-- Real-Time AI & Sensor HUD -->
            <div class="card">
                <div class="card-title">
                    <span>🧠 Vision & AI State</span>
                </div>
                <div class="stat-list">
                    <div class="stat-item">
                        <span class="stat-label">Security Mode</span>
                        <span class="stat-value" id="ai-security" style="color: var(--green-ok);">PATROL</span>
                    </div>
                    <div class="stat-item">
                        <span class="stat-label">Detected Object</span>
                        <span class="stat-value" id="ai-object">Searching...</span>
                    </div>
                    <div class="stat-item">
                        <span class="stat-label">Confidence</span>
                        <span class="stat-value" id="ai-conf">--%</span>
                    </div>
                    <div class="stat-item">
                        <span class="stat-label">Face Recognition</span>
                        <span class="stat-value" id="ai-face">Enrolled Owner</span>
                    </div>
                    <div class="stat-item">
                        <span class="stat-label">Threat Assessment</span>
                        <span class="stat-value" id="ai-threat" style="color: var(--green-ok);">CLEAR</span>
                    </div>
                </div>

                <div class="alert-box alert-ok" id="status-alert-box">
                    System nominal. Live video feed is being proxied through AEGIS Central Server.
                </div>
            </div>
        </div>
    </div>

    <!-- Scripts -->
    <script>
        const currentBot = "{selected_bot}";

        // Live Clock
        function updateClock() {{
            const now = new Date();
            const timeStr = now.toISOString().substring(11, 19) + ' UTC';
            document.getElementById('live-clock').innerText = timeStr;
            document.getElementById('hud-timestamp').innerText = timeStr;
        }}
        setInterval(updateClock, 1000);
        updateClock();

        // Stream Error Handler / Auto Reconnect
        function handleStreamError(img) {{
            console.warn('[AEGIS Video] Stream connection lost. Retrying in 2 seconds...');
            setTimeout(() => {{
                img.src = '/api/v1/video/stream/' + currentBot + '?t=' + Date.now();
            }}, 2000);
        }}

        function reconnectStream() {{
            const img = document.getElementById('stream-img');
            img.src = '/api/v1/video/stream/' + currentBot + '?t=' + Date.now();
        }}

        function toggleFullscreen() {{
            const monitor = document.getElementById('video-monitor');
            if (!document.fullscreenElement) {{
                monitor.requestFullscreen().catch(err => alert(err.message));
            }} else {{
                document.exitFullscreen();
            }}
        }}

        function captureSnapshot(botId) {{
            const link = document.createElement('a');
            link.href = '/api/v1/video/snapshot/' + botId + '?t=' + Date.now();
            link.download = 'aegis_' + botId + '_snapshot_' + Date.now() + '.jpg';
            document.body.appendChild(link);
            link.click();
            document.body.removeChild(link);
        }}

        function copyStreamUrl(botId) {{
            const url = window.location.origin + '/api/v1/video/stream/' + botId;
            navigator.clipboard.writeText(url).then(() => {{
                alert('Stream URL copied to clipboard:\\n' + url);
            }});
        }}

        async function fetchVideoStatus() {{
            try {{
                const res = await fetch('/api/v1/video/status');
                if (res.ok) {{
                    const data = await res.json();
                    const botInfo = data.bots[currentBot];
                    if (botInfo) {{
                        document.getElementById('stat-ip').innerText = botInfo.ip_address + ':' + botInfo.port;
                        document.getElementById('stat-frames').innerText = botInfo.total_frames_processed;
                        document.getElementById('manual-ip-input').placeholder = botInfo.ip_address;
                    }}
                }}
            }} catch (e) {{
                console.error('Failed to fetch video status', e);
            }}
        }}
        setInterval(fetchVideoStatus, 3000);
        fetchVideoStatus();

        async function saveManualIp(botId) {{
            const ip = document.getElementById('manual-ip-input').value.trim();
            if (!ip) {{
                alert('Please enter a valid IP address.');
                return;
            }}
            try {{
                const res = await fetch('/api/v1/video/config', {{
                    method: 'POST',
                    headers: {{ 'Content-Type': 'application/json' }},
                    body: JSON.stringify({{ bot_id: botId, ip_address: ip, port: 80 }})
                }});
                if (res.ok) {{
                    alert('IP updated to ' + ip + '. Reconnecting stream...');
                    reconnectStream();
                    fetchVideoStatus();
                }}
            }} catch (e) {{
                alert('Failed to configure IP: ' + e);
            }}
        }}

        // WebSocket telemetry hookup for live UI overlay updates
        function connectTelemetryWs() {{
            const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
            const ws = new WebSocket(protocol + '//' + window.location.host + '/api/v1/ws/telemetry');

            ws.onmessage = (event) => {{
                try {{
                    const msg = JSON.parse(event.data);
                    if (msg.type === 'INGESTED_TELEMETRY' && msg.bot_id === currentBot) {{
                        if (msg.kind === 'vision_detection') {{
                            document.getElementById('ai-object').innerText = msg.payload.label || 'None';
                            document.getElementById('ai-conf').innerText = (msg.payload.confidence || 0).toFixed(1) + '%';
                            const isThreat = msg.payload.is_threat;
                            const threatEl = document.getElementById('ai-threat');
                            threatEl.innerText = isThreat ? 'THREAT DETECTED' : 'CLEAR';
                            threatEl.style.color = isThreat ? 'var(--red-threat)' : 'var(--green-ok)';
                        }} else if (msg.kind === 'telemetry') {{
                            if (msg.payload.ip_address) {{
                                document.getElementById('stat-ip').innerText = msg.payload.ip_address;
                            }}
                            if (msg.payload.status) {{
                                document.getElementById('ai-security').innerText = msg.payload.status;
                            }}
                        }}
                    }}
                }} catch (err) {{
                    console.error('WS parse error', err);
                }}
            }};

            ws.onclose = () => {{
                setTimeout(connectTelemetryWs, 3000);
            }};
        }}
        connectTelemetryWs();
    </script>
</body>
</html>
"""
