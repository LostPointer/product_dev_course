"""Конфигурация и фикстуры для тестов."""
import asyncio
import os
import pytest
from unittest.mock import AsyncMock, MagicMock, patch
from uuid import uuid4, UUID
from aiohttp import web
from aiohttp.test_utils import TestClient, TestServer

from main import create_app
from src.middleware import error_middleware


# Тестовая база данных URL (можно использовать in-memory или test DB)
TEST_DATABASE_URL = os.getenv(
    "TEST_DATABASE_URL",
    "postgresql://postgres:postgres@localhost:5432/experiment_test_db"
)


@pytest.fixture(scope="session")
def event_loop():
    """Создание event loop для тестов."""
    loop = asyncio.get_event_loop_policy().new_event_loop()
    yield loop
    loop.close()


@pytest.fixture
def mock_db_pool():
    """Мок для пула подключений к БД."""
    pool = AsyncMock()
    conn = AsyncMock()
    pool.acquire.return_value.__aenter__.return_value = conn
    pool.acquire.return_value.__aexit__.return_value = None
    return pool, conn


@pytest.fixture
def mock_user_id():
    """Тестовый user_id."""
    return uuid4()


@pytest.fixture
def auth_headers(mock_user_id):
    """Заголовки для аутентификации."""
    return {"Authorization": f"Bearer test-token-{mock_user_id}"}


@pytest.fixture
async def app(mock_user_id):
    """Создание тестового приложения."""
    # Патчим middleware чтобы пропускать аутентификацию в тестах
    app = create_app()

    # Переопределяем auth_middleware для тестов
    async def test_auth_middleware(request, handler):
        # Для публичных путей пропускаем без проверки
        if request.path == '/health':
            return await handler(request)
        # Для остальных устанавливаем тестовый user_id
        request['user_id'] = mock_user_id
        return await handler(request)

    # Заменяем middleware
    app.middlewares = [
        app.middlewares[0],  # normalize_path_middleware
        error_middleware,
        test_auth_middleware
    ]

    # Патчим инициализацию БД
    with patch('src.database.init_db'), \
         patch('src.database.close_db'):
        yield app


@pytest.fixture
async def client(app):
    """Тестовый клиент."""
    async with TestClient(TestServer(app)) as client:
        yield client


@pytest.fixture
def sample_experiment_data():
    """Пример данных для эксперимента."""
    return {
        "project_id": str(uuid4()),
        "name": "Test Experiment",
        "description": "Test description",
        "experiment_type": "aerodynamics",
        "tags": ["test", "aerodynamics"],
        "metadata": {"key": "value"}
    }


@pytest.fixture
def sample_experiment_db_record(mock_user_id):
    """Пример записи эксперимента из БД."""
    return {
        "id": uuid4(),
        "project_id": uuid4(),
        "name": "Test Experiment",
        "description": "Test description",
        "experiment_type": "aerodynamics",
        "created_by": mock_user_id,
        "status": "created",
        "tags": ["test", "aerodynamics"],
        "metadata": {"key": "value"},
        "created_at": "2024-01-01T00:00:00",
        "updated_at": "2024-01-01T00:00:00"
    }


@pytest.fixture
def sample_run_data():
    """Пример данных для run."""
    return {
        "name": "Test Run",
        "parameters": {"velocity": 100, "altitude": 5000},
        "notes": "Test notes",
        "metadata": {"run_key": "run_value"}
    }


@pytest.fixture
def sample_run_db_record():
    """Пример записи run из БД."""
    experiment_id = uuid4()
    return {
        "id": uuid4(),
        "experiment_id": experiment_id,
        "name": "Test Run",
        "parameters": {"velocity": 100, "altitude": 5000},
        "status": "created",
        "started_at": None,
        "completed_at": None,
        "duration_seconds": None,
        "notes": "Test notes",
        "metadata": {"run_key": "run_value"},
        "created_at": "2024-01-01T00:00:00",
        "updated_at": "2024-01-01T00:00:00"
    }


@pytest.fixture
def mock_publish_event():
    """Мок для публикации событий."""
    with patch('src.events.publish_event', new_callable=AsyncMock) as mock:
        yield mock


@pytest.fixture(autouse=True)
def mock_database(mock_db_pool):
    """Автоматический мок для базы данных."""
    pool, conn = mock_db_pool

    with patch('src.database.get_db_pool', return_value=pool), \
         patch('src.queries.experiments.get_db_pool', return_value=pool), \
         patch('src.queries.runs.get_db_pool', return_value=pool):
        yield pool, conn



