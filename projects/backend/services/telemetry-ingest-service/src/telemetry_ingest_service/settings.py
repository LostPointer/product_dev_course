"""Application settings."""
from __future__ import annotations

from functools import lru_cache
from typing import cast

from pydantic import Field, PostgresDsn

from backend_common.settings.base import BaseServiceSettings


class Settings(BaseServiceSettings):
    """Core configuration for Telemetry Ingest Service."""

    app_name: str = "telemetry-ingest-service"
    port: int = 8003

    # MVP: shared DB with experiment-service
    database_url: PostgresDsn = Field(
        default=cast(PostgresDsn, "postgresql://postgres:postgres@localhost:5432/experiment_db")
    )

    # Ingest safety limits (MVP)
    telemetry_max_meta_bytes: int = 64 * 1024
    telemetry_max_reading_meta_bytes: int = 64 * 1024
    telemetry_max_batch_meta_bytes: int = 64 * 1024

    # Telemetry streaming (SSE, MVP: poll DB)
    telemetry_stream_poll_interval_seconds: float = 0.2
    telemetry_stream_heartbeat_seconds: float = 10.0


@lru_cache
def get_settings() -> Settings:
    return Settings()


settings = get_settings()

