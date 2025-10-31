# Experiment Service

Сервис для сохранения, просмотра и наблюдения за экспериментами (аэродинамические, прочностные и другие типы).

## Возможности

- ✅ CRUD операции с экспериментами
- ✅ Управление запусками (runs) экспериментов
- ✅ Фильтрация по проекту, статусу, тегам
- ✅ Поиск по названию и описанию
- ✅ Поддержка различных типов экспериментов
- ✅ Метаданные в формате JSON
- ✅ Интеграция с RabbitMQ для событий

## Запуск

### Локальная разработка

```bash
# Установка зависимостей
pip install -r requirements.txt

# Настройка переменных окружения
export DATABASE_URL="postgresql://postgres:postgres@localhost:5432/experiment_db"
export PORT=8002

# Запуск
python main.py
```

### Docker

```bash
docker build -t experiment-service .
docker run -p 8002:8002 \
  -e DATABASE_URL="postgresql://postgres:postgres@host.docker.internal:5432/experiment_db" \
  experiment-service
```

## API Endpoints

### Experiments

- `GET /experiments` - список экспериментов
- `POST /experiments` - создание эксперимента
- `GET /experiments/{id}` - получение эксперимента
- `PUT /experiments/{id}` - обновление эксперимента
- `DELETE /experiments/{id}` - удаление эксперимента
- `GET /experiments/search?q=...` - поиск экспериментов

### Runs

- `GET /experiments/{id}/runs` - список runs эксперимента
- `POST /experiments/{id}/runs` - создание run
- `GET /runs/{id}` - получение run
- `PUT /runs/{id}` - обновление run
- `PUT /runs/{id}/complete` - завершение run
- `PUT /runs/{id}/fail` - пометить run как failed

## Примеры использования

### Создание эксперимента

```bash
curl -X POST http://localhost:8002/experiments \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer <token>" \
  -d '{
    "project_id": "550e8400-e29b-41d4-a716-446655440000",
    "name": "Аэродинамические испытания крыла",
    "description": "Тестирование профиля NACA 2412",
    "experiment_type": "aerodynamics",
    "tags": ["aerodynamics", "wing", "naca"],
    "metadata": {
      "wind_speed": "30 m/s",
      "angle_of_attack": "0-15 deg"
    }
  }'
```

### Создание run

```bash
curl -X POST http://localhost:8002/experiments/{experiment_id}/runs \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer <token>" \
  -d '{
    "name": "run_001",
    "parameters": {
      "wind_speed": 30,
      "angle_of_attack": 5,
      "reynolds_number": 1000000
    },
    "notes": "Первичное тестирование"
  }'
```

### Поиск экспериментов

```bash
curl "http://localhost:8002/experiments/search?q=аэродинамика&project_id={project_id}"
```

## Структура данных

### Experiment

- `id` - UUID
- `project_id` - UUID проекта
- `name` - название
- `description` - описание
- `experiment_type` - тип (aerodynamics, strength, etc.)
- `status` - статус (created, running, completed, failed, archived)
- `tags` - массив тегов
- `metadata` - дополнительные данные (JSON)
- `created_by` - UUID пользователя
- `created_at`, `updated_at` - временные метки

### Run

- `id` - UUID
- `experiment_id` - UUID эксперимента
- `name` - название запуска
- `parameters` - параметры запуска (JSON)
- `status` - статус (created, running, completed, failed)
- `started_at`, `completed_at` - временные метки
- `duration_seconds` - длительность в секундах
- `notes` - заметки
- `metadata` - дополнительные данные (JSON)

## События

Сервис публикует следующие события в RabbitMQ:

- `experiment.created`
- `experiment.updated`
- `experiment.deleted`
- `run.created`
- `run.started`
- `run.completed`
- `run.failed`

