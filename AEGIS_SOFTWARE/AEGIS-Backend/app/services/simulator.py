import asyncio
import random
from datetime import datetime, timezone
from typing import Any, Dict, List

from app.schemas.telemetry import BotStatus, IncidentLog, MapPacket, VisionDetection


def canonicalize_bot_id(bot_id: str) -> str:
    """Normalize incoming bot IDs across variations and typos (e.g. 'gurdian', 'guardian', 'Pathfinder')."""
    if not bot_id:
        return "Guardian"
    cleaned = str(bot_id).strip().lower()
    if "path" in cleaned:
        return "Pathfinder"
    if "guard" in cleaned or "gurd" in cleaned:
        return "Guardian"
    if "ward" in cleaned:
        return "Warden"
    return str(bot_id).strip().capitalize()


class TelemetrySimulator:
    """Background manager for AEGIS bot telemetry and live hardware data coordination."""

    def __init__(self, manager: Any) -> None:
        self.manager = manager
        self._task: asyncio.Task | None = None
        self._running = False
        self._bot_states: Dict[str, Dict[str, Any]] = {}
        self._incidents: List[Dict[str, Any]] = []
        self._incident_counter = 0
        self._bot_order = ["Pathfinder", "Guardian", "Warden"]

    async def start(self) -> None:
        if self._task and not self._task.done():
            return
        self._running = True
        self._task = asyncio.create_task(self._run_loop())

    async def stop(self) -> None:
        self._running = False
        if self._task:
            self._task.cancel()
            try:
                await self._task
            except asyncio.CancelledError:
                pass
            self._task = None

    async def _run_loop(self) -> None:
        while self._running:
            await self._tick()
            await asyncio.sleep(1.0)

    async def _tick(self) -> None:
        self._update_bot_states()
        frame = self._build_frame()
        await self.manager.broadcast(frame)

    def _update_bot_states(self) -> None:
        now = datetime.now(timezone.utc)
        for bot_id in self._bot_order:
            state = self._bot_states.setdefault(
                bot_id,
                {
                    "bot_id": bot_id,
                    "status": "OFFLINE",
                    "battery_pct": 0,
                    "free_heap": 0,
                    "psram": 0,
                    "wifi_rssi": -128,
                    "last_seen": now,
                    "ip_address": "0.0.0.0",
                    "wake_word_active": False,
                    "is_real_hardware": False,
                },
            )

            # If this bot is connected physical hardware, check connection freshness
            if state.get("is_real_hardware"):
                last_seen = state.get("last_seen")
                if isinstance(last_seen, datetime):
                    diff = (now - last_seen).total_seconds()
                    if diff > 15.0:
                        state["status"] = "OFFLINE"
            else:
                state["status"] = "OFFLINE"

    def _build_frame(self) -> Dict[str, Any]:
        bots = []
        for bot_id in self._bot_order:
            state = self._bot_states.get(bot_id, {})
            is_active = state.get("is_real_hardware", False) and state.get("status") != "OFFLINE"
            status_val = state.get("status", "OFFLINE")

            bot_dict = {
                "bot_id": bot_id,
                "status": status_val,
                "battery_pct": int(state.get("battery_pct", 0 if not is_active else 95)),
                "system_info": {
                    "free_heap": int(state.get("free_heap", 0 if not is_active else 120000)),
                    "psram": int(state.get("psram", 0 if not is_active else 256000)),
                },
                "wifi_rssi": int(state.get("wifi_rssi", -128 if not is_active else -58)),
                "ip_address": str(state.get("ip_address", "0.0.0.0")),
                "timestamp": state.get("last_seen", datetime.now(timezone.utc)).isoformat()
                if isinstance(state.get("last_seen"), datetime)
                else str(state.get("last_seen")),
                "wake_word_triggered": bool(state.get("wake_word_active", False)),
                "wake_word_label": state.get("wake_word_label", ""),
                "first_aid_status": state.get("first_aid_status", {"box_attached": False, "delivered": False}),
                "vision_detections": state.get("current_vision", []),
                "map_packet": state.get("current_map_packet"),
            }
            bots.append(bot_dict)

        frame = {
            "type": "TELEMETRY_FRAME",
            "timestamp": datetime.now(timezone.utc).isoformat(),
            "bots": bots,
            "incidents": self._incidents[-15:],
        }
        return frame

    def record_incident(self, incident: Any) -> None:
        if isinstance(incident, IncidentLog):
            self._incidents.append(incident.model_dump(mode="json"))
        elif isinstance(incident, dict):
            self._incidents.append(incident)

    def get_bots(self) -> List[Dict[str, Any]]:
        now = datetime.now(timezone.utc)
        bots = []
        for bot_id in self._bot_order:
            state = self._bot_states.get(bot_id, {})
            if not state:
                bots.append(
                    {
                        "bot_id": bot_id,
                        "status": "OFFLINE",
                        "battery_pct": 0,
                        "system_info": {"free_heap": 0, "psram": 0},
                        "wifi_rssi": -128,
                        "ip_address": "0.0.0.0",
                        "timestamp": now,
                        "wake_word_triggered": False,
                        "wake_word_label": "",
                        "first_aid_status": {"box_attached": False, "delivered": False},
                        "vision_detections": [],
                        "map_packet": None,
                    }
                )
                continue

            last_seen = state.get("last_seen")
            is_stale = False
            if isinstance(last_seen, datetime):
                diff = (now - last_seen).total_seconds()
                if diff > 15.0:
                    is_stale = True

            status_val = "OFFLINE" if is_stale else state.get("status", "PATROL")

            bots.append(
                {
                    "bot_id": bot_id,
                    "status": status_val,
                    "battery_pct": int(state.get("battery_pct", 0 if is_stale else 90)),
                    "system_info": {
                        "free_heap": int(state.get("free_heap", 0 if is_stale else 120000)),
                        "psram": int(state.get("psram", 0 if is_stale else 256000)),
                    },
                    "wifi_rssi": int(state.get("wifi_rssi", -128 if is_stale else -60)),
                    "ip_address": str(state.get("ip_address", "0.0.0.0" if is_stale else "192.168.0.100")),
                    "timestamp": state.get("last_seen", now),
                    "wake_word_triggered": bool(state.get("wake_word_active", False)),
                    "wake_word_label": state.get("wake_word_label", ""),
                    "first_aid_status": state.get("first_aid_status", {"box_attached": True, "delivered": False}),
                    "vision_detections": state.get("current_vision", []),
                    "map_packet": state.get("current_map_packet"),
                }
            )
        return bots

    def update_bot_from_telemetry(self, bot_id: str, kind: str, payload: Dict[str, Any]) -> None:
        norm_bot_id = canonicalize_bot_id(bot_id)
        now = datetime.now(timezone.utc)
        state = self._bot_states.setdefault(
            norm_bot_id,
            {
                "bot_id": norm_bot_id,
                "status": "MAPPING" if norm_bot_id == "Pathfinder" else "PATROL",
                "battery_pct": 100,
                "free_heap": 0,
                "psram": 0,
                "last_seen": now,
                "ip_address": "0.0.0.0",
                "wake_word_active": False,
                "wake_word_label": "",
                "first_aid_status": {"box_attached": True, "delivered": False},
                "is_real_hardware": True,
            },
        )
        state["bot_id"] = norm_bot_id
        state["is_real_hardware"] = True
        state["last_seen"] = now

        # If bot was marked OFFLINE, promote to active status on any incoming telemetry
        if state.get("status") == "OFFLINE":
            state["status"] = "MAPPING" if norm_bot_id == "Pathfinder" else "PATROL"

        if kind in {"telemetry", "status"}:
            if "status" in payload and payload["status"]:
                state["status"] = payload["status"]
            if "battery_pct" in payload:
                state["battery_pct"] = payload["battery_pct"]
            if "system_info" in payload and isinstance(payload["system_info"], dict):
                state["free_heap"] = payload["system_info"].get("free_heap", state.get("free_heap", 0))
                state["psram"] = payload["system_info"].get("psram", state.get("psram", 0))
            if "ip_address" in payload:
                state["ip_address"] = payload["ip_address"]
            if "wifi_rssi" in payload:
                state["wifi_rssi"] = payload["wifi_rssi"]
            if "wake_word_triggered" in payload:
                state["wake_word_active"] = payload["wake_word_triggered"]
            if "wake_word_label" in payload:
                state["wake_word_label"] = payload["wake_word_label"]
            if "first_aid_status" in payload:
                state["first_aid_status"] = payload["first_aid_status"]
        elif kind == "vision_detection":
            if "current_vision" not in state:
                state["current_vision"] = []
            state["current_vision"].append(payload)
            state["current_vision"] = state["current_vision"][-5:]
            if payload.get("is_threat"):
                state["status"] = "ALERT"
            if "ip_address" in payload:
                state["ip_address"] = payload["ip_address"]
        elif kind == "map_packet":
            state["current_map_packet"] = payload
            if "ip_address" in payload:
                state["ip_address"] = payload["ip_address"]

    def get_incidents(self) -> List[Dict[str, Any]]:
        return list(self._incidents)
