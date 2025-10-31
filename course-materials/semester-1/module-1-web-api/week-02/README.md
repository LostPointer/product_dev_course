# Неделя 2: aiohttp и CRUD API

## Цели недели
- Освоить структуру aiohttp приложения
- Реализовать полноценный CRUD API
- Использовать Pydantic для валидации
- Обрабатывать ошибки правильно

## Теоретическая часть

### 1. Структура aiohttp приложения

**Рекомендуемая структура проекта:**
```
todo-api/
├── main.py              # Entry point
├── config.py            # Конфигурация
├── routes.py            # Определение routes
├── handlers/            # HTTP handlers
│   ├── __init__.py
│   ├── todos.py
│   └── health.py
├── schemas.py           # Pydantic schemas
├── models.py            # Database models (пока in-memory)
├── middleware/          # Custom middleware
│   ├── __init__.py
│   └── error_handler.py
├── tests/               # Тесты
│   ├── __init__.py
│   └── test_todos.py
└── requirements.txt
```

### 2. CRUD Operations

**CRUD** = Create, Read, Update, Delete

| Operation | HTTP Method | Endpoint | Description |
|-----------|-------------|----------|-------------|
| Create | POST | `/todos` | Создать новый TODO |
| Read (list) | GET | `/todos` | Получить список TODO |
| Read (one) | GET | `/todos/{id}` | Получить конкретный TODO |
| Update | PUT | `/todos/{id}` | Обновить TODO полностью |
| Update | PATCH | `/todos/{id}` | Обновить TODO частично |
| Delete | DELETE | `/todos/{id}` | Удалить TODO |

### 3. Pydantic для валидации

**Почему Pydantic:**
- Автоматическая валидация типов
- Понятные сообщения об ошибках
- Генерация JSON Schema
- Отличная интеграция с FastAPI/aiohttp

**Базовый пример:**
```python
from pydantic import BaseModel, Field, validator
from typing import Optional
from datetime import datetime


class TodoBase(BaseModel):
    """Базовая схема TODO."""
    title: str = Field(..., min_length=1, max_length=200)
    description: Optional[str] = Field(None, max_length=1000)
    completed: bool = False
    priority: int = Field(default=1, ge=1, le=5)

    @validator('title')
    def title_must_not_be_empty(cls, v):
        if not v.strip():
            raise ValueError('Title cannot be empty')
        return v.strip()


class TodoCreate(TodoBase):
    """Схема для создания TODO."""
    pass


class TodoUpdate(BaseModel):
    """Схема для обновления TODO (все поля опциональны)."""
    title: Optional[str] = Field(None, min_length=1, max_length=200)
    description: Optional[str] = None
    completed: Optional[bool] = None
    priority: Optional[int] = Field(None, ge=1, le=5)


class TodoResponse(TodoBase):
    """Схема для ответа с TODO."""
    id: int
    created_at: datetime
    updated_at: datetime

    class Config:
        from_attributes = True
```

## Практическая часть

### Задание 1: TODO API с Pydantic

Создайте полноценное TODO API:

**schemas.py:**
```python
from pydantic import BaseModel, Field, validator
from typing import Optional
from datetime import datetime
from enum import Enum


class Priority(str, Enum):
    """Приоритеты задач."""
    LOW = "low"
    MEDIUM = "medium"
    HIGH = "high"
    URGENT = "urgent"


class TodoCreate(BaseModel):
    """Схема для создания TODO."""
    title: str = Field(..., min_length=1, max_length=200, description="Название задачи")
    description: Optional[str] = Field(None, max_length=1000, description="Описание")
    priority: Priority = Field(default=Priority.MEDIUM, description="Приоритет")

    @validator('title')
    def title_must_not_be_empty(cls, v):
        if not v or not v.strip():
            raise ValueError('Title cannot be empty')
        return v.strip()


class TodoUpdate(BaseModel):
    """Схема для обновления TODO."""
    title: Optional[str] = Field(None, min_length=1, max_length=200)
    description: Optional[str] = Field(None, max_length=1000)
    completed: Optional[bool] = None
    priority: Optional[Priority] = None


class TodoResponse(BaseModel):
    """Схема ответа с TODO."""
    id: int
    title: str
    description: Optional[str]
    completed: bool
    priority: Priority
    created_at: datetime
    updated_at: datetime


class TodoListResponse(BaseModel):
    """Схема ответа со списком TODO."""
    todos: list[TodoResponse]
    total: int
```

**handlers/todos.py:**
```python
from aiohttp import web
from typing import Dict, Any
import structlog
from datetime import datetime

from schemas import TodoCreate, TodoUpdate, TodoResponse, TodoListResponse

logger = structlog.get_logger()

# In-memory storage (на следующей неделе перейдем на БД)
todos: Dict[int, Dict[str, Any]] = {}
todo_id_counter = 1


async def create_todo(request: web.Request) -> web.Response:
    """
    Создать новый TODO.

    POST /todos
    Body: TodoCreate schema
    """
    try:
        data = await request.json()
        todo_data = TodoCreate(**data)
    except ValueError as e:
        logger.warning("validation_error", error=str(e))
        return web.json_response(
            {"error": "Validation error", "details": str(e)},
            status=400
        )
    except Exception as e:
        logger.error("invalid_json", error=str(e))
        return web.json_response(
            {"error": "Invalid JSON"},
            status=400
        )

    global todo_id_counter
    todo_id = todo_id_counter
    todo_id_counter += 1

    now = datetime.utcnow()
    todo = {
        "id": todo_id,
        "title": todo_data.title,
        "description": todo_data.description,
        "completed": False,
        "priority": todo_data.priority.value,
        "created_at": now.isoformat(),
        "updated_at": now.isoformat(),
    }

    todos[todo_id] = todo

    logger.info("todo_created", todo_id=todo_id, title=todo_data.title)

    return web.json_response(todo, status=201)


async def list_todos(request: web.Request) -> web.Response:
    """
    Получить список всех TODO.

    GET /todos?completed=true&priority=high
    """
    # Query parameters для фильтрации
    completed_filter = request.query.get('completed')
    priority_filter = request.query.get('priority')

    filtered_todos = list(todos.values())

    # Фильтрация по completed
    if completed_filter is not None:
        completed_bool = completed_filter.lower() == 'true'
        filtered_todos = [
            t for t in filtered_todos
            if t['completed'] == completed_bool
        ]

    # Фильтрация по priority
    if priority_filter:
        filtered_todos = [
            t for t in filtered_todos
            if t['priority'] == priority_filter
        ]

    response = {
        "todos": filtered_todos,
        "total": len(filtered_todos)
    }

    return web.json_response(response)


async def get_todo(request: web.Request) -> web.Response:
    """
    Получить конкретный TODO по ID.

    GET /todos/{id}
    """
    try:
        todo_id = int(request.match_info['id'])
    except ValueError:
        return web.json_response(
            {"error": "Invalid todo ID"},
            status=400
        )

    todo = todos.get(todo_id)
    if not todo:
        return web.json_response(
            {"error": "Todo not found"},
            status=404
        )

    return web.json_response(todo)


async def update_todo(request: web.Request) -> web.Response:
    """
    Обновить TODO.

    PUT /todos/{id}
    Body: TodoUpdate schema
    """
    try:
        todo_id = int(request.match_info['id'])
    except ValueError:
        return web.json_response(
            {"error": "Invalid todo ID"},
            status=400
        )

    todo = todos.get(todo_id)
    if not todo:
        return web.json_response(
            {"error": "Todo not found"},
            status=404
        )

    try:
        data = await request.json()
        update_data = TodoUpdate(**data)
    except ValueError as e:
        return web.json_response(
            {"error": "Validation error", "details": str(e)},
            status=400
        )

    # Обновляем только переданные поля
    update_dict = update_data.dict(exclude_unset=True)
    for key, value in update_dict.items():
        if key == 'priority' and value:
            todo[key] = value.value
        else:
            todo[key] = value

    todo['updated_at'] = datetime.utcnow().isoformat()

    logger.info("todo_updated", todo_id=todo_id)

    return web.json_response(todo)


async def delete_todo(request: web.Request) -> web.Response:
    """
    Удалить TODO.

    DELETE /todos/{id}
    """
    try:
        todo_id = int(request.match_info['id'])
    except ValueError:
        return web.json_response(
            {"error": "Invalid todo ID"},
            status=400
        )

    todo = todos.pop(todo_id, None)
    if not todo:
        return web.json_response(
            {"error": "Todo not found"},
            status=404
        )

    logger.info("todo_deleted", todo_id=todo_id)

    return web.Response(status=204)
```

**routes.py:**
```python
from aiohttp import web
from handlers import todos


def setup_routes(app: web.Application) -> None:
    """Настроить все routes."""
    # TODO endpoints
    app.router.add_post('/todos', todos.create_todo)
    app.router.add_get('/todos', todos.list_todos)
    app.router.add_get('/todos/{id}', todos.get_todo)
    app.router.add_put('/todos/{id}', todos.update_todo)
    app.router.add_delete('/todos/{id}', todos.delete_todo)
```

**main.py:**
```python
from aiohttp import web
import structlog

from routes import setup_routes
from middleware.error_handler import error_middleware


def create_app() -> web.Application:
    """Создать и настроить приложение."""
    # Настройка логирования
    structlog.configure(
        processors=[
            structlog.processors.TimeStamper(fmt="iso"),
            structlog.processors.add_log_level,
            structlog.processors.JSONRenderer()
        ],
        logger_factory=structlog.PrintLoggerFactory(),
    )

    app = web.Application(middlewares=[error_middleware])

    # Setup routes
    setup_routes(app)

    return app


if __name__ == '__main__':
    app = create_app()
    web.run_app(app, host='0.0.0.0', port=8000)
```

### Задание 2: Error Handling Middleware

**middleware/error_handler.py:**
```python
from aiohttp import web
import structlog
from typing import Callable

logger = structlog.get_logger()


@web.middleware
async def error_middleware(
    request: web.Request,
    handler: Callable
) -> web.Response:
    """
    Middleware для обработки ошибок.
    """
    try:
        return await handler(request)
    except web.HTTPException:
        # Пропускаем HTTP исключения
        raise
    except ValueError as e:
        logger.warning(
            "validation_error",
            path=request.path,
            error=str(e)
        )
        return web.json_response(
            {"error": "Validation error", "details": str(e)},
            status=400
        )
    except Exception as e:
        logger.error(
            "unexpected_error",
            path=request.path,
            method=request.method,
            error=str(e),
            exc_info=True
        )
        return web.json_response(
            {"error": "Internal server error"},
            status=500
        )
```

### Тестирование API

```bash
# Создать TODO
curl -X POST http://localhost:8000/todos \
  -H "Content-Type: application/json" \
  -d '{
    "title": "Изучить aiohttp",
    "description": "Пройти неделю 2",
    "priority": "high"
  }'

# Получить список
curl http://localhost:8000/todos

# Получить конкретный TODO
curl http://localhost:8000/todos/1

# Обновить TODO
curl -X PUT http://localhost:8000/todos/1 \
  -H "Content-Type: application/json" \
  -d '{
    "completed": true
  }'

# Удалить TODO
curl -X DELETE http://localhost:8000/todos/1

# Фильтрация
curl "http://localhost:8000/todos?completed=false&priority=high"
```

## Дополнительные материалы

### Полезные ссылки
- [Pydantic Documentation](https://docs.pydantic.dev/)
- [aiohttp Middleware](https://docs.aiohttp.org/en/stable/web_advanced.html#middlewares)
- [REST API Best Practices](https://stackoverflow.blog/2020/03/02/best-practices-for-rest-api-design/)

### Примеры кода
- [aiohttp Examples](https://github.com/aio-libs/aiohttp/tree/master/examples)
- [Real World aiohttp](https://github.com/gothinkster/realworld)

## Вопросы для самопроверки

1. В чем разница между PUT и PATCH?
2. Когда использовать 400, а когда 422 статус?
3. Что такое idempotent операция?
4. Зачем нужны Pydantic схемы?
5. Как правильно обрабатывать ошибки валидации?

## Следующая неделя

На [Неделе 3](../week-03/README.md) изучим PostgreSQL и asyncpg для работы с базой данных! 🚀

---

**Удачи с CRUD операциями! 📝**

