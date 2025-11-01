"""Интеграционные тесты API endpoints."""
import pytest
from uuid import uuid4
from unittest.mock import AsyncMock, patch, MagicMock

from src.queries import experiments as experiment_queries
from src.queries import runs as run_queries


@pytest.mark.asyncio
@pytest.mark.integration
async def test_health_endpoint(client):
    """Тест health check endpoint."""
    resp = await client.get('/health')
    assert resp.status == 200
    data = await resp.json()
    assert data['status'] == 'healthy'
    assert data['service'] == 'experiment-service'


@pytest.mark.asyncio
@pytest.mark.integration
async def test_create_experiment_endpoint(client, sample_experiment_data, mock_user_id, mock_publish_event):
    """Тест создания эксперимента через API."""
    experiment_id = uuid4()
    project_id = uuid4()

    db_record = {
        "id": experiment_id,
        "project_id": project_id,
        "name": sample_experiment_data["name"],
        "description": sample_experiment_data["description"],
        "experiment_type": sample_experiment_data["experiment_type"],
        "created_by": mock_user_id,
        "status": "created",
        "tags": sample_experiment_data["tags"],
        "metadata": sample_experiment_data["metadata"],
        "created_at": "2024-01-01T00:00:00",
        "updated_at": "2024-01-01T00:00:00"
    }

    with patch.object(experiment_queries, 'create_experiment', new_callable=AsyncMock) as mock_create:
        mock_create.return_value = db_record

        resp = await client.post('/experiments', json=sample_experiment_data)

        assert resp.status == 201
        data = await resp.json()
        assert data['name'] == sample_experiment_data['name']
        assert 'id' in data

        mock_create.assert_called_once()
        mock_publish_event.assert_called_once()


@pytest.mark.asyncio
@pytest.mark.integration
async def test_get_experiment_endpoint(client, sample_experiment_db_record):
    """Тест получения эксперимента через API."""
    experiment_id = sample_experiment_db_record['id']

    with patch.object(experiment_queries, 'get_experiment_by_id', new_callable=AsyncMock) as mock_get:
        mock_get.return_value = sample_experiment_db_record

        resp = await client.get(f'/experiments/{experiment_id}')

        assert resp.status == 200
        data = await resp.json()
        assert data['id'] == str(experiment_id)
        assert data['name'] == sample_experiment_db_record['name']


@pytest.mark.asyncio
@pytest.mark.integration
async def test_list_experiments_endpoint(client, sample_experiment_db_record):
    """Тест получения списка экспериментов через API."""
    with patch.object(experiment_queries, 'list_experiments', new_callable=AsyncMock) as mock_list:
        mock_list.return_value = ([sample_experiment_db_record], 1)

        resp = await client.get('/experiments')

        assert resp.status == 200
        data = await resp.json()
        assert 'experiments' in data
        assert 'total' in data
        assert 'page' in data
        assert len(data['experiments']) == 1


@pytest.mark.asyncio
@pytest.mark.integration
async def test_list_experiments_with_filters(client, sample_experiment_db_record):
    """Тест фильтрации экспериментов через API."""
    project_id = uuid4()

    with patch.object(experiment_queries, 'list_experiments', new_callable=AsyncMock) as mock_list:
        mock_list.return_value = ([sample_experiment_db_record], 1)

        resp = await client.get(f'/experiments?project_id={project_id}&status=created&tags=test')

        assert resp.status == 200
        mock_list.assert_called_once()


@pytest.mark.asyncio
@pytest.mark.integration
async def test_update_experiment_endpoint(client, sample_experiment_db_record, mock_publish_event):
    """Тест обновления эксперимента через API."""
    experiment_id = sample_experiment_db_record['id']
    update_data = {"name": "Updated Name", "status": "running"}

    updated_record = sample_experiment_db_record.copy()
    updated_record.update(update_data)
    updated_record['status'] = 'running'

    with patch.object(experiment_queries, 'get_experiment_by_id', new_callable=AsyncMock) as mock_get, \
         patch.object(experiment_queries, 'update_experiment', new_callable=AsyncMock) as mock_update:
        mock_get.return_value = sample_experiment_db_record
        mock_update.return_value = updated_record

        resp = await client.put(f'/experiments/{experiment_id}', json=update_data)

        assert resp.status == 200
        data = await resp.json()
        assert data['name'] == update_data['name']
        assert data['status'] == 'running'


@pytest.mark.asyncio
@pytest.mark.integration
async def test_delete_experiment_endpoint(client, sample_experiment_db_record, mock_publish_event):
    """Тест удаления эксперимента через API."""
    experiment_id = sample_experiment_db_record['id']

    with patch.object(experiment_queries, 'get_experiment_by_id', new_callable=AsyncMock) as mock_get, \
         patch.object(experiment_queries, 'delete_experiment', new_callable=AsyncMock) as mock_delete:
        mock_get.return_value = sample_experiment_db_record
        mock_delete.return_value = True

        resp = await client.delete(f'/experiments/{experiment_id}')

        assert resp.status == 200
        data = await resp.json()
        assert data['message'] == "Experiment deleted"


@pytest.mark.asyncio
@pytest.mark.integration
async def test_create_run_endpoint(client, sample_experiment_db_record, sample_run_data, sample_run_db_record, mock_publish_event):
    """Тест создания run через API."""
    experiment_id = sample_experiment_db_record['id']

    with patch.object(experiment_queries, 'get_experiment_by_id', new_callable=AsyncMock) as mock_get_exp, \
         patch.object(run_queries, 'create_run', new_callable=AsyncMock) as mock_create:
        mock_get_exp.return_value = sample_experiment_db_record
        mock_create.return_value = sample_run_db_record

        resp = await client.post(f'/experiments/{experiment_id}/runs', json=sample_run_data)

        assert resp.status == 201
        data = await resp.json()
        assert data['name'] == sample_run_data['name']
        assert 'id' in data

        mock_create.assert_called_once()
        mock_publish_event.assert_called_once()


@pytest.mark.asyncio
@pytest.mark.integration
async def test_get_run_endpoint(client, sample_run_db_record):
    """Тест получения run через API."""
    run_id = sample_run_db_record['id']

    with patch.object(run_queries, 'get_run_by_id', new_callable=AsyncMock) as mock_get:
        mock_get.return_value = sample_run_db_record

        resp = await client.get(f'/runs/{run_id}')

        assert resp.status == 200
        data = await resp.json()
        assert data['id'] == str(run_id)
        assert data['name'] == sample_run_db_record['name']


@pytest.mark.asyncio
@pytest.mark.integration
async def test_list_runs_endpoint(client, sample_experiment_db_record, sample_run_db_record):
    """Тест получения списка runs через API."""
    experiment_id = sample_experiment_db_record['id']

    with patch.object(run_queries, 'list_runs', new_callable=AsyncMock) as mock_list:
        mock_list.return_value = ([sample_run_db_record], 1)

        resp = await client.get(f'/experiments/{experiment_id}/runs')

        assert resp.status == 200
        data = await resp.json()
        assert 'runs' in data
        assert 'total' in data
        assert len(data['runs']) == 1


@pytest.mark.asyncio
@pytest.mark.integration
async def test_complete_run_endpoint(client, sample_run_db_record, mock_publish_event):
    """Тест завершения run через API."""
    run_id = sample_run_db_record['id']
    running_record = sample_run_db_record.copy()
    running_record['status'] = 'running'
    running_record['started_at'] = '2024-01-01T00:00:00'

    completed_record = running_record.copy()
    completed_record['status'] = 'completed'
    completed_record['completed_at'] = '2024-01-01T01:00:00'
    completed_record['duration_seconds'] = 3600

    with patch.object(run_queries, 'get_run_by_id', new_callable=AsyncMock) as mock_get, \
         patch.object(run_queries, 'complete_run', new_callable=AsyncMock) as mock_complete:
        mock_get.return_value = running_record
        mock_complete.return_value = completed_record

        resp = await client.put(f'/runs/{run_id}/complete')

        assert resp.status == 200
        data = await resp.json()
        assert data['status'] == 'completed'
        assert data['duration_seconds'] == 3600


@pytest.mark.asyncio
@pytest.mark.integration
async def test_fail_run_endpoint(client, sample_run_db_record, mock_publish_event):
    """Тест пометки run как failed через API."""
    run_id = sample_run_db_record['id']
    failed_record = sample_run_db_record.copy()
    failed_record['status'] = 'failed'

    with patch.object(run_queries, 'get_run_by_id', new_callable=AsyncMock) as mock_get, \
         patch.object(run_queries, 'fail_run', new_callable=AsyncMock) as mock_fail:
        mock_get.return_value = sample_run_db_record
        mock_fail.return_value = failed_record

        resp = await client.put(f'/runs/{run_id}/fail', json={"reason": "Test error"})

        assert resp.status == 200
        data = await resp.json()
        assert data['status'] == 'failed'


@pytest.mark.asyncio
@pytest.mark.integration
async def test_not_found_endpoints(client):
    """Тест 404 для несуществующих ресурсов."""
    non_existent_id = uuid4()

    with patch.object(experiment_queries, 'get_experiment_by_id', new_callable=AsyncMock) as mock_get:
        mock_get.return_value = None

        resp = await client.get(f'/experiments/{non_existent_id}')
        assert resp.status == 404

    with patch.object(run_queries, 'get_run_by_id', new_callable=AsyncMock) as mock_get:
        mock_get.return_value = None

        resp = await client.get(f'/runs/{non_existent_id}')
        assert resp.status == 404


@pytest.mark.asyncio
@pytest.mark.integration
async def test_invalid_request_data(client, mock_user_id):
    """Тест обработки невалидных данных запроса."""
    # Пустое имя
    invalid_data = {
        "project_id": str(uuid4()),
        "name": ""  # Пустое имя недопустимо
    }

    resp = await client.post('/experiments', json=invalid_data)
    assert resp.status == 400 or resp.status == 422  # Валидационная ошибка

    # Отсутствующий project_id
    invalid_data2 = {
        "name": "Test"
    }

    resp = await client.post('/experiments', json=invalid_data2)
    assert resp.status == 400 or resp.status == 422

