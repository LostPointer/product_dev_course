"""Audit log (run_events) service."""
from __future__ import annotations

from typing import Any, List
from uuid import UUID

from experiment_service.domain.models import RunEvent
from experiment_service.repositories.run_events import RunEventRepository


class RunEventService:
    """Read/write operations for run audit events."""

    def __init__(self, repository: RunEventRepository):
        self._repository = repository

    async def record_event(
        self,
        *,
        run_id: UUID,
        event_type: str,
        actor_id: UUID,
        actor_role: str,
        payload: dict[str, Any] | None = None,
    ) -> RunEvent:
        return await self._repository.create(
            run_id=run_id,
            event_type=event_type,
            actor_id=actor_id,
            actor_role=actor_role,
            payload=payload,
        )

    async def list_events(
        self,
        run_id: UUID,
        *,
        limit: int = 50,
        offset: int = 0,
    ) -> tuple[List[RunEvent], int]:
        return await self._repository.list_by_run(run_id, limit=limit, offset=offset)

