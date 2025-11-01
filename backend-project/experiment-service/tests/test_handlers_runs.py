"""Тесты для handlers/runs.py."""
import pytest
from uuid import uuid4
from unittest.mock import AsyncMock, patch
from aiohttp import web
from aiohttp.test_utils import make_mocked_request

from src.handlers import runs
from src.queries import runs as run_queries
from src.queries import experiments as experiment_queries


@pytest.mark.asyncio
async def test_create_run_success(sample_experiment_db_record, sample_run_data, sample_run_db_record, mock_publish_event):
    """Тест успешного создания run."""
    experiment_id = sample_experiment_db_record['id']
    run_id = sample_run_db_record['id']

    with patch.object(experiment_queries, 'get_experiment_by_id', new_callable=AsyncMock) as mock_get_exp, \
         patch.object(run_queries, 'create_run', new_callable=AsyncMock) as mock_create:
        mock_get_exp.return_value = sample_experiment_db_record
        mock_create.return_value = sample_run_db_record

        request = make_mocked_request('POST', f'/experiments/{experiment_id}/runs', json=sample_run_data)
        request.match_info = {'experiment_id': str(experiment_id)}

        response = await runs.create_run(request)

        assert response.status == 201
        data = await response.json()
        assert data['id'] == str(run_id)
        assert data['name'] == sample_run_data['name']
        assert data['experiment_id'] == str(experiment_id)

        mock_create.assert_called_once()
        mock_publish_event.assert_called_once_with("run.created", {
            "run_id": str(run_id),
            "experiment_id": str(experiment_id)
        })


@pytest.mark.asyncio
async def test_create_run_experiment_not_found(sample_run_data):
    """Тест создания run для несуществующего эксперимента."""
    experiment_id = uuid4()

    with patch.object(experiment_queries, 'get_experiment_by_id', new_callable=AsyncMock) as mock_get_exp:
        mock_get_exp.return_value = None

        request = make_mocked_request('POST', f'/experiments/{experiment_id}/runs', json=sample_run_data)
        request.match_info = {'experiment_id': str(experiment_id)}

        with pytest.raises(web.HTTPNotFound):
            await runs.create_run(request)


@pytest.mark.asyncio
async def test_get_run_success(sample_run_db_record):
    """Тест успешного получения run."""
    run_id = sample_run_db_record['id']

    with patch.object(run_queries, 'get_run_by_id', new_callable=AsyncMock) as mock_get:
        mock_get.return_value = sample_run_db_record

        request = make_mocked_request('GET', f'/runs/{run_id}')
        request.match_info = {'run_id': str(run_id)}

        response = await runs.get_run(request)

        assert response.status == 200
        data = await response.json()
        assert data['id'] == str(run_id)
        assert data['name'] == sample_run_db_record['name']


@pytest.mark.asyncio
async def test_get_run_not_found():
    """Тест получения несуществующего run."""
    run_id = uuid4()

    with patch.object(run_queries, 'get_run_by_id', new_callable=AsyncMock) as mock_get:
        mock_get.return_value = None

        request = make_mocked_request('GET', f'/runs/{run_id}')
        request.match_info = {'run_id': str(run_id)}

        with pytest.raises(web.HTTPNotFound):
            await runs.get_run(request)


@pytest.mark.asyncio
async def test_list_runs_success(sample_experiment_db_record, sample_run_db_record):
    """Тест успешного получения списка runs."""
    experiment_id = sample_experiment_db_record['id']
    runs_list = [sample_run_db_record]
    total = 1

    with patch.object(run_queries, 'list_runs', new_callable=AsyncMock) as mock_list:
        mock_list.return_value = (runs_list, total)

        request = make_mocked_request('GET', f'/experiments/{experiment_id}/runs')
        request.match_info = {'experiment_id': str(experiment_id)}
        request.query = {}

        response = await runs.list_runs(request)

        assert response.status == 200
        data = await response.json()
        assert len(data['runs']) == 1
        assert data['total'] == 1
        assert data['page'] == 1
        assert data['page_size'] == 50


@pytest.mark.asyncio
async def test_list_runs_with_filters(sample_experiment_db_record, sample_run_db_record):
    """Тест получения списка runs с фильтрами."""
    experiment_id = sample_experiment_db_record['id']
    runs_list = [sample_run_db_record]
    total = 1

    with patch.object(run_queries, 'list_runs', new_callable=AsyncMock) as mock_list:
        mock_list.return_value = (runs_list, total)

        request = make_mocked_request('GET', f'/experiments/{experiment_id}/runs')
        request.match_info = {'experiment_id': str(experiment_id)}
        request.query = {'status': 'running', 'page': '1', 'page_size': '10'}

        response = await runs.list_runs(request)

        assert response.status == 200
        mock_list.assert_called_once()


@pytest.mark.asyncio
async def test_update_run_success(sample_run_db_record, mock_publish_event):
    """Тест успешного обновления run."""
    run_id = sample_run_db_record['id']
    update_data = {
        "name": "Updated Run Name",
        "status": "running"
    }

    updated_record = sample_run_db_record.copy()
    updated_record.update(update_data)
    updated_record['status'] = 'running'

    with patch.object(run_queries, 'get_run_by_id', new_callable=AsyncMock) as mock_get, \
         patch.object(run_queries, 'update_run', new_callable=AsyncMock) as mock_update:
        mock_get.return_value = sample_run_db_record
        mock_update.return_value = updated_record

        request = make_mocked_request('PUT', f'/runs/{run_id}', json=update_data)
        request.match_info = {'run_id': str(run_id)}

        response = await runs.update_run(request)

        assert response.status == 200
        data = await response.json()
        assert data['name'] == update_data['name']
        assert data['status'] == 'running'

        # Проверяем публикацию события при переходе в running
        mock_publish_event.assert_called_once_with("run.started", {
            "run_id": str(run_id),
            "experiment_id": str(sample_run_db_record['experiment_id'])
        })


@pytest.mark.asyncio
async def test_update_run_status_change(sample_run_db_record, mock_publish_event):
    """Тест обновления run с изменением статуса."""
    run_id = sample_run_db_record['id']
    running_record = sample_run_db_record.copy()
    running_record['status'] = 'running'

    update_data = {"status": "completed"}
    completed_record = running_record.copy()
    completed_record['status'] = 'completed'

    with patch.object(run_queries, 'get_run_by_id', new_callable=AsyncMock) as mock_get, \
         patch.object(run_queries, 'update_run', new_callable=AsyncMock) as mock_update:
        mock_get.return_value = running_record
        mock_update.return_value = completed_record

        request = make_mocked_request('PUT', f'/runs/{run_id}', json=update_data)
        request.match_info = {'run_id': str(run_id)}

        response = await runs.update_run(request)

        assert response.status == 200

        # При переходе из running в completed должно быть событие run.updated
        mock_publish_event.assert_called_once_with("run.updated", {
            "run_id": str(run_id),
            "old_status": "running",
            "new_status": "completed"
        })


@pytest.mark.asyncio
async def test_update_run_not_found():
    """Тест обновления несуществующего run."""
    run_id = uuid4()

    with patch.object(run_queries, 'get_run_by_id', new_callable=AsyncMock) as mock_get:
        mock_get.return_value = None

        request = make_mocked_request('PUT', f'/runs/{run_id}', json={"name": "New Name"})
        request.match_info = {'run_id': str(run_id)}

        with pytest.raises(web.HTTPNotFound):
            await runs.update_run(request)


@pytest.mark.asyncio
async def test_complete_run_success(sample_run_db_record, mock_publish_event):
    """Тест успешного завершения run."""
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

        request = make_mocked_request('PUT', f'/runs/{run_id}/complete')
        request.match_info = {'run_id': str(run_id)}

        response = await runs.complete_run(request)

        assert response.status == 200
        data = await response.json()
        assert data['status'] == 'completed'
        assert data['duration_seconds'] == 3600

        mock_publish_event.assert_called_once_with("run.completed", {
            "run_id": str(run_id),
            "experiment_id": str(running_record['experiment_id']),
            "duration_seconds": 3600
        })


@pytest.mark.asyncio
async def test_complete_run_not_running(sample_run_db_record):
    """Тест завершения run, который не в статусе running."""
    run_id = sample_run_db_record['id']

    with patch.object(run_queries, 'get_run_by_id', new_callable=AsyncMock) as mock_get, \
         patch.object(run_queries, 'complete_run', new_callable=AsyncMock) as mock_complete:
        mock_get.return_value = sample_run_db_record  # status = 'created'
        mock_complete.return_value = None  # Не может завершить

        request = make_mocked_request('PUT', f'/runs/{run_id}/complete')
        request.match_info = {'run_id': str(run_id)}

        with pytest.raises(web.HTTPBadRequest):
            await runs.complete_run(request)


@pytest.mark.asyncio
async def test_complete_run_not_found():
    """Тест завершения несуществующего run."""
    run_id = uuid4()

    with patch.object(run_queries, 'get_run_by_id', new_callable=AsyncMock) as mock_get:
        mock_get.return_value = None

        request = make_mocked_request('PUT', f'/runs/{run_id}/complete')
        request.match_info = {'run_id': str(run_id)}

        with pytest.raises(web.HTTPNotFound):
            await runs.complete_run(request)


@pytest.mark.asyncio
async def test_fail_run_success(sample_run_db_record, mock_publish_event):
    """Тест успешного пометки run как failed."""
    run_id = sample_run_db_record['id']
    failed_record = sample_run_db_record.copy()
    failed_record['status'] = 'failed'
    failed_record['completed_at'] = '2024-01-01T01:00:00'
    failed_record['notes'] = "Error: Test error reason"

    with patch.object(run_queries, 'get_run_by_id', new_callable=AsyncMock) as mock_get, \
         patch.object(run_queries, 'fail_run', new_callable=AsyncMock) as mock_fail:
        mock_get.return_value = sample_run_db_record
        mock_fail.return_value = failed_record

        request = make_mocked_request('PUT', f'/runs/{run_id}/fail', json={"reason": "Test error reason"})
        request.match_info = {'run_id': str(run_id)}

        response = await runs.fail_run(request)

        assert response.status == 200
        data = await response.json()
        assert data['status'] == 'failed'

        mock_fail.assert_called_once_with(run_id, reason="Test error reason")
        mock_publish_event.assert_called_once_with("run.failed", {
            "run_id": str(run_id),
            "experiment_id": str(sample_run_db_record['experiment_id']),
            "reason": "Test error reason"
        })


@pytest.mark.asyncio
async def test_fail_run_without_reason(sample_run_db_record, mock_publish_event):
    """Тест пометки run как failed без указания причины."""
    run_id = sample_run_db_record['id']
    failed_record = sample_run_db_record.copy()
    failed_record['status'] = 'failed'

    with patch.object(run_queries, 'get_run_by_id', new_callable=AsyncMock) as mock_get, \
         patch.object(run_queries, 'fail_run', new_callable=AsyncMock) as mock_fail:
        mock_get.return_value = sample_run_db_record
        mock_fail.return_value = failed_record

        request = make_mocked_request('PUT', f'/runs/{run_id}/fail')
        request.match_info = {'run_id': str(run_id)}
        request.can_read_body = False

        response = await runs.fail_run(request)

        assert response.status == 200
        mock_fail.assert_called_once_with(run_id, reason=None)


@pytest.mark.asyncio
async def test_fail_run_not_found():
    """Тест пометки несуществующего run как failed."""
    run_id = uuid4()

    with patch.object(run_queries, 'get_run_by_id', new_callable=AsyncMock) as mock_get:
        mock_get.return_value = None

        request = make_mocked_request('PUT', f'/runs/{run_id}/fail', json={"reason": "Error"})
        request.match_info = {'run_id': str(run_id)}

        with pytest.raises(web.HTTPNotFound):
            await runs.fail_run(request)

