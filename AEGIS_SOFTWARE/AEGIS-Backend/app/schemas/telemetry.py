from datetime import datetime
from typing import Dict, List, Literal, Optional

from pydantic import BaseModel, Field


class SystemInfo(BaseModel):
    free_heap: int
    psram: int


class MapPacket(BaseModel):
    """Matches the ESP32-S3 mesh map packet shape used by the swarm."""

    total_snaps: int = Field(default=10, ge=1)
    snap_index: int = Field(default=0, ge=0, le=9)
    x_coord: float
    y_coord: float
    ultrasonic_distances_cm: List[int] = Field(min_length=19, max_length=19)
    has_door: bool


class VisionDetection(BaseModel):
    """Vision payload emitted by a bot during scanning or patrol."""

    label: str
    confidence: float = Field(ge=0.0, le=100.0)
    is_threat: bool = False
    owner_id: Optional[int] = None
    bbox: Dict[str, float] = Field(default_factory=lambda: {"x": 0.0, "y": 0.0, "w": 0.0, "h": 0.0})


class FirstAidStatus(BaseModel):
    box_attached: bool
    delivered: bool


class DoorSweepStatus(BaseModel):
    in_progress: bool
    current_room_checking: Optional[str] = None


class HazardData(BaseModel):
    gas_ppm: float
    gas_alert: bool
    fire_detected: bool
    temperature_c: float


class BotStatus(BaseModel):
    """Current runtime status for one simulated or real bot."""

    bot_id: Literal["Pathfinder", "Guardian", "Warden"]
    status: Literal["MAPPING", "PATROL", "ALERT", "RESCUE", "OFFLINE", "CLEAR"]  # Added "CLEAR" for Warden
    battery_pct: int = Field(ge=0, le=100)
    system_info: SystemInfo
    wifi_rssi: int
    ip_address: str
    timestamp: datetime
    map_packet: Optional[MapPacket] = None
    vision_detections: Optional[List[VisionDetection]] = []
    wake_word_triggered: bool = False
    wake_word_label: Optional[str] = None
    first_aid_status: Optional[FirstAidStatus] = None
    hazard_data: Optional[HazardData] = None
    door_sweep_status: Optional[DoorSweepStatus] = None


class IncidentLog(BaseModel):
    """Historical or active incident notice emitted by the swarm."""

    id: str
    title: str
    severity: Literal["LOW", "MEDIUM", "HIGH", "CRITICAL"]
    type: Literal["INTRUDER", "FIRE", "GAS_LEAK", "MEDICAL_HELP"]
    message: str
    bot_id: Literal["Pathfinder", "Guardian", "Warden"]
    timestamp: datetime
    active: bool
