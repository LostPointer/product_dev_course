# Тесты для experiment-service

Этот каталог содержит тестовый набор для experiment-service.

## Структура тестов

- `conftest.py` - конфигурация pytest и общие фикстуры
- `test_handlers_experiments.py` - тесты для handlers экспериментов
- `test_handlers_runs.py` - тесты для handlers runs
- `test_queries_experiments.py` - тесты для queries экспериментов
- `test_queries_runs.py` - тесты для queries runs
- `test_schemas.py` - тесты валидации схем Pydantic
- `test_integration_api.py` - интеграционные тесты API endpoints

## Запуск тестов

### Все тесты
```bash
pytest
```

### С покрытием кода
```bash
pytest --cov=src --cov-report=html
```

### Только unit тесты
```bash
pytest -m "not integration"
```

### Только интеграционные тесты
```bash
pytest -m integration
```

### Конкретный файл
```bash
pytest tests/test_handlers_experiments.py
```

### Конкретный тест
```bash
pytest tests/test_handlers_experiments.py::test_create_experiment_success
```

### Verbose режим
```bash
pytest -v
```

## Установка зависимостей

Убедитесь, что установлены все зависимости:

```bash
pip install -r requirements.txt
```

## Фикстуры

Основные фикстуры, доступные в тестах:

- `mock_user_id` - тестовый UUID пользователя
- `app` - тестовое приложение aiohttp
- `client` - тестовый HTTP клиент
- `mock_db_pool` - мок пула подключений к БД
- `sample_experiment_data` - пример данных для создания эксперимента
- `sample_experiment_db_record` - пример записи эксперимента из БД
- `sample_run_data` - пример данных для создания run
- `sample_run_db_record` - пример записи run из БД
- `mock_publish_event` - мок для публикации событий

## Примечания

- Все тесты используют моки для базы данных и событий
- Аутентификация автоматически обходится в тестах через переопределение middleware
- Для реальных интеграционных тестов может потребоваться тестовая база данных

