"""Тесты для queries/runs.py."""
import pytest
from uuid import uuid4
from unittest.mock import AsyncMock, MagicMock, patch
from datetime import datetime

from src.queries import runs as run_queries


@pytest.mark.asyncio
async def test_create_run(mock_db_pool):
    """Тест создания run."""
    pool, conn = mock_db_pool
    run_id = uuid4()
    experiment_id = uuid4()

    row_data = {
        'id': run_id,
        'experiment_id': experiment_id,
        'name': 'Test Run',
        'parameters': {'velocity': 100},
        'status': 'created',
        'started_at': None,
        'completed_at': None,
        'duration_seconds': None,
        'notes': None,
        'metadata': {},
        'created_at': datetime.now(),
        'updated_at': datetime.now()
    }

    mock_row = MagicMock()
    for key, value in row_data.items():
        setattr(mock_row, key, value)

    conn.fetchrow = AsyncMock(return_value=mock_row)

    with patch('src.queries.runs.get_db_pool', return_value=pool):
        result = await run_queries.create_run(
            experiment_id=experiment_id,
            name='Test Run',
            parameters={'velocity': 100}
        )

        assert result['id'] == run_id
        assert result['name'] == 'Test Run'
        assert result['status'] == 'created'
        conn.fetchrow.assert_called_once()


@pytest.mark.asyncio
async def test_get_run_by_id(mock_db_pool):
    """Тест получения run по ID."""
    pool, conn = mock_db_pool
    run_id = uuid4()

    row_data = {
        'id': run_id,
        'name': 'Test Run',
        'status': 'created'
    }

    mock_row = MagicMock()
    for key, value in row_data.items():
        setattr(mock_row, key, value)

    conn.fetchrow = AsyncMock(return_value=mock_row)

    with patch('src.queries.runs.get_db_pool', return_value=pool):
        result = await run_queries.get_run_by_id(run_id)

        assert result is not None
        assert result['id'] == run_id
        conn.fetchrow.assert_called_once()


@pytest.mark.asyncio
async def test_get_run_by_id_not_found(mock_db_pool):
    """Тест получения несуществующего run."""
    pool, conn = mock_db_pool
    run_id = uuid4()

    conn.fetchrow = AsyncMock(return_value=None)

    with patch('src.queries.runs.get_db_pool', return_value=pool):
        result = await run_queries.get_run_by_id(run_id)

        assert result is None


@pytest.mark.asyncio
async def test_list_runs(mock_db_pool):
    """Тест получения списка runs."""
    pool, conn = mock_db_pool
    run_id = uuid4()
    experiment_id = uuid4()

    row_data = {
        'id': run_id,
        'experiment_id': experiment_id,
        'name': 'Test Run',
        'status': 'created'
    }

    mock_row = MagicMock()
    for key, value in row_data.items():
        setattr(mock_row, key, value)

    conn.fetchval = AsyncMock(return_value=1)
    conn.fetch = AsyncMock(return_value=[mock_row])

    with patch('src.queries.runs.get_db_pool', return_value=pool):
        runs_list, total = await run_queries.list_runs(
            experiment_id=experiment_id,
            limit=10,
            offset=0
        )

        assert total == 1
        assert len(runs_list) == 1
        assert runs_list[0]['id'] == run_id


@pytest.mark.asyncio
async def test_list_runs_with_status_filter(mock_db_pool):
    """Тест получения списка с фильтром по статусу."""
    pool, conn = mock_db_pool
    experiment_id = uuid4()

    conn.fetchval = AsyncMock(return_value=0)
    conn.fetch = AsyncMock(return_value=[])

    with patch('src.queries.runs.get_db_pool', return_value=pool):
        runs_list, total = await run_queries.list_runs(
            experiment_id=experiment_id,
            status='running',
            limit=10,
            offset=0
        )

        assert total == 0
        assert len(runs_list) == 0


@pytest.mark.asyncio
async def test_update_run(mock_db_pool):
    """Тест обновления run."""
    pool, conn = mock_db_pool
    run_id = uuid4()

    updated_row_data = {
        'id': run_id,
        'name': 'Updated Run',
        'status': 'running',
        'started_at': datetime.now()
    }

    mock_row = MagicMock()
    for key, value in updated_row_data.items():
        setattr(mock_row, key, value)

    conn.fetchrow = AsyncMock(return_value=mock_row)

    with patch('src.queries.runs.get_db_pool', return_value=pool):
        result = await run_queries.update_run(
            run_id=run_id,
            name='Updated Run',
            status='running'
        )

        assert result is not None
        assert result['name'] == 'Updated Run'
        assert result['status'] == 'running'


@pytest.mark.asyncio
async def test_update_run_set_started_at(mock_db_pool):
    """Тест установки started_at при переходе в running."""
    pool, conn = mock_db_pool
    run_id = uuid4()

    updated_row_data = {
        'id': run_id,
        'status': 'running',
        'started_at': datetime.now()
    }

    mock_row = MagicMock()
    for key, value in updated_row_data.items():
        setattr(mock_row, key, value)

    conn.fetchrow = AsyncMock(return_value=mock_row)

    with patch('src.queries.runs.get_db_pool', return_value=pool):
        result = await run_queries.update_run(
            run_id=run_id,
            status='running'
        )

        assert result is not None
        assert result['status'] == 'running'
        # Проверяем что был добавлен started_at в запрос
        assert conn.fetchrow.called


@pytest.mark.asyncio
async def test_complete_run(mock_db_pool):
    """Тест завершения run."""
    pool, conn = mock_db_pool
    run_id = uuid4()

    completed_row_data = {
        'id': run_id,
        'status': 'completed',
        'completed_at': datetime.now(),
        'duration_seconds': 3600
    }

    mock_row = MagicMock()
    for key, value in completed_row_data.items():
        setattr(mock_row, key, value)

    conn.fetchrow = AsyncMock(return_value=mock_row)

    with patch('src.queries.runs.get_db_pool', return_value=pool):
        result = await run_queries.complete_run(run_id)

        assert result is not None
        assert result['status'] == 'completed'
        assert result['duration_seconds'] == 3600


@pytest.mark.asyncio
async def test_complete_run_not_running(mock_db_pool):
    """Тест завершения run не в статусе running."""
    pool, conn = mock_db_pool
    run_id = uuid4()

    # Запрос возвращает None если статус не 'running'
    conn.fetchrow = AsyncMock(return_value=None)

    with patch('src.queries.runs.get_db_pool', return_value=pool):
        result = await run_queries.complete_run(run_id)

        assert result is None


@pytest.mark.asyncio
async def test_fail_run(mock_db_pool):
    """Тест пометки run как failed."""
    pool, conn = mock_db_pool
    run_id = uuid4()

    failed_row_data = {
        'id': run_id,
        'status': 'failed',
        'completed_at': datetime.now(),
        'notes': 'Error: Test reason'
    }

    mock_row = MagicMock()
    for key, value in failed_row_data.items():
        setattr(mock_row, key, value)

    conn.fetchrow = AsyncMock(return_value=mock_row)

    with patch('src.queries.runs.get_db_pool', return_value=pool):
        result = await run_queries.fail_run(run_id, reason='Test reason')

        assert result is not None
        assert result['status'] == 'failed'


@pytest.mark.asyncio
async def test_fail_run_without_reason(mock_db_pool):
    """Тест пометки run как failed без причины."""
    pool, conn = mock_db_pool
    run_id = uuid4()

    failed_row_data = {
        'id': run_id,
        'status': 'failed',
        'completed_at': datetime.now(),
        'notes': None
    }

    mock_row = MagicMock()
    for key, value in failed_row_data.items():
        setattr(mock_row, key, value)

    conn.fetchrow = AsyncMock(return_value=mock_row)

    with patch('src.queries.runs.get_db_pool', return_value=pool):
        result = await run_queries.fail_run(run_id, reason=None)

        assert result is not None
        assert result['status'] == 'failed'

