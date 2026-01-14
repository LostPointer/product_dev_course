"""Database migrations helpers shared between services."""
from __future__ import annotations

import asyncio
import hashlib
from pathlib import Path
from typing import Any, Awaitable, Callable, Iterable, Protocol

import asyncpg
from aiohttp import web


class SettingsProtocol(Protocol):
    """Protocol for settings objects with database configuration."""

    database_url: Any


def _find_migrations_dir(possible_paths: list[Path]) -> Path | None:
    for path in possible_paths:
        if path.exists():
            return path
    return None


def create_migration_runner(
    settings: SettingsProtocol,
    possible_paths: Iterable[Path],
    *,
    create_db_hint: str | None = None,
) -> Callable[[web.Application], Awaitable[None]]:
    """Create an aiohttp startup hook to apply SQL migrations."""
    possible_paths_list = list(possible_paths)

    async def apply_migrations_on_startup(_app: web.Application) -> None:
        """Apply pending migrations on startup."""
        migrations_dir = _find_migrations_dir(possible_paths_list)
        if migrations_dir is None:
            print(f"⚠️  Migrations directory not found. Tried: {possible_paths_list}, skipping migrations")
            return

        migrations: dict[str, Path] = {}
        for path in sorted(migrations_dir.glob("*.sql")):
            version = path.stem
            if version in migrations:
                raise ValueError(f"Duplicate migration version detected: {version}")
            migrations[version] = path

        if not migrations:
            print("⚠️  No migrations found, skipping")
            return

        max_retries = 5
        retry_delay = 2
        conn = None
        for attempt in range(max_retries):
            try:
                conn = await asyncpg.connect(str(settings.database_url))
                break
            except asyncpg.exceptions.InvalidCatalogNameError as e:
                if "does not exist" in str(e):
                    print(f"⚠️  Database does not exist yet (attempt {attempt + 1}/{max_retries})")
                    if attempt < max_retries - 1:
                        await asyncio.sleep(retry_delay)
                        continue
                    print("❌ Database does not exist. Please create it manually or restart PostgreSQL container.")
                    if create_db_hint:
                        print(f"   You can create it with: {create_db_hint}")
                    return
                raise
            except Exception as e:
                print(f"⚠️  Database connection error (attempt {attempt + 1}/{max_retries}): {e}")
                if attempt < max_retries - 1:
                    await asyncio.sleep(retry_delay)
                    continue
                print("❌ Failed to connect to database after retries. Skipping migrations.")
                return

        if conn is None:
            print("❌ Failed to establish database connection. Skipping migrations.")
            return

        try:
            await conn.execute(
                """
                CREATE TABLE IF NOT EXISTS schema_migrations (
                    version text PRIMARY KEY,
                    checksum text NOT NULL,
                    applied_at timestamptz NOT NULL DEFAULT now()
                );
                """
            )
            rows = await conn.fetch("SELECT version, checksum FROM schema_migrations")
            applied = {row["version"]: row["checksum"] for row in rows}
            pending = []

            for version, path in migrations.items():
                sql = path.read_text(encoding="utf-8")
                checksum = hashlib.sha256(sql.encode("utf-8")).hexdigest()
                if version in applied:
                    if applied[version] != checksum:
                        raise RuntimeError(
                            f"Checksum mismatch for {version}: "
                            f"{applied[version]} (db) != {checksum} (file)"
                        )
                    continue
                pending.append((version, path, sql, checksum))

            if not pending:
                print("✅ No pending migrations.")
                return

            print(f"🔄 Applying {len(pending)} pending migration(s)...")
            for version, path, sql, checksum in pending:
                print(f"   Applying {path.name}...")
                try:
                    async with conn.transaction():
                        await conn.execute(sql)
                        await conn.execute(
                            "INSERT INTO schema_migrations (version, checksum) VALUES ($1, $2)",
                            version,
                            checksum,
                        )
                except (asyncpg.exceptions.DuplicateObjectError, asyncpg.exceptions.DuplicateTableError) as e:
                    if "already exists" in str(e).lower():
                        existing = await conn.fetchval(
                            "SELECT version FROM schema_migrations WHERE version = $1", version
                        )
                        if existing:
                            print(f"   ⚠️  Migration {version} already applied, skipping")
                            continue
                        print(f"   ⚠️  Objects already exist for {version}, marking as applied")
                        await conn.execute(
                            "INSERT INTO schema_migrations (version, checksum) VALUES ($1, $2) "
                            "ON CONFLICT (version) DO NOTHING",
                            version,
                            checksum,
                        )
                    else:
                        raise
            print(f"✅ Applied {len(pending)} migration(s).")
        finally:
            await conn.close()

    return apply_migrations_on_startup
