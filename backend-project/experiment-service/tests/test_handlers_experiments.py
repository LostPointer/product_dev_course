"""Тесты для handlers/experiments.py."""
import pytest
from uuid import uuid4
from unittest.mock import AsyncMock, patch
from aiohttp import web
from aiohttp.test_utils import make_mocked_request

from src.handlers import experiments
from src.queries import experiments as experiment_queries


@pytest.mark.asyncio
async def test_create_experiment_success(mock_user_id, sample_experiment_data, mock_publish_event):
    """Тест успешного создания эксперимента."""
    experiment_id = uuid4()
    project_id = uuid4()

    # Мок результата запроса
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

        request = make_mocked_request('POST', '/experiments', json=sample_experiment_data)
        request['user_id'] = mock_user_id

        response = await experiments.create_experiment(request)

        assert response.status == 201
        data = await response.json()
        assert data['id'] == str(experiment_id)
        assert data['name'] == sample_experiment_data['name']
        assert data['project_id'] == str(project_id)

        # Проверяем вызов create_experiment
        mock_create.assert_called_once()
        mock_publish_event.assert_called_once_with("experiment.created", {
            "experiment_id": str(experiment_id),
            "project_id": str(project_id),
            "created_by": str(mock_user_id)
        })


@pytest.mark.asyncio
async def test_create_experiment_unauthorized():
    """Тест создания эксперимента без авторизации."""
    request = make_mocked_request('POST', '/experiments', json={})
    request['user_id'] = None

    with pytest.raises(web.HTTPUnauthorized):
        await experiments.create_experiment(request)


@pytest.mark.asyncio
async def test_get_experiment_success(mock_user_id, sample_experiment_db_record):
    """Тест успешного получения эксперимента."""
    experiment_id = sample_experiment_db_record['id']

    with patch.object(experiment_queries, 'get_experiment_by_id', new_callable=AsyncMock) as mock_get:
        mock_get.return_value = sample_experiment_db_record

        request = make_mocked_request('GET', f'/experiments/{experiment_id}')
        request.match_info = {'experiment_id': str(experiment_id)}

        response = await experiments.get_experiment(request)

        assert response.status == 200
        data = await response.json()
        assert data['id'] == str(experiment_id)
        assert data['name'] == sample_experiment_db_record['name']


@pytest.mark.asyncio
async def test_get_experiment_not_found():
    """Тест получения несуществующего эксперимента."""
    experiment_id = uuid4()

    with patch.object(experiment_queries, 'get_experiment_by_id', new_callable=AsyncMock) as mock_get:
        mock_get.return_value = None

        request = make_mocked_request('GET', f'/experiments/{experiment_id}')
        request.match_info = {'experiment_id': str(experiment_id)}

        with pytest.raises(web.HTTPNotFound):
            await experiments.get_experiment(request)


@pytest.mark.asyncio
async def test_list_experiments_success(mock_user_id, sample_experiment_db_record):
    """Тест успешного получения списка экспериментов."""
    experiments_list = [sample_experiment_db_record]
    total = 1

    with patch.object(experiment_queries, 'list_experiments', new_callable=AsyncMock) as mock_list:
        mock_list.return_value = (experiments_list, total)

        request = make_mocked_request('GET', '/experiments')
        request.query = {}

        response = await experiments.list_experiments(request)

        assert response.status == 200
        data = await response.json()
        assert len(data['experiments']) == 1
        assert data['total'] == 1
        assert data['page'] == 1
        assert data['page_size'] == 50


@pytest.mark.asyncio
async def test_list_experiments_with_filters(mock_user_id, sample_experiment_db_record):
    """Тест получения списка с фильтрами."""
    project_id = uuid4()
    experiments_list = [sample_experiment_db_record]
    total = 1

    with patch.object(experiment_queries, 'list_experiments', new_callable=AsyncMock) as mock_list:
        mock_list.return_value = (experiments_list, total)

        request = make_mocked_request('GET', '/experiments')
        request.query = {
            'project_id': str(project_id),
            'status': 'created',
            'tags': 'test,aerodynamics',
            'page': '1',
            'page_size': '10'
        }

        response = await experiments.list_experiments(request)

        assert response.status == 200
        mock_list.assert_called_once()


@pytest.mark.asyncio
async def test_list_experiments_page_size_limits(mock_user_id):
    """Тест ограничения page_size."""
    with patch.object(experiment_queries, 'list_experiments', new_callable=AsyncMock) as mock_list:
        mock_list.return_value = ([], 0)

        # Тест превышения максимума
        request = make_mocked_request('GET', '/experiments')
        request.query = {'page_size': '150'}
        await experiments.list_experiments(request)

        # Должен быть ограничен до 100
        call_args = mock_list.call_args[1]
        assert call_args['limit'] == 100

        # Тест минимума
        request.query = {'page_size': '0'}
        await experiments.list_experiments(request)

        call_args = mock_list.call_args[1]
        assert call_args['limit'] == 50


@pytest.mark.asyncio
async def test_update_experiment_success(mock_user_id, sample_experiment_db_record, mock_publish_event):
    """Тест успешного обновления эксперимента."""
    experiment_id = sample_experiment_db_record['id']
    update_data = {
        "name": "Updated Name",
        "status": "running"
    }

    updated_record = sample_experiment_db_record.copy()
    updated_record.update(update_data)
    updated_record['status'] = 'running'

    with patch.object(experiment_queries, 'get_experiment_by_id', new_callable=AsyncMock) as mock_get, \
         patch.object(experiment_queries, 'update_experiment', new_callable=AsyncMock) as mock_update:
        mock_get.return_value = sample_experiment_db_record
        mock_update.return_value = updated_record

        request = make_mocked_request('PUT', f'/experiments/{experiment_id}', json=update_data)
        request.match_info = {'experiment_id': str(experiment_id)}
        request['user_id'] = mock_user_id

        response = await experiments.update_experiment(request)

        assert response.status == 200
        data = await response.json()
        assert data['name'] == update_data['name']
        assert data['status'] == 'running'

        # Проверяем публикацию события при изменении статуса
        mock_publish_event.assert_called_once_with("experiment.updated", {
            "experiment_id": str(experiment_id),
            "old_status": "created",
            "new_status": "running"
        })


@pytest.mark.asyncio
async def test_update_experiment_not_found(mock_user_id):
    """Тест обновления несуществующего эксперимента."""
    experiment_id = uuid4()

    with patch.object(experiment_queries, 'get_experiment_by_id', new_callable=AsyncMock) as mock_get:
        mock_get.return_value = None

        request = make_mocked_request('PUT', f'/experiments/{experiment_id}', json={"name": "New Name"})
        request.match_info = {'experiment_id': str(experiment_id)}
        request['user_id'] = mock_user_id

        with pytest.raises(web.HTTPNotFound):
            await experiments.update_experiment(request)


@pytest.mark.asyncio
async def test_delete_experiment_success(mock_user_id, sample_experiment_db_record, mock_publish_event):
    """Тест успешного удаления эксперимента."""
    experiment_id = sample_experiment_db_record['id']

    with patch.object(experiment_queries, 'get_experiment_by_id', new_callable=AsyncMock) as mock_get, \
         patch.object(experiment_queries, 'delete_experiment', new_callable=AsyncMock) as mock_delete:
        mock_get.return_value = sample_experiment_db_record
        mock_delete.return_value = True

        request = make_mocked_request('DELETE', f'/experiments/{experiment_id}')
        request.match_info = {'experiment_id': str(experiment_id)}

        response = await experiments.delete_experiment(request)

        assert response.status == 200
        data = await response.json()
        assert data['message'] == "Experiment deleted"

        mock_publish_event.assert_called_once_with("experiment.deleted", {
            "experiment_id": str(experiment_id),
            "project_id": str(sample_experiment_db_record['project_id'])
        })


@pytest.mark.asyncio
async def test_delete_experiment_not_found(mock_user_id):
    """Тест удаления несуществующего эксперимента."""
    experiment_id = uuid4()

    with patch.object(experiment_queries, 'get_experiment_by_id', new_callable=AsyncMock) as mock_get:
        mock_get.return_value = None

        request = make_mocked_request('DELETE', f'/experiments/{experiment_id}')
        request.match_info = {'experiment_id': str(experiment_id)}

        with pytest.raises(web.HTTPNotFound):
            await experiments.delete_experiment(request)


@pytest.mark.asyncio
async def test_delete_experiment_failed(mock_user_id, sample_experiment_db_record):
    """Тест неудачного удаления эксперимента."""
    experiment_id = sample_experiment_db_record['id']

    with patch.object(experiment_queries, 'get_experiment_by_id', new_callable=AsyncMock) as mock_get, \
         patch.object(experiment_queries, 'delete_experiment', new_callable=AsyncMock) as mock_delete:
        mock_get.return_value = sample_experiment_db_record
        mock_delete.return_value = False

        request = make_mocked_request('DELETE', f'/experiments/{experiment_id}')
        request.match_info = {'experiment_id': str(experiment_id)}

        with pytest.raises(web.HTTPInternalServerError):
            await experiments.delete_experiment(request)


@pytest.mark.asyncio
async def test_search_experiments_success(mock_user_id, sample_experiment_db_record):
    """Тест успешного поиска экспериментов."""
    experiments_list = [sample_experiment_db_record]
    total = 1

    with patch.object(experiment_queries, 'search_experiments', new_callable=AsyncMock) as mock_search:
        mock_search.return_value = (experiments_list, total)

        request = make_mocked_request('GET', '/experiments/search')
        request.query = {'q': 'test', 'page': '1', 'page_size': '10'}

        response = await experiments.search_experiments(request)

        assert response.status == 200
        data = await response.json()
        assert len(data['experiments']) == 1
        assert data['total'] == 1
        mock_search.assert_called_once()

