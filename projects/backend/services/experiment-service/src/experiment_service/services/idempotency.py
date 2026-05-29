"""Idempotency service."""
from __future__ import annotations

import hashlib
import json
from dataclasses import dataclass
from typing import Any, Tuple
from uuid import UUID

from aiohttp import web

from experiment_service.core.exceptions import IdempotencyConflictError
from experiment_service.repositories.idempotency import IdempotencyRecord, IdempotencyRepository

IDEMPOTENCY_HEADER = "Idempotency-Key"


@dataclass
class IdempotencyPayload:
    status: int
    body: dict[str, Any]


class IdempotencyService:
    """High-level helper for handling idempotent requests."""

    def __init__(self, repository: IdempotencyRepository):
        self._repository = repository

    @staticmethod
    def canonical_body(body: dict[str, Any]) -> Tuple[str, bytes]:
        serialized = json.dumps(body, sort_keys=True, separators=(",", ":"), default=str)
        digest = hashlib.sha256(serialized.encode("utf-8")).digest()
        return serialized, digest

    async def get_cached_response(
        self,
        key: str,
        user_id: UUID,
        request_path: str,
        body_hash: bytes,
    ) -> IdempotencyPayload | None:
        record = await self._repository.get(key)
        if record is None:
            return None
        self._assert_record(record, user_id, request_path, body_hash)
        return IdempotencyPayload(status=record.response_status, body=record.response_body)

    async def store_response(
        self,
        key: str,
        user_id: UUID,
        request_path: str,
        body_hash: bytes,
        response_status: int,
        response_body: dict[str, Any],
    ) -> IdempotencyPayload | None:
        """Persist the response, resolving concurrent same-key races.

        Returns ``None`` when this request owns the key (the caller returns its
        freshly-computed response). If a concurrent request already stored a
        response under this key, returns that stored payload so both callers
        emit an identical result; raises ``IdempotencyConflictError`` (→ 409)
        if the stored request used a different body/path/user.
        """
        inserted = await self._repository.save(
            key,
            user_id,
            request_path,
            body_hash,
            response_status,
            response_body,
        )
        if inserted:
            return None
        existing = await self._repository.get(key)
        if existing is None:
            # The winning row was removed (e.g. TTL cleanup) between INSERT and
            # SELECT — nothing left to reconcile against.
            return None
        self._assert_record(existing, user_id, request_path, body_hash)
        return IdempotencyPayload(status=existing.response_status, body=existing.response_body)

    @staticmethod
    def build_response(payload: IdempotencyPayload) -> web.Response:
        return web.json_response(payload.body, status=payload.status)

    @staticmethod
    def _assert_record(
        record: IdempotencyRecord,
        user_id: UUID,
        request_path: str,
        body_hash: bytes,
    ) -> None:
        if record.user_id != user_id or record.request_path != request_path:
            raise IdempotencyConflictError("Idempotency key belongs to another request")
        if record.request_body_hash != body_hash:
            raise IdempotencyConflictError("Idempotency key reused with different payload")



