"""SQLite persistence layer for AEGIS incident/notification records."""
from pathlib import Path

from sqlalchemy import create_engine
from sqlalchemy.orm import declarative_base, sessionmaker

# .../AEGIS-Backend/aegis_incidents.db — sits next to requirements.txt, not inside app/
DB_PATH = Path(__file__).resolve().parent.parent.parent / "aegis_incidents.db"

engine = create_engine(
    f"sqlite:///{DB_PATH}",
    connect_args={"check_same_thread": False},  # FastAPI can hand a request to any worker thread
)
SessionLocal = sessionmaker(bind=engine, autoflush=False, autocommit=False)
Base = declarative_base()


def init_db() -> None:
    """Create tables if they don't exist yet. Call once at app startup."""
    from app.db import models  # noqa: F401 — import so Base knows about the table before create_all

    Base.metadata.create_all(bind=engine)
