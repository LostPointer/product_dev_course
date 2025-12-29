"""Sensor repository backed by asyncpg."""
from __future__ import annotations

from typing import Any, List, Tuple
from uuid import UUID

from asyncpg import Pool, Record  # type: ignore[import-untyped]

from experiment_service.core.exceptions import NotFoundError
from experiment_service.domain.dto import SensorCreateDTO, SensorUpdateDTO
from experiment_service.domain.models import Sensor
from experiment_service.repositories.base import BaseRepository


class SensorRepository(BaseRepository):
    """CRUD operations for sensors."""

    def __init__(self, pool: Pool):
        super().__init__(pool)

    @staticmethod
    def _to_model(record: Record) -> Sensor:
        payload = dict(record)
        return Sensor.model_validate(payload)

    async def create(
        self,
        data: SensorCreateDTO,
        *,
        token_hash: bytes | None = None,
        token_preview: str | None = None,
    ) -> Sensor:
        async with self._pool.acquire() as conn:
            async with conn.transaction():
                record = await conn.fetchrow(
                    """
                    INSERT INTO sensors (
                        project_id,
                        name,
                        type,
                        input_unit,
                        display_unit,
                        status,
                        token_hash,
                        token_preview,
                        calibration_notes
                    )
                    VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9)
                    RETURNING *
                    """,
                    data.project_id,
                    data.name,
                    data.type,
                    data.input_unit,
                    data.display_unit,
                    data.status.value,
                    token_hash,
                    token_preview,
                    data.calibration_notes,
                )
                assert record is not None
                sensor = self._to_model(record)
                # Add sensor to sensor_projects table
                await conn.execute(
                    """
                    INSERT INTO sensor_projects (sensor_id, project_id, created_at)
                    VALUES ($1, $2, $3)
                    ON CONFLICT (sensor_id, project_id) DO NOTHING
                    """,
                    sensor.id,
                    data.project_id,
                    sensor.created_at,
                )
                return sensor

    async def get(self, project_id: UUID, sensor_id: UUID) -> Sensor:
        # Check if sensor exists and is associated with the project
        record = await self._fetchrow(
            """
            SELECT s.*
            FROM sensors s
            INNER JOIN sensor_projects sp ON s.id = sp.sensor_id
            WHERE s.id = $1 AND sp.project_id = $2
            """,
            sensor_id,
            project_id,
        )
        if record is None:
            raise NotFoundError("Sensor not found")
        return self._to_model(record)

    async def get_by_id(self, sensor_id: UUID) -> Sensor:
        """Get sensor by ID without project check (for internal use)."""
        record = await self._fetchrow(
            "SELECT * FROM sensors WHERE id = $1",
            sensor_id,
        )
        if record is None:
            raise NotFoundError("Sensor not found")
        return self._to_model(record)

    async def list_by_project(
        self, project_id: UUID | None = None, *, limit: int = 50, offset: int = 0
    ) -> Tuple[List[Sensor], int]:
        """List sensors by project. If project_id is None, returns all sensors."""
        if project_id is None:
            # Return all sensors (for users with access to multiple projects)
            records = await self._fetch(
                """
                SELECT DISTINCT s.*,
                       COUNT(*) OVER() AS total_count
                FROM sensors s
                INNER JOIN sensor_projects sp ON s.id = sp.sensor_id
                ORDER BY s.created_at DESC
                LIMIT $1 OFFSET $2
                """,
                limit,
                offset,
            )
        else:
            # Return sensors for a specific project
            records = await self._fetch(
                """
                SELECT s.*,
                       COUNT(*) OVER() AS total_count
                FROM sensors s
                INNER JOIN sensor_projects sp ON s.id = sp.sensor_id
                WHERE sp.project_id = $1
                ORDER BY s.created_at DESC
                LIMIT $2 OFFSET $3
                """,
                project_id,
                limit,
                offset,
            )
        items: List[Sensor] = []
        total: int | None = None
        for rec in records:
            rec_dict = dict(rec)
            total_value = rec_dict.pop("total_count", None)
            if total_value is not None:
                total = int(total_value)
            items.append(Sensor.model_validate(rec_dict))
        if total is None:
            total = await self._count_by_project(project_id)
        return items, total

    async def list_by_projects(
        self, project_ids: List[UUID], *, limit: int = 50, offset: int = 0
    ) -> Tuple[List[Sensor], int]:
        """List sensors that belong to any of the given projects."""
        if not project_ids:
            return [], 0
        records = await self._fetch(
            """
            SELECT DISTINCT s.*,
                   COUNT(*) OVER() AS total_count
            FROM sensors s
            INNER JOIN sensor_projects sp ON s.id = sp.sensor_id
            WHERE sp.project_id = ANY($1::uuid[])
            ORDER BY s.created_at DESC
            LIMIT $2 OFFSET $3
            """,
            project_ids,
            limit,
            offset,
        )
        items: List[Sensor] = []
        total: int | None = None
        for rec in records:
            rec_dict = dict(rec)
            total_value = rec_dict.pop("total_count", None)
            if total_value is not None:
                total = int(total_value)
            items.append(Sensor.model_validate(rec_dict))
        if total is None:
            total = await self._count_by_projects(project_ids)
        return items, total

    async def _count_by_project(self, project_id: UUID | None) -> int:
        if project_id is None:
            record = await self._fetchrow(
                "SELECT COUNT(DISTINCT sensor_id) AS total FROM sensor_projects"
            )
        else:
            record = await self._fetchrow(
                "SELECT COUNT(*) AS total FROM sensor_projects WHERE project_id = $1",
                project_id,
            )
        return int(record["total"]) if record else 0

    async def _count_by_projects(self, project_ids: List[UUID]) -> int:
        if not project_ids:
            return 0
        record = await self._fetchrow(
            "SELECT COUNT(DISTINCT sensor_id) AS total FROM sensor_projects WHERE project_id = ANY($1::uuid[])",
            project_ids,
        )
        return int(record["total"]) if record else 0

    async def update(
        self,
        project_id: UUID,
        sensor_id: UUID,
        updates: SensorUpdateDTO,
    ) -> Sensor:
        # Verify sensor is associated with project
        await self.get(project_id, sensor_id)
        payload = updates.model_dump(exclude_none=True)
        if not payload:
            raise ValueError("No fields provided for update")
        assignments = []
        values: list[Any] = []
        idx = 1
        for column, value in payload.items():
            if column == "status" and hasattr(value, "value"):
                value = value.value
            assignments.append(f"{column} = ${idx}")
            values.append(value)
            idx += 1
        assignments.append("updated_at = now()")
        values.append(sensor_id)
        record = await self._fetchrow(
            f"""
            UPDATE sensors
            SET {', '.join(assignments)}
            WHERE id = ${idx}
            RETURNING *
            """,
            *values,
        )
        if record is None:
            raise NotFoundError("Sensor not found")
        return self._to_model(record)

    async def delete(self, project_id: UUID, sensor_id: UUID) -> None:
        # Verify sensor is associated with project
        await self.get(project_id, sensor_id)
        # Delete sensor (cascade will remove from sensor_projects)
        record = await self._fetchrow(
            "DELETE FROM sensors WHERE id = $1 RETURNING id",
            sensor_id,
        )
        if record is None:
            raise NotFoundError("Sensor not found")

    async def rotate_token(
        self,
        project_id: UUID,
        sensor_id: UUID,
        *,
        token_hash: bytes,
        token_preview: str,
    ) -> Sensor:
        # Verify sensor is associated with project
        await self.get(project_id, sensor_id)
        record = await self._fetchrow(
            """
            UPDATE sensors
            SET token_hash = $2,
                token_preview = $3,
                updated_at = now()
            WHERE id = $1
            RETURNING *
            """,
            sensor_id,
            token_hash,
            token_preview,
        )
        if record is None:
            raise NotFoundError("Sensor not found")
        return self._to_model(record)

    async def get_by_token(self, sensor_id: UUID, token_hash: bytes) -> Sensor:
        record = await self._fetchrow(
            """
            SELECT * FROM sensors
            WHERE id = $1 AND token_hash = $2
            """,
            sensor_id,
            token_hash,
        )
        if record is None:
            raise NotFoundError("Sensor not found")
        return self._to_model(record)

    async def set_active_profile(
        self,
        project_id: UUID,
        sensor_id: UUID,
        profile_id: UUID | None,
    ) -> Sensor:
        # Verify sensor is associated with project
        await self.get(project_id, sensor_id)
        record = await self._fetchrow(
            """
            UPDATE sensors
            SET active_profile_id = $2,
                updated_at = now()
            WHERE id = $1
            RETURNING *
            """,
            sensor_id,
            profile_id,
        )
        if record is None:
            raise NotFoundError("Sensor not found")
        return self._to_model(record)

    # Methods for managing sensor-project relationships
    async def get_sensor_projects(self, sensor_id: UUID) -> List[UUID]:
        """Get all project IDs associated with a sensor."""
        records = await self._fetch(
            "SELECT project_id FROM sensor_projects WHERE sensor_id = $1 ORDER BY created_at",
            sensor_id,
        )
        return [UUID(str(record["project_id"])) for record in records]

    async def add_sensor_project(self, sensor_id: UUID, project_id: UUID) -> None:
        """Add a sensor to a project."""
        # Verify sensor exists
        await self.get_by_id(sensor_id)
        await self._fetchrow(
            """
            INSERT INTO sensor_projects (sensor_id, project_id)
            VALUES ($1, $2)
            ON CONFLICT (sensor_id, project_id) DO NOTHING
            """,
            sensor_id,
            project_id,
        )

    async def remove_sensor_project(self, sensor_id: UUID, project_id: UUID) -> None:
        """Remove a sensor from a project."""
        record = await self._fetchrow(
            """
            DELETE FROM sensor_projects
            WHERE sensor_id = $1 AND project_id = $2
            RETURNING sensor_id
            """,
            sensor_id,
            project_id,
        )
        if record is None:
            raise NotFoundError("Sensor-project relationship not found")
