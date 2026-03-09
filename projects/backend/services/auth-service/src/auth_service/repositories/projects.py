"""Project repository."""
from __future__ import annotations

from uuid import UUID

from auth_service.core.exceptions import NotFoundError
from auth_service.domain.models import Project
from auth_service.repositories.base import BaseRepository


class ProjectRepository(BaseRepository):
    """Repository for project operations."""

    async def create(
        self,
        name: str,
        description: str | None,
        owner_id: UUID,
    ) -> Project:
        """Create a new project."""
        query = """
            INSERT INTO projects (name, description, owner_id)
            VALUES ($1, $2, $3)
            RETURNING id, name, description, owner_id, created_at, updated_at
        """
        row = await self._fetchrow(query, name, description, owner_id)
        if not row:
            raise RuntimeError("Failed to create project")
        return Project.from_row(dict(row))

    async def get_by_id(self, project_id: UUID) -> Project | None:
        """Get project by ID."""
        query = """
            SELECT id, name, description, owner_id, created_at, updated_at
            FROM projects
            WHERE id = $1
        """
        row = await self._fetchrow(query, project_id)
        if not row:
            return None
        return Project.from_row(dict(row))

    async def get_by_id_or_raise(self, project_id: UUID) -> Project:
        """Get project by ID or raise NotFoundError."""
        project = await self.get_by_id(project_id)
        if not project:
            raise NotFoundError(f"Project {project_id} not found")
        return project

    async def list_by_user(self, user_id: UUID) -> list[Project]:
        """List all projects where user has any active role."""
        query = """
            SELECT DISTINCT p.id, p.name, p.description, p.owner_id, p.created_at, p.updated_at
            FROM projects p
            INNER JOIN user_project_roles upr ON p.id = upr.project_id
            WHERE upr.user_id = $1
              AND (upr.expires_at IS NULL OR upr.expires_at > now())
            ORDER BY p.created_at DESC
        """
        rows = await self._fetch(query, user_id)
        return [Project.from_row(dict(row)) for row in rows]

    async def list_all(self) -> list[Project]:
        """List all projects (for superadmin/admin views)."""
        query = """
            SELECT id, name, description, owner_id, created_at, updated_at
            FROM projects
            ORDER BY created_at DESC
        """
        rows = await self._fetch(query)
        return [Project.from_row(dict(row)) for row in rows]

    async def list_by_owner(self, owner_id: UUID) -> list[Project]:
        """List all projects owned by user."""
        query = """
            SELECT id, name, description, owner_id, created_at, updated_at
            FROM projects
            WHERE owner_id = $1
            ORDER BY created_at DESC
        """
        rows = await self._fetch(query, owner_id)
        return [Project.from_row(dict(row)) for row in rows]

    async def update(
        self,
        project_id: UUID,
        name: str | None = None,
        description: str | None = None,
    ) -> Project:
        """Update project."""
        updates = []
        params = []
        param_idx = 1

        if name is not None:
            updates.append(f"name = ${param_idx}")
            params.append(name)
            param_idx += 1

        if description is not None:
            updates.append(f"description = ${param_idx}")
            params.append(description)
            param_idx += 1

        if not updates:
            return await self.get_by_id_or_raise(project_id)

        params.append(str(project_id))
        query = f"""
            UPDATE projects
            SET {', '.join(updates)}, updated_at = now()
            WHERE id = ${param_idx}
            RETURNING id, name, description, owner_id, created_at, updated_at
        """
        row = await self._fetchrow(query, *params)
        if not row:
            raise NotFoundError(f"Project {project_id} not found")
        return Project.from_row(dict(row))

    async def delete(self, project_id: UUID) -> None:
        """Delete project."""
        query = "DELETE FROM projects WHERE id = $1"
        result = await self._execute(query, project_id)
        if result == "DELETE 0":
            raise NotFoundError(f"Project {project_id} not found")
