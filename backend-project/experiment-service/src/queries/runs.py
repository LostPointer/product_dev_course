"""SQL запросы для работы с runs."""
import asyncpg
from typing import List, Dict, Optional, Any
from uuid import UUID
from datetime import datetime

from src.database import get_db_pool


async def create_run(
    experiment_id: UUID,
    name: str,
    parameters: Dict[str, Any],
    notes: Optional[str] = None,
    metadata: Optional[Dict[str, Any]] = None
) -> Dict:
    """Создание run."""
    pool = get_db_pool()

    async with pool.acquire() as conn:
        row = await conn.fetchrow("""
            INSERT INTO runs (
                experiment_id, name, parameters, notes, metadata, status
            )
            VALUES ($1, $2, $3, $4, $5, 'created')
            RETURNING id, experiment_id, name, parameters, status,
                      started_at, completed_at, duration_seconds, notes,
                      metadata, created_at, updated_at
        """, experiment_id, name, parameters, notes, metadata or {})

        return dict(row)


async def get_run_by_id(run_id: UUID) -> Optional[Dict]:
    """Получение run по ID."""
    pool = get_db_pool()

    async with pool.acquire() as conn:
        row = await conn.fetchrow("""
            SELECT id, experiment_id, name, parameters, status,
                   started_at, completed_at, duration_seconds, notes,
                   metadata, created_at, updated_at
            FROM runs
            WHERE id = $1
        """, run_id)

        return dict(row) if row else None


async def list_runs(
    experiment_id: Optional[UUID] = None,
    status: Optional[str] = None,
    limit: int = 50,
    offset: int = 0
) -> tuple[List[Dict], int]:
    """Список runs с фильтрацией."""
    pool = get_db_pool()

    async with pool.acquire() as conn:
        conditions = []
        params = []
        param_num = 1

        if experiment_id:
            conditions.append(f"experiment_id = ${param_num}")
            params.append(experiment_id)
            param_num += 1

        if status:
            conditions.append(f"status = ${param_num}")
            params.append(status)
            param_num += 1

        where_clause = " AND ".join(conditions) if conditions else "1=1"

        count_query = f"SELECT COUNT(*) FROM runs WHERE {where_clause}"
        total = await conn.fetchval(count_query, *params)

        query = f"""
            SELECT id, experiment_id, name, parameters, status,
                   started_at, completed_at, duration_seconds, notes,
                   metadata, created_at, updated_at
            FROM runs
            WHERE {where_clause}
            ORDER BY created_at DESC
            LIMIT ${param_num} OFFSET ${param_num + 1}
        """
        params.extend([limit, offset])

        rows = await conn.fetch(query, *params)
        return [dict(row) for row in rows], total


async def update_run(
    run_id: UUID,
    name: Optional[str] = None,
    parameters: Optional[Dict[str, Any]] = None,
    notes: Optional[str] = None,
    metadata: Optional[Dict[str, Any]] = None,
    status: Optional[str] = None
) -> Optional[Dict]:
    """Обновление run."""
    pool = get_db_pool()

    async with pool.acquire() as conn:
        updates = []
        params = []
        param_num = 1

        if name is not None:
            updates.append(f"name = ${param_num}")
            params.append(name)
            param_num += 1

        if parameters is not None:
            updates.append(f"parameters = ${param_num}")
            params.append(parameters)
            param_num += 1

        if notes is not None:
            updates.append(f"notes = ${param_num}")
            params.append(notes)
            param_num += 1

        if metadata is not None:
            updates.append(f"metadata = ${param_num}")
            params.append(metadata)
            param_num += 1

        if status is not None:
            updates.append(f"status = ${param_num}")
            params.append(status)

            # Автоматически устанавливаем started_at при переходе в running
            if status == 'running':
                updates.append(f"started_at = COALESCE(started_at, CURRENT_TIMESTAMP)")
            param_num += 1

        if not updates:
            return await get_run_by_id(run_id)

        updates.append(f"updated_at = CURRENT_TIMESTAMP")
        params.append(run_id)

        set_clause = ", ".join(updates)

        row = await conn.fetchrow(f"""
            UPDATE runs
            SET {set_clause}
            WHERE id = ${param_num}
            RETURNING id, experiment_id, name, parameters, status,
                      started_at, completed_at, duration_seconds, notes,
                      metadata, created_at, updated_at
        """, *params)

        return dict(row) if row else None


async def complete_run(run_id: UUID) -> Optional[Dict]:
    """Завершение run."""
    pool = get_db_pool()

    async with pool.acquire() as conn:
        row = await conn.fetchrow("""
            UPDATE runs
            SET status = 'completed',
                completed_at = CURRENT_TIMESTAMP,
                duration_seconds = EXTRACT(EPOCH FROM (CURRENT_TIMESTAMP - started_at))::INTEGER,
                updated_at = CURRENT_TIMESTAMP
            WHERE id = $1 AND status = 'running'
            RETURNING id, experiment_id, name, parameters, status,
                      started_at, completed_at, duration_seconds, notes,
                      metadata, created_at, updated_at
        """, run_id)

        return dict(row) if row else None


async def fail_run(run_id: UUID, reason: Optional[str] = None) -> Optional[Dict]:
    """Пометить run как failed."""
    pool = get_db_pool()

    async with pool.acquire() as conn:
        # Обновляем notes с причиной ошибки
        notes_update = ""
        params = [run_id]

        if reason:
            notes_update = ", notes = COALESCE(notes || E'\\n', '') || $2"
            params.append(f"Error: {reason}")

        row = await conn.fetchrow(f"""
            UPDATE runs
            SET status = 'failed',
                completed_at = CURRENT_TIMESTAMP,
                duration_seconds = CASE
                    WHEN started_at IS NOT NULL
                    THEN EXTRACT(EPOCH FROM (CURRENT_TIMESTAMP - started_at))::INTEGER
                    ELSE NULL
                END,
                updated_at = CURRENT_TIMESTAMP
                {notes_update}
            WHERE id = $1
            RETURNING id, experiment_id, name, parameters, status,
                      started_at, completed_at, duration_seconds, notes,
                      metadata, created_at, updated_at
        """, *params)

        return dict(row) if row else None

