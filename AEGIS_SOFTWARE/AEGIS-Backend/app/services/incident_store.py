"""Persistence helpers for incidents/notifications.

This is the durable counterpart to TelemetrySimulator's in-memory rolling
list, which only keeps the last 15 incidents for the live dashboard feed and
loses everything on restart. Every incident recorded through the API also
gets saved here, so the full history survives restarts and can be queried or
exported later.
"""
from datetime import datetime
from typing import Any, Dict, List, Optional

from app.db.database import SessionLocal
from app.db.models import IncidentRecord


def _parse_timestamp(ts: Any) -> datetime:
    if isinstance(ts, datetime):
        return ts
    return datetime.fromisoformat(str(ts).replace("Z", "+00:00"))


def save_incident(incident: Dict[str, Any]) -> None:
    """Persist one incident. Upserts on id so re-delivery/retries don't duplicate rows."""
    ts = _parse_timestamp(incident["timestamp"])

    with SessionLocal() as session:
        existing = session.get(IncidentRecord, incident["id"])
        if existing is not None:
            existing.title = incident["title"]
            existing.severity = incident["severity"]
            existing.type = incident["type"]
            existing.message = incident["message"]
            existing.bot_id = incident["bot_id"]
            existing.timestamp = ts
            existing.active = incident.get("active", True)
        else:
            session.add(
                IncidentRecord(
                    id=incident["id"],
                    title=incident["title"],
                    severity=incident["severity"],
                    type=incident["type"],
                    message=incident["message"],
                    bot_id=incident["bot_id"],
                    timestamp=ts,
                    active=incident.get("active", True),
                )
            )
        session.commit()


def list_incidents(
    bot_id: Optional[str] = None,
    severity: Optional[str] = None,
    type_: Optional[str] = None,
    start: Optional[datetime] = None,
    end: Optional[datetime] = None,
    limit: Optional[int] = None,
) -> List[Dict[str, Any]]:
    """Query persisted incidents, newest first, with optional filters."""
    with SessionLocal() as session:
        query = session.query(IncidentRecord)
        if bot_id:
            query = query.filter(IncidentRecord.bot_id == bot_id)
        if severity:
            query = query.filter(IncidentRecord.severity == severity)
        if type_:
            query = query.filter(IncidentRecord.type == type_)
        if start:
            query = query.filter(IncidentRecord.timestamp >= start)
        if end:
            query = query.filter(IncidentRecord.timestamp <= end)
        query = query.order_by(IncidentRecord.timestamp.desc())
        if limit:
            query = query.limit(limit)

        return [
            {
                "id": row.id,
                "title": row.title,
                "severity": row.severity,
                "type": row.type,
                "message": row.message,
                "bot_id": row.bot_id,
                "timestamp": row.timestamp,
                "active": row.active,
            }
            for row in query.all()
        ]
