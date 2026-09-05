"""ORM models for persisted AEGIS data."""
from sqlalchemy import Boolean, Column, DateTime, String

from app.db.database import Base


class IncidentRecord(Base):
    """One persisted incident/notification — mirrors app.schemas.telemetry.IncidentLog."""

    __tablename__ = "incidents"

    id = Column(String, primary_key=True)
    title = Column(String, nullable=False)
    severity = Column(String, nullable=False, index=True)
    type = Column(String, nullable=False, index=True)
    message = Column(String, nullable=False)
    bot_id = Column(String, nullable=False, index=True)
    timestamp = Column(DateTime(timezone=True), nullable=False, index=True)
    active = Column(Boolean, nullable=False, default=True)
