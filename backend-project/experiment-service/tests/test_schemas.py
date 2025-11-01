"""Тесты для schemas.py - валидация данных."""
import pytest
from uuid import uuid4
from datetime import datetime
from pydantic import ValidationError

from src.schemas import (
    ExperimentCreate,
    ExperimentUpdate,
    ExperimentResponse,
    RunCreate,
    RunUpdate,
    RunResponse,
    ExperimentListResponse,
    RunListResponse
)


class TestExperimentSchemas:
    """Тесты для схем экспериментов."""

    def test_experiment_create_valid(self):
        """Тест валидного создания эксперимента."""
        data = {
            "project_id": uuid4(),
            "name": "Test Experiment",
            "description": "Test description",
            "experiment_type": "aerodynamics",
            "tags": ["test", "aerodynamics"],
            "metadata": {"key": "value"}
        }

        schema = ExperimentCreate(**data)
        assert schema.name == "Test Experiment"
        assert schema.project_id == data["project_id"]
        assert schema.tags == ["test", "aerodynamics"]

    def test_experiment_create_minimal(self):
        """Тест создания с минимальными полями."""
        data = {
            "project_id": uuid4(),
            "name": "Minimal Experiment"
        }

        schema = ExperimentCreate(**data)
        assert schema.name == "Minimal Experiment"
        assert schema.description is None
        assert schema.tags == []
        assert schema.metadata == {}

    def test_experiment_create_name_too_long(self):
        """Тест создания с именем превышающим максимум."""
        data = {
            "project_id": uuid4(),
            "name": "x" * 256  # Превышает max_length=255
        }

        with pytest.raises(ValidationError) as exc_info:
            ExperimentCreate(**data)

        assert "name" in str(exc_info.value)

    def test_experiment_create_name_empty(self):
        """Тест создания с пустым именем."""
        data = {
            "project_id": uuid4(),
            "name": ""
        }

        with pytest.raises(ValidationError):
            ExperimentCreate(**data)

    def test_experiment_update_valid(self):
        """Тест валидного обновления эксперимента."""
        data = {
            "name": "Updated Name",
            "status": "running"
        }

        schema = ExperimentUpdate(**data)
        assert schema.name == "Updated Name"
        assert schema.status == "running"

    def test_experiment_update_partial(self):
        """Тест частичного обновления."""
        data = {
            "name": "New Name"
        }

        schema = ExperimentUpdate(**data)
        assert schema.name == "New Name"
        assert schema.status is None

    def test_experiment_update_invalid_status(self):
        """Тест обновления с невалидным статусом."""
        data = {
            "status": "invalid_status"
        }

        with pytest.raises(ValidationError) as exc_info:
            ExperimentUpdate(**data)

        assert "status" in str(exc_info.value)

    def test_experiment_update_valid_statuses(self):
        """Тест всех валидных статусов."""
        valid_statuses = ['created', 'running', 'completed', 'failed', 'archived']

        for status in valid_statuses:
            data = {"status": status}
            schema = ExperimentUpdate(**data)
            assert schema.status == status

    def test_experiment_response(self):
        """Тест схемы ответа эксперимента."""
        data = {
            "id": uuid4(),
            "project_id": uuid4(),
            "created_by": uuid4(),
            "name": "Test Experiment",
            "description": "Test",
            "experiment_type": "aerodynamics",
            "status": "created",
            "tags": ["test"],
            "metadata": {"key": "value"},
            "created_at": datetime.now(),
            "updated_at": datetime.now()
        }

        schema = ExperimentResponse(**data)
        assert schema.id == data["id"]
        assert schema.status == "created"


class TestRunSchemas:
    """Тесты для схем runs."""

    def test_run_create_valid(self):
        """Тест валидного создания run."""
        data = {
            "name": "Test Run",
            "parameters": {"velocity": 100, "altitude": 5000},
            "notes": "Test notes",
            "metadata": {"key": "value"}
        }

        schema = RunCreate(**data)
        assert schema.name == "Test Run"
        assert schema.parameters == {"velocity": 100, "altitude": 5000}
        assert schema.notes == "Test notes"

    def test_run_create_minimal(self):
        """Тест создания с минимальными полями."""
        data = {
            "name": "Minimal Run",
            "parameters": {}
        }

        schema = RunCreate(**data)
        assert schema.name == "Minimal Run"
        assert schema.parameters == {}
        assert schema.notes is None
        assert schema.metadata == {}

    def test_run_create_name_empty(self):
        """Тест создания с пустым именем."""
        data = {
            "name": "",
            "parameters": {}
        }

        with pytest.raises(ValidationError):
            RunCreate(**data)

    def test_run_create_parameters_required(self):
        """Тест что parameters обязательны."""
        data = {
            "name": "Test Run"
        }

        with pytest.raises(ValidationError):
            RunCreate(**data)

    def test_run_update_valid(self):
        """Тест валидного обновления run."""
        data = {
            "name": "Updated Run",
            "status": "running"
        }

        schema = RunUpdate(**data)
        assert schema.name == "Updated Run"
        assert schema.status == "running"

    def test_run_update_invalid_status(self):
        """Тест обновления с невалидным статусом."""
        data = {
            "status": "invalid_status"
        }

        with pytest.raises(ValidationError):
            RunUpdate(**data)

    def test_run_update_valid_statuses(self):
        """Тест всех валидных статусов."""
        valid_statuses = ['created', 'running', 'completed', 'failed']

        for status in valid_statuses:
            data = {"status": status}
            schema = RunUpdate(**data)
            assert schema.status == status

    def test_run_response(self):
        """Тест схемы ответа run."""
        data = {
            "id": uuid4(),
            "experiment_id": uuid4(),
            "name": "Test Run",
            "parameters": {"velocity": 100},
            "status": "created",
            "started_at": None,
            "completed_at": None,
            "duration_seconds": None,
            "notes": "Test notes",
            "metadata": {},
            "created_at": datetime.now(),
            "updated_at": datetime.now()
        }

        schema = RunResponse(**data)
        assert schema.id == data["id"]
        assert schema.status == "created"


class TestListSchemas:
    """Тесты для схем списков."""

    def test_experiment_list_response(self):
        """Тест схемы списка экспериментов."""
        experiment_data = {
            "id": uuid4(),
            "project_id": uuid4(),
            "created_by": uuid4(),
            "name": "Test",
            "description": None,
            "experiment_type": None,
            "status": "created",
            "tags": [],
            "metadata": {},
            "created_at": datetime.now(),
            "updated_at": datetime.now()
        }

        experiment = ExperimentResponse(**experiment_data)

        data = {
            "experiments": [experiment],
            "total": 1,
            "page": 1,
            "page_size": 50
        }

        schema = ExperimentListResponse(**data)
        assert len(schema.experiments) == 1
        assert schema.total == 1
        assert schema.page == 1

    def test_run_list_response(self):
        """Тест схемы списка runs."""
        run_data = {
            "id": uuid4(),
            "experiment_id": uuid4(),
            "name": "Test Run",
            "parameters": {},
            "status": "created",
            "started_at": None,
            "completed_at": None,
            "duration_seconds": None,
            "notes": None,
            "metadata": {},
            "created_at": datetime.now(),
            "updated_at": datetime.now()
        }

        run = RunResponse(**run_data)

        data = {
            "runs": [run],
            "total": 1,
            "page": 1,
            "page_size": 50
        }

        schema = RunListResponse(**data)
        assert len(schema.runs) == 1
        assert schema.total == 1
        assert schema.page == 1

