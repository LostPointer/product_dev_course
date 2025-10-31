"""Управление подключением к базе данных."""
import asyncpg
import logging
from typing import Optional

from src.config import settings

logger = logging.getLogger(__name__)

# Глобальный pool подключений
_db_pool: Optional[asyncpg.Pool] = None


async def init_db() -> None:
    """Инициализация пула подключений к БД."""
    global _db_pool

    try:
        _db_pool = await asyncpg.create_pool(
            settings.DATABASE_URL,
            min_size=settings.DB_POOL_MIN_SIZE,
            max_size=settings.DB_POOL_MAX_SIZE,
            command_timeout=60
        )
        logger.info("Database pool created successfully")

        # Создание таблиц
        await create_tables()
        logger.info("Database tables created")

    except Exception as e:
        logger.error(f"Failed to initialize database: {e}")
        raise


async def close_db() -> None:
    """Закрытие пула подключений."""
    global _db_pool

    if _db_pool:
        await _db_pool.close()
        logger.info("Database pool closed")
        _db_pool = None


def get_db_pool() -> asyncpg.Pool:
    """Получение пула подключений."""
    if _db_pool is None:
        raise RuntimeError("Database pool is not initialized")
    return _db_pool


async def create_tables() -> None:
    """Создание таблиц в БД."""
    pool = get_db_pool()

    async with pool.acquire() as conn:
        # Таблица экспериментов
        await conn.execute("""
            CREATE TABLE IF NOT EXISTS experiments (
                id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
                project_id UUID NOT NULL,
                name VARCHAR(255) NOT NULL,
                description TEXT,
                experiment_type VARCHAR(100),  -- aerodynamics, strength, etc.
                created_by UUID NOT NULL,
                status VARCHAR(50) DEFAULT 'created',  -- created, running, completed, failed, archived
                tags TEXT[],  -- массив тегов для фильтрации
                metadata JSONB,  -- дополнительные метаданные эксперимента
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,

                CONSTRAINT status_check CHECK (status IN ('created', 'running', 'completed', 'failed', 'archived'))
            );
        """)

        # Индексы для experiments
        await conn.execute("""
            CREATE INDEX IF NOT EXISTS idx_experiments_project_id ON experiments(project_id);
            CREATE INDEX IF NOT EXISTS idx_experiments_status ON experiments(status);
            CREATE INDEX IF NOT EXISTS idx_experiments_created_by ON experiments(created_by);
            CREATE INDEX IF NOT EXISTS idx_experiments_created_at ON experiments(created_at DESC);
            CREATE INDEX IF NOT EXISTS idx_experiments_tags ON experiments USING GIN(tags);
            CREATE INDEX IF NOT EXISTS idx_experiments_metadata ON experiments USING GIN(metadata);
        """)

        # Таблица runs (запусков экспериментов)
        await conn.execute("""
            CREATE TABLE IF NOT EXISTS runs (
                id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
                experiment_id UUID NOT NULL REFERENCES experiments(id) ON DELETE CASCADE,
                name VARCHAR(255) NOT NULL,
                parameters JSONB NOT NULL,  -- параметры запуска
                status VARCHAR(50) DEFAULT 'created',  -- created, running, completed, failed
                started_at TIMESTAMP,
                completed_at TIMESTAMP,
                duration_seconds INTEGER,
                notes TEXT,
                metadata JSONB,
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,

                CONSTRAINT run_status_check CHECK (status IN ('created', 'running', 'completed', 'failed'))
            );
        """)

        # Индексы для runs
        await conn.execute("""
            CREATE INDEX IF NOT EXISTS idx_runs_experiment_id ON runs(experiment_id);
            CREATE INDEX IF NOT EXISTS idx_runs_status ON runs(status);
            CREATE INDEX IF NOT EXISTS idx_runs_started_at ON runs(started_at DESC);
            CREATE INDEX IF NOT EXISTS idx_runs_parameters ON runs USING GIN(parameters);
        """)

        # Таблица тегов (для нормализации, опционально)
        await conn.execute("""
            CREATE TABLE IF NOT EXISTS experiment_tags (
                experiment_id UUID NOT NULL REFERENCES experiments(id) ON DELETE CASCADE,
                tag VARCHAR(100) NOT NULL,
                PRIMARY KEY (experiment_id, tag)
            );

            CREATE INDEX IF NOT EXISTS idx_experiment_tags_tag ON experiment_tags(tag);
        """)

