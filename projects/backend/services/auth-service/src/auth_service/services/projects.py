"""Project service."""
from __future__ import annotations

from uuid import UUID

from auth_service.core.exceptions import ForbiddenError, NotFoundError
from auth_service.domain.models import PROJECT_OWNER_ROLE_ID, Project, UserProjectRole
from auth_service.repositories.projects import ProjectRepository
from auth_service.repositories.users import UserRepository
from auth_service.repositories.user_roles import UserRoleRepository
from auth_service.services.permission import PermissionService


class ProjectService:
    """Service for project operations."""

    def __init__(
        self,
        project_repo: ProjectRepository,
        user_repo: UserRepository,
        user_role_repo: UserRoleRepository,
        permission_service: PermissionService,
    ) -> None:
        self.project_repo = project_repo
        self.user_repo = user_repo
        self.user_role_repo = user_role_repo
        self.perm_svc = permission_service

    async def create_project(
        self,
        name: str,
        description: str | None,
        owner_id: UUID,
    ) -> Project:
        """Create a new project. Any authenticated user can create a project."""
        # Check if user exists
        user = await self.user_repo.get_by_id(owner_id)
        if not user:
            raise NotFoundError(f"User {owner_id} not found")

        # Create project (trigger will automatically add owner as member)
        return await self.project_repo.create(name, description, owner_id)

    async def get_project(self, project_id: UUID, user_id: UUID) -> Project:
        """Get project by ID (user must have 'project.members.view' permission)."""
        await self.perm_svc.ensure_permission(user_id, "project.members.view", project_id)
        
        project = await self.project_repo.get_by_id(project_id)
        if not project:
            raise NotFoundError(f"Project {project_id} not found")

        return project

    async def list_user_projects(self, user_id: UUID) -> list[Project]:
        """List all projects where user is a member."""
        return await self.project_repo.list_by_user(user_id)

    async def list_all_projects(self, user_id: UUID) -> list[Project]:
        """List all projects. Requires 'projects.list' or superadmin."""
        # Check if user has system-level permission to list all projects
        # For now, allow if superadmin or has projects.list (if such permission exists)
        # Otherwise fall back to list_user_projects
        if await self.user_role_repo.is_superadmin(user_id):
            return await self.project_repo.list_all()
        return await self.project_repo.list_by_user(user_id)

    async def update_project(
        self,
        project_id: UUID,
        user_id: UUID,
        name: str | None = None,
        description: str | None = None,
    ) -> Project:
        """Update project. Requires 'project.settings.update' permission."""
        await self.perm_svc.ensure_permission(user_id, "project.settings.update", project_id)
        
        project = await self.project_repo.get_by_id(project_id)
        if not project:
            raise NotFoundError(f"Project {project_id} not found")

        return await self.project_repo.update(project_id, name, description)

    async def delete_project(self, project_id: UUID, user_id: UUID) -> None:
        """Delete project. Requires 'project.settings.delete' permission."""
        await self.perm_svc.ensure_permission(user_id, "project.settings.delete", project_id)
        
        project = await self.project_repo.get_by_id(project_id)
        if not project:
            raise NotFoundError(f"Project {project_id} not found")

        await self.project_repo.delete(project_id)

    async def add_member(
        self,
        project_id: UUID,
        requester_id: UUID,
        new_user_id: UUID,
        role_id: UUID,
    ) -> UserProjectRole:
        """Add member to project. Requires 'project.members.invite' permission."""
        await self.perm_svc.ensure_permission(requester_id, "project.members.invite", project_id)
        
        # Check if new user exists
        new_user = await self.user_repo.get_by_id(new_user_id)
        if not new_user:
            raise NotFoundError(f"User {new_user_id} not found")

        # Validate that role is project-scoped (prevents assigning system roles as project roles)
        await self.perm_svc.validate_project_role(role_id)

        return await self.user_role_repo.grant_project_role(
            new_user_id, project_id, role_id, requester_id,
        )

    async def remove_member(
        self,
        project_id: UUID,
        requester_id: UUID,
        member_user_id: UUID,
        role_id: UUID,
    ) -> bool:
        """Remove member from project. Requires 'project.members.remove' permission."""
        await self.perm_svc.ensure_permission(requester_id, "project.members.remove", project_id)
        
        project = await self.project_repo.get_by_id(project_id)
        if not project:
            raise NotFoundError(f"Project {project_id} not found")

        # Cannot remove owner
        if member_user_id == project.owner_id:
            raise ForbiddenError("Cannot remove project owner")

        return await self.user_role_repo.revoke_project_role(
            member_user_id, project_id, role_id,
        )

    async def update_member_role(
        self,
        project_id: UUID,
        requester_id: UUID,
        member_user_id: UUID,
        old_role_id: UUID,
        new_role_id: UUID,
    ) -> UserProjectRole:
        """Update member role. Requires 'project.members.change_role' permission."""
        # grant_project_role checks the permission and validates scope_type
        new_assignment = await self.perm_svc.grant_project_role(
            requester_id, project_id, member_user_id, new_role_id,
        )
        # Revoke old role after grant succeeds
        await self.user_role_repo.revoke_project_role(
            member_user_id, project_id, old_role_id,
        )
        return new_assignment

    async def list_members(self, project_id: UUID, user_id: UUID) -> list[dict]:
        """List project members. Requires 'project.members.view' permission."""
        await self.perm_svc.ensure_permission(user_id, "project.members.view", project_id)
        
        return await self.user_role_repo.list_project_members(project_id)

