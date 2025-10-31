"""SQL запросы для работы с экспериментами."""
import asyncpg
from typing import List, Dict, Optional, Any
from uuid import UUID
from datetime import datetime

from src.database import get_db_pool


async def create_experiment(
    project_id: UUID,
    name: str,
    created_by: UUID,
    description: Optional[str] = None,
    experiment_type: Optional[str] = None,
    tags: Optional[List[str]] = None,
    metadata: Optional[Dict[str, Any]] = None
) -> Dict:
    """Создание эксперимента."""
    pool = get_db_pool()

    async with pool.acquire() as conn:
        row = await conn.fetchrow("""
            INSERT INTO experiments (
                project_id, name, description, experiment_type,
                created_by, tags, metadata, status
            )
            VALUES ($1, $2, $3, $4, $5, $6, $7, 'created')
            RETURNING id, project_id, name, description, experiment_type,
                      created_by, status, tags, metadata, created_at, updated_at
        """, project_id, name, description, experiment_type,
            created_by, tags or [], metadata or {})

        return dict(row)


async def get_experiment_by_id(experiment_id: UUID) -> Optional[Dict]:
    """Получение эксперимента по ID."""
    pool = get_db_pool()

    async with pool.acquire() as conn:
        row = await conn.fetchrow("""
            SELECT id, project_id, name, description, experiment_type,
                   created_by, status, tags, metadata, created_at, updated_at
            FROM experiments
            WHERE id = $1
        """, experiment_id)

        return dict(row) if row else None


async def list_experiments(
    project_id: Optional[UUID] = None,
    status: Optional[str] = None,
    tags: Optional[List[str]] = None,
    created_by: Optional[UUID] = None,
    limit: int = 50,
    offset: int = 0
) -> tuple[List[Dict], int]:
    """Список экспериментов с фильтрацией."""
    pool = get_db_pool()

    async with pool.acquire() as conn:
        # Построение WHERE условий
        conditions = []
        params = []
        param_num = 1

        if project_id:
            conditions.append(f"project_id = ${param_num}")
            params.append(project_id)
            param_num += 1

        if status:
            conditions.append(f"status = ${param_num}")
            params.append(status)
            param_num += 1

        if created_by:
            conditions.append(f"created_by = ${param_num}")
            params.append(created_by)
            param_num += 1

        if tags:
            conditions.append(f"tags && ${param_num}")  # Пересечение массивов
            params.append(tags)
            param_num += 1

        where_clause = " AND ".join(conditions) if conditions else "1=1"

        # Подсчет общего количества
        count_query = f"SELECT COUNT(*) FROM experiments WHERE {where_clause}"
        total = await conn.fetchval(count_query, *params)

        # Получение данных
        query = f"""
            SELECT id, project_id, name, description, experiment_type,
                   created_by, status, tags, metadata, created_at, updated_at
            FROM experiments
            WHERE {where_clause}
            ORDER BY created_at DESC
            LIMIT ${param_num} OFFSET ${param_num + 1}
        """
        params.extend([limit, offset])

        rows = await conn.fetch(query, *params)
        return [dict(row) for row in rows], total


async def update_experiment(
    experiment_id: UUID,
    name: Optional[str] = None,
    description: Optional[str] = None,
    experiment_type: Optional[str] = None,
    tags: Optional[List[str]] = None,
    metadata: Optional[Dict[str, Any]] = None,
    status: Optional[str] = None
) -> Optional[Dict]:
    """Обновление эксперимента."""
    pool = get_db_pool()

    async with pool.acquire() as conn:
        # Построение SET условий
        updates = []
        params = []
        param_num = 1

        if name is not None:
            updates.append(f"name = ${param_num}")
            params.append(name)
            param_num += 1

        if description is not None:
            updates.append(f"description = ${param_num}")
            params.append(description)
            param_num += 1

        if experiment_type is not None:
            updates.append(f"experiment_type = ${param_num}")
            params.append(experiment_type)
            param_num += 1

        if tags is not None:
            updates.append(f"tags = ${param_num}")
            params.append(tags)
            param_num += 1

        if metadata is not None:
            updates.append(f"metadata = ${param_num}")
            params.append(metadata)
            param_num += 1

        if status is not None:
            updates.append(f"status = ${param_num}")
            params.append(status)
            param_num += 1

        if not updates:
            return await get_experiment_by_id(experiment_id)

        updates.append(f"updated_at = CURRENT_TIMESTAMP")
        params.append(experiment_id)

        set_clause = ", ".join(updates)

        row = await conn.fetchrow(f"""
            UPDATE experiments
            SET {set_clause}
            WHERE id = ${param_num}
            RETURNING id, project_id, name, description, experiment_type,
                      created_by, status, tags, metadata, created_at, updated_at
        """, *params)

        return dict(row) if row else None


async def delete_experiment(experiment_id: UUID) -> bool:
    """Удаление эксперимента."""
    pool = get_db_pool()

    async with pool.acquire() as conn:
        result = await conn.execute("""
            DELETE FROM experiments WHERE id = $1
        """, experiment_id)

        return result == "DELETE 1"


async def search_experiments(
    query: Optional[str] = None,
    project_id: Optional[UUID] = None,
    limit: int = 50,
    offset: int = 0
) -> tuple[List[Dict], int]:
    """Поиск экспериментов по тексту."""
    pool = get_db_pool()

    async with pool.acquire() as conn:
        conditions = []
        params = []
        param_num = 1

        if project_id:
            conditions.append(f"project_id = ${param_num}")
            params.append(project_id)
            param_num += 1

        if query:
            conditions.append(f"""
                (name ILIKE ${param_num} OR
                 description ILIKE ${param_num} OR
                 experiment_type ILIKE ${param_num})
            """)
            search_pattern = f"%{query}%"
            params.extend([search_pattern, search_pattern, search_pattern])
            param_num += 3

        where_clause = " AND ".join(conditions) if conditions else "1=1"

        count_query = f"SELECT COUNT(*) FROM experiments WHERE {where_clause}"
        total = await conn.fetchval(count_query, *params)

        data_query = f"""
            SELECT id, project_id, name, description, experiment_type,
                   created_by, status, tags, metadata, created_at, updated_at
            FROM experiments
            WHERE {where_clause}
            ORDER BY created_at DESC
            LIMIT ${param_num} OFFSET ${param_num + 1}
        """
        params.extend([limit, offset])

        rows = await conn.fetch(data_query, *params)
        return [dict(row) for row in rows], total

