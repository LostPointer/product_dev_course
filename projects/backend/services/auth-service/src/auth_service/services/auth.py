"""Authentication service."""
from __future__ import annotations

import secrets
from datetime import datetime, timedelta, timezone
from uuid import UUID

from auth_service.core.exceptions import (
    ConflictError,
    ForbiddenError,
    InvalidCredentialsError,
    InvalidTokenError,
    NotFoundError,
    UserAlreadyExistsError,
    UserNotFoundError,
)
from auth_service.domain.dto import AuthTokensResponse, UserResponse
from auth_service.domain.models import InviteToken, User
from auth_service.repositories.invites import InviteRepository
from auth_service.repositories.password_reset import PasswordResetRepository
from auth_service.repositories.revoked_tokens import RevokedTokenRepository
from auth_service.repositories.users import UserRepository
from auth_service.services.jwt import (
    create_access_token,
    create_refresh_token,
    decode_token,
    get_user_id_from_token,
)
from auth_service.services.password import hash_password, verify_password
from auth_service.services.permission import PermissionService


class AuthService:
    """Service for authentication operations."""

    def __init__(
        self,
        user_repository: UserRepository,
        revoked_repo: RevokedTokenRepository,
        reset_repo: PasswordResetRepository,
        permission_service: PermissionService,
        invite_repo: InviteRepository | None = None,
        registration_mode: str = "open",
    ):
        self._user_repo = user_repository
        self._revoked_repo = revoked_repo
        self._reset_repo = reset_repo
        self._perm_svc = permission_service
        self._invite_repo = invite_repo
        self._registration_mode = registration_mode

    async def bootstrap_admin(
        self,
        bootstrap_secret: str,
        expected_secret: str | None,
        username: str,
        email: str,
        password: str,
    ) -> tuple[User, AuthTokensResponse]:
        """Create the first superadmin user. Only works if no superadmin exists yet."""
        if not expected_secret or bootstrap_secret != expected_secret:
            raise ForbiddenError("Invalid bootstrap secret")

        count = await self._perm_svc.count_superadmins()
        if count > 0:
            raise ConflictError("Admin user already exists")

        if await self._user_repo.user_exists(username, email):
            raise UserAlreadyExistsError("User with this username or email already exists")

        hashed_pw = hash_password(password)
        user = await self._user_repo.create(username, email, hashed_pw, password_change_required=False)

        # Grant superadmin system role (self-grant for bootstrap, no grantor check)
        await self._perm_svc.bootstrap_grant_superadmin(user.id)

        tokens = await self._create_tokens(str(user.id))
        return user, tokens

    async def register(
        self,
        username: str,
        email: str,
        password: str,
        invite_token: UUID | None = None,
    ) -> tuple[User, AuthTokensResponse]:
        """Register a new user."""
        if self._registration_mode == "invite":
            if self._invite_repo is None:
                raise ForbiddenError("Invite system is not configured")
            if invite_token is None:
                raise ForbiddenError("Invite token required")
            invite = await self._invite_repo.get_by_token(invite_token)
            if not invite or not invite.is_active:
                raise InvalidTokenError("Invalid or expired invite token")

        if await self._user_repo.user_exists(username, email):
            raise UserAlreadyExistsError("User with this username or email already exists")

        hashed_pw = hash_password(password)
        user = await self._user_repo.create(username, email, hashed_pw, password_change_required=False)

        if self._registration_mode == "invite" and self._invite_repo is not None and invite_token is not None:
            await self._invite_repo.mark_used(invite_token, user.id)

        tokens = await self._create_tokens(str(user.id))
        return user, tokens

    async def create_invite(
        self,
        requester_id: UUID,
        email_hint: str | None,
        expires_in_hours: int,
    ) -> InviteToken:
        """Create a new invite token. Requires 'users.create' permission."""
        await self._perm_svc.ensure_permission(requester_id, "users.create")
        if self._invite_repo is None:
            raise ForbiddenError("Invite system is not configured")

        expires_at = datetime.now(timezone.utc) + timedelta(hours=expires_in_hours)
        return await self._invite_repo.create(requester_id, email_hint, expires_at)

    async def list_invites(
        self,
        requester_id: UUID,
        active_only: bool = False,
    ) -> list[InviteToken]:
        """List invite tokens. Requires 'users.create' permission."""
        await self._perm_svc.ensure_permission(requester_id, "users.create")
        if self._invite_repo is None:
            raise ForbiddenError("Invite system is not configured")

        return await self._invite_repo.list_all(active_only=active_only)

    async def revoke_invite(self, requester_id: UUID, token: UUID) -> bool:
        """Revoke (delete) an invite token. Requires 'users.create'. Cannot revoke used tokens."""
        await self._perm_svc.ensure_permission(requester_id, "users.create")
        if self._invite_repo is None:
            raise ForbiddenError("Invite system is not configured")

        invite = await self._invite_repo.get_by_token(token)
        if not invite:
            raise NotFoundError("Invite token not found")
        if invite.used_at is not None:
            raise ConflictError("Cannot revoke a used invite token")

        return await self._invite_repo.delete(token)

    async def login(self, username: str, password: str) -> tuple[User, AuthTokensResponse]:
        """Authenticate user and return tokens."""
        user = await self._user_repo.get_by_username(username)
        if not user:
            raise InvalidCredentialsError()

        if not verify_password(password, user.hashed_password):
            raise InvalidCredentialsError()

        if not user.is_active:
            raise ForbiddenError("Account is deactivated")

        tokens = await self._create_tokens(str(user.id))
        return user, tokens

    async def change_password(
        self,
        user_id: UUID,
        old_password: str,
        new_password: str,
    ) -> User:
        """Change user password."""
        user = await self._user_repo.get_by_id(user_id)
        if not user:
            raise UserNotFoundError()

        if not verify_password(old_password, user.hashed_password):
            raise InvalidCredentialsError("Invalid old password")

        new_hashed = hash_password(new_password)
        return await self._user_repo.update_password(
            user_id, new_hashed, password_change_required=False,
        )

    async def logout(self, refresh_token: str) -> None:
        """Revoke a refresh token."""
        try:
            payload = decode_token(refresh_token)
            if payload.get("type") != "refresh":
                raise ValueError("Invalid token type")
            jti = payload.get("jti")
            if not jti:
                raise ValueError("Token missing jti")
            user_id = payload.get("sub")
            if not user_id:
                raise ValueError("Token missing user ID")
            exp: int = payload["exp"]
        except ValueError as e:
            raise InvalidCredentialsError(str(e)) from e

        expires_at = datetime.fromtimestamp(exp, tz=timezone.utc)
        await self._revoked_repo.revoke(UUID(jti), UUID(user_id), expires_at)

    async def refresh_token(self, refresh_token: str) -> AuthTokensResponse:
        """Refresh access token using refresh token."""
        try:
            payload = decode_token(refresh_token)
            if payload.get("type") != "refresh":
                raise ValueError("Invalid token type")
            jti = payload.get("jti")
            if not jti:
                raise ValueError("Token missing jti")
            user_id = payload.get("sub")
            if not user_id:
                raise ValueError("Token missing user ID")
        except ValueError as e:
            raise InvalidCredentialsError(str(e)) from e

        if await self._revoked_repo.is_revoked(UUID(jti)):
            raise InvalidCredentialsError("Token has been revoked")

        user = await self._user_repo.get_by_id(UUID(user_id))
        if not user:
            raise UserNotFoundError()

        return await self._create_tokens(user_id)

    async def get_user_by_token(self, access_token: str) -> User:
        """Get user from access token."""
        try:
            user_id = get_user_id_from_token(access_token)
        except ValueError as e:
            raise InvalidCredentialsError(str(e)) from e

        user = await self._user_repo.get_by_id(UUID(user_id))
        if not user:
            raise UserNotFoundError()

        if not user.is_active:
            raise ForbiddenError("Account is deactivated")

        return user

    async def request_password_reset(self, email: str) -> tuple[str, datetime]:
        """Generate a password reset token for the given email."""
        user = await self._user_repo.get_by_email(email)
        if not user:
            raise UserNotFoundError()

        token = secrets.token_urlsafe(32)
        expires_at = datetime.now(timezone.utc) + timedelta(hours=1)
        await self._reset_repo.create_token(token, user.id, expires_at)
        return token, expires_at

    async def confirm_password_reset(self, token: str, new_password: str) -> AuthTokensResponse:
        """Reset password using a valid reset token."""
        record = await self._reset_repo.get_by_token(token)
        if not record:
            raise InvalidCredentialsError("Invalid or expired reset token")

        if record["expires_at"] < datetime.now(timezone.utc):
            raise InvalidCredentialsError("Reset token expired")

        user_id: UUID = record["user_id"]
        await self._user_repo.update_password(user_id, hash_password(new_password), password_change_required=False)
        await self._reset_repo.delete_token(token)
        return await self._create_tokens(str(user_id))

    async def admin_reset_user(
        self,
        requester_id: UUID,
        target_user_id: UUID,
        new_password: str | None,
    ) -> tuple[User, str]:
        """Admin resets another user's password. Requires 'users.reset_password'."""
        await self._perm_svc.ensure_permission(requester_id, "users.reset_password")

        target = await self._user_repo.get_by_id(target_user_id)
        if not target:
            raise UserNotFoundError()

        pwd = new_password if new_password else ("Tmp1" + secrets.token_hex(8))
        updated_user = await self._user_repo.update_password(
            target_user_id, hash_password(pwd), password_change_required=True,
        )
        return updated_user, pwd

    async def list_users(
        self,
        requester_id: UUID,
        search: str | None = None,
    ) -> list[User]:
        """List all users. Requires 'users.list' permission."""
        await self._perm_svc.ensure_permission(requester_id, "users.list")
        return await self._user_repo.list_all(search)

    async def update_user(
        self,
        requester_id: UUID,
        target_user_id: UUID,
        is_active: bool | None,
    ) -> User:
        """Update user is_active flag. Requires 'users.update' permission."""
        await self._perm_svc.ensure_permission(requester_id, "users.update")

        if requester_id == target_user_id:
            raise ForbiddenError("Cannot modify your own account")

        target = await self._user_repo.get_by_id(target_user_id)
        if not target:
            raise UserNotFoundError()

        if is_active is not None:
            target = await self._user_repo.set_active(target_user_id, is_active)

        return target

    async def delete_user(
        self,
        requester_id: UUID,
        target_user_id: UUID,
    ) -> bool:
        """Delete a user. Requires 'users.delete' permission."""
        await self._perm_svc.ensure_permission(requester_id, "users.delete")

        if requester_id == target_user_id:
            raise ForbiddenError("Cannot delete your own account")

        target = await self._user_repo.get_by_id(target_user_id)
        if not target:
            raise NotFoundError("User not found")

        # Protect last superadmin
        is_sa = await self._perm_svc.is_superadmin(target_user_id)
        if is_sa:
            count = await self._perm_svc.count_superadmins()
            if count <= 1:
                raise ConflictError("Cannot delete the last superadmin")

        deleted = await self._user_repo.delete(target_user_id)
        if not deleted:
            raise NotFoundError("User not found")
        return True

    async def get_user_response(self, user: User) -> UserResponse:
        """Build UserResponse with system role names."""
        role_names = await self._perm_svc.list_system_role_names(user.id)
        return UserResponse.from_user(user, system_roles=role_names)

    async def _create_tokens(self, user_id: str) -> AuthTokensResponse:
        """Create access and refresh tokens with RBAC v2 claims."""
        uid = UUID(user_id)
        is_sa = await self._perm_svc.is_superadmin(uid)
        
        # Get system permissions (empty if superadmin)
        system_perms: list[str] = []
        if not is_sa:
            effective = await self._perm_svc.get_effective_permissions(uid)
            system_perms = effective.system_permissions
        
        return AuthTokensResponse(
            access_token=create_access_token(user_id, is_superadmin=is_sa, system_permissions=system_perms),
            refresh_token=create_refresh_token(user_id),
        )
