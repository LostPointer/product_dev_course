"""Тесты для queries/experiments.py."""
import pytest
from uuid import uuid4
from unittest.mock import AsyncMock, MagicMock, patch
from datetime import datetime

from src.queries import experiments as experiment_queries


@pytest.mark.asyncio
async def test_create_experiment(mock_db_pool, mock_user_id):
    """Тест создания эксперимента."""
    pool, conn = mock_db_pool
    experiment_id = uuid4()
    project_id = uuid4()

    row_data = {
        'id': experiment_id,
        'project_id': project_id,
        'name': 'Test Experiment',
        'description': 'Test description',
        'experiment_type': 'aerodynamics',
        'created_by': mock_user_id,
        'status': 'created',
        'tags': ['test'],
        'metadata': {'key': 'value'},
        'created_at': datetime.now(),
        'updated_at': datetime.now()
    }

    mock_row = MagicMock()
    for key, value in row_data.items():
        setattr(mock_row, key, value)

    conn.fetchrow = AsyncMock(return_value=mock_row)

    with patch('src.queries.experiments.get_db_pool', return_value=pool):
        result = await experiment_queries.create_experiment(
            project_id=project_id,
            name='Test Experiment',
            created_by=mock_user_id,
            description='Test description',
            experiment_type='aerodynamics',
            tags=['test'],
            metadata={'key': 'value'}
        )

        assert result['id'] == experiment_id
        assert result['name'] == 'Test Experiment'
        assert result['status'] == 'created'
        conn.fetchrow.assert_called_once()


@pytest.mark.asyncio
async def test_get_experiment_by_id(mock_db_pool):
    """Тест получения эксперимента по ID."""
    pool, conn = mock_db_pool
    experiment_id = uuid4()

    row_data = {
        'id': experiment_id,
        'name': 'Test Experiment',
        'status': 'created'
    }

    mock_row = MagicMock()
    for key, value in row_data.items():
        setattr(mock_row, key, value)

    conn.fetchrow = AsyncMock(return_value=mock_row)

    with patch('src.queries.experiments.get_db_pool', return_value=pool):
        result = await experiment_queries.get_experiment_by_id(experiment_id)

        assert result is not None
        assert result['id'] == experiment_id
        conn.fetchrow.assert_called_once()


@pytest.mark.asyncio
async def test_get_experiment_by_id_not_found(mock_db_pool):
    """Тест получения несуществующего эксперимента."""
    pool, conn = mock_db_pool
    experiment_id = uuid4()

    conn.fetchrow = AsyncMock(return_value=None)

    with patch('src.queries.experiments.get_db_pool', return_value=pool):
        result = await experiment_queries.get_experiment_by_id(experiment_id)

        assert result is None


@pytest.mark.asyncio
async def test_list_experiments(mock_db_pool):
    """Тест получения списка экспериментов."""
    pool, conn = mock_db_pool
    experiment_id = uuid4()

    row_data = {
        'id': experiment_id,
        'name': 'Test Experiment',
        'status': 'created'
    }

    mock_row = MagicMock()
    for key, value in row_data.items():
        setattr(mock_row, key, value)

    conn.fetchval = AsyncMock(return_value=1)
    conn.fetch = AsyncMock(return_value=[mock_row])

    with patch('src.queries.experiments.get_db_pool', return_value=pool):
        experiments_list, total = await experiment_queries.list_experiments(
            limit=10,
            offset=0
        )

        assert total == 1
        assert len(experiments_list) == 1
        assert experiments_list[0]['id'] == experiment_id


@pytest.mark.asyncio
async def test_list_experiments_with_filters(mock_db_pool, mock_user_id):
    """Тест получения списка с фильтрами."""
    pool, conn = mock_db_pool
    project_id = uuid4()

    conn.fetchval = AsyncMock(return_value=0)
    conn.fetch = AsyncMock(return_value=[])

    with patch('src.queries.experiments.get_db_pool', return_value=pool):
        experiments_list, total = await experiment_queries.list_experiments(
            project_id=project_id,
            status='running',
            tags=['test'],
            created_by=mock_user_id,
            limit=10,
            offset=0
        )

        assert total == 0
        assert len(experiments_list) == 0
        # Проверяем что был вызван запрос с фильтрами
        assert conn.fetchval.call_count == 1
        assert conn.fetch.call_count == 1


@pytest.mark.asyncio
async def test_update_experiment(mock_db_pool):
    """Тест обновления эксперимента."""
    pool, conn = mock_db_pool
    experiment_id = uuid4()

    updated_row_data = {
        'id': experiment_id,
        'name': 'Updated Name',
        'status': 'running'
    }

    mock_row = MagicMock()
    for key, value in updated_row_data.items():
        setattr(mock_row, key, value)

    conn.fetchrow = AsyncMock(return_value=mock_row)

    with patch('src.queries.experiments.get_db_pool', return_value=pool):
        result = await experiment_queries.update_experiment(
            experiment_id=experiment_id,
            name='Updated Name',
            status='running'
        )

        assert result is not None
        assert result['name'] == 'Updated Name'
        assert result['status'] == 'running'


@pytest.mark.asyncio
async def test_update_experiment_no_changes(mock_db_pool):
    """Тест обновления без изменений."""
    pool, conn = mock_db_pool
    experiment_id = uuid4()

    existing_data = {
        'id': experiment_id,
        'name': 'Test',
        'status': 'created'
    }

    mock_row = MagicMock()
    for key, value in existing_data.items():
        setattr(mock_row, key, value)

    conn.fetchrow = AsyncMock(return_value=mock_row)

    with patch('src.queries.experiments.get_db_pool', return_value=pool), \
         patch.object(experiment_queries, 'get_experiment_by_id', new_callable=AsyncMock) as mock_get:
        mock_get.return_value = existing_data

        # Вызов без параметров должен вернуть существующие данные
        result = await experiment_queries.update_experiment(experiment_id=experiment_id)

        assert result is not None
        mock_get.assert_called_once_with(experiment_id)


@pytest.mark.asyncio
async def test_delete_experiment(mock_db_pool):
    """Тест удаления эксперимента."""
    pool, conn = mock_db_pool
    experiment_id = uuid4()

    conn.execute = AsyncMock(return_value="DELETE 1")

    with patch('src.queries.experiments.get_db_pool', return_value=pool):
        result = await experiment_queries.delete_experiment(experiment_id)

        assert result is True
        conn.execute.assert_called_once()


@pytest.mark.asyncio
async def test_delete_experiment_not_found(mock_db_pool):
    """Тест удаления несуществующего эксперимента."""
    pool, conn = mock_db_pool
    experiment_id = uuid4()

    conn.execute = AsyncMock(return_value="DELETE 0")

    with patch('src.queries.experiments.get_db_pool', return_value=pool):
        result = await experiment_queries.delete_experiment(experiment_id)

        assert result is False


@pytest.mark.asyncio
async def test_search_experiments(mock_db_pool):
    """Тест поиска экспериментов."""
    pool, conn = mock_db_pool
    experiment_id = uuid4()

    row_data = {
        'id': experiment_id,
        'name': 'Test Experiment',
        'status': 'created'
    }

    mock_row = MagicMock()
    for key, value in row_data.items():
        setattr(mock_row, key, value)

    conn.fetchval = AsyncMock(return_value=1)
    conn.fetch = AsyncMock(return_value=[mock_row])

    with patch('src.queries.experiments.get_db_pool', return_value=pool):
        experiments_list, total = await experiment_queries.search_experiments(
            query='test',
            limit=10,
            offset=0
        )

        assert total == 1
        assert len(experiments_list) == 1


@pytest.mark.asyncio
async def test_search_experiments_with_project_id(mock_db_pool):
    """Тест поиска с фильтром по project_id."""
    pool, conn = mock_db_pool
    project_id = uuid4()

    conn.fetchval = AsyncMock(return_value=0)
    conn.fetch = AsyncMock(return_value=[])

    with patch('src.queries.experiments.get_db_pool', return_value=pool):
        experiments_list, total = await experiment_queries.search_experiments(
            query='test',
            project_id=project_id,
            limit=10,
            offset=0
        )

        assert total == 0
        assert len(experiments_list) == 0

