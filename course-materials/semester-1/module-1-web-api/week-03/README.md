# Неделя 3: PostgreSQL и asyncpg

## Цели недели
- Подключить PostgreSQL к приложению
- Освоить asyncpg для работы с PostgreSQL
- Научиться писать SQL запросы
- Работать с миграциями (Alembic)

## Теоретическая часть

### 1. Реляционные базы данных

**PostgreSQL** - мощная open-source реляционная СУБД.

**Основные концепции:**
- **Таблицы** - хранят данные в строках и столбцах
- **Схемы** - логическая группировка таблиц
- **Индексы** - ускоряют поиск
- **Constraints** - ограничения целостности данных
- **Транзакции** - атомарные операции

**SQL основы:**
```sql
-- Создание таблицы
CREATE TABLE users (
    id SERIAL PRIMARY KEY,
    username VARCHAR(50) UNIQUE NOT NULL,
    email VARCHAR(100) UNIQUE NOT NULL,
    password_hash VARCHAR(255) NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- INSERT
INSERT INTO users (username, email, password_hash)
VALUES ('john_doe', 'john@example.com', 'hashed_password');

-- SELECT
SELECT * FROM users WHERE username = 'john_doe';

-- UPDATE
UPDATE users SET email = 'newemail@example.com' WHERE id = 1;

-- DELETE
DELETE FROM users WHERE id = 1;
```

### 2. asyncpg - асинхронный драйвер PostgreSQL

**asyncpg** - быстрый и эффективный асинхронный драйвер для PostgreSQL на Python.

**Преимущества:**
- ✅ Очень быстрый (в 2-3 раза быстрее psycopg)
- ✅ Нативная поддержка async/await
- ✅ Поддержка PostgreSQL специфичных фич
- ✅ Защита от SQL injection через параметризованные запросы
- ✅ Connection pooling из коробки
- ✅ Поддержка prepared statements

**Архитектура с asyncpg:**
```
┌─────────────────────────────────┐
│    Application (Python code)    │
└─────────────┬───────────────────┘
              │
┌─────────────▼───────────────────┐
│         asyncpg Pool            │
│    (Connection Management)      │
└─────────────┬───────────────────┘
              │
┌─────────────▼───────────────────┐
│         SQL Queries             │
│   (Direct SQL, no ORM)          │
└─────────────┬───────────────────┘
              │
┌─────────────▼───────────────────┐
│         PostgreSQL              │
└─────────────────────────────────┘
```

**Почему asyncpg вместо ORM?**
- 🚀 Больше контроля над запросами
- 🚀 Лучшая производительность
- 🚀 Понимание SQL (важно для бэкенд разработчика)
- 🚀 Меньше абстракций = меньше "магии"

### 3. Работа с asyncpg

Основные концепции:

```python
import asyncpg

# Создание connection pool
pool = await asyncpg.create_pool(
    "postgresql://user:password@localhost:5432/dbname",
    min_size=10,
    max_size=20
)

# Выполнение запроса
async with pool.acquire() as conn:
    rows = await conn.fetch("SELECT * FROM users WHERE id = $1", user_id)

# Использование транзакций
async with pool.acquire() as conn:
    async with conn.transaction():
        await conn.execute("INSERT INTO users ...")
        await conn.execute("INSERT INTO todos ...")
```

## Практическая часть

### Задание 1: Настройка PostgreSQL

**1. Установка PostgreSQL:**
```bash
# Docker (рекомендуется)
docker run --name todo-postgres \
  -e POSTGRES_USER=todouser \
  -e POSTGRES_PASSWORD=todopass \
  -e POSTGRES_DB=tododb \
  -p 5432:5432 \
  -d postgres:15-alpine

# Проверка подключения
docker exec -it todo-postgres psql -U todouser -d tododb
```

**2. Установка зависимостей:**
```bash
pip install asyncpg alembic
```

**3. Структура проекта:**
```
todo-api/
├── main.py
├── config.py              # NEW
├── database.py            # NEW - connection pool
├── queries/               # NEW - SQL запросы
│   └── todos.py
├── handlers/
│   └── todos.py
├── schemas.py
├── alembic.ini            # NEW
├── alembic/               # NEW
│   └── versions/
└── requirements.txt
```

### Задание 2: Подключение к БД и SQL схемы

**database.py:**
```python
import asyncpg
from typing import Optional
from config import settings

# Глобальный connection pool
_db_pool: Optional[asyncpg.Pool] = None


async def init_db():
    """Инициализация connection pool."""
    global _db_pool

    _db_pool = await asyncpg.create_pool(
        settings.database_url,
        min_size=5,
        max_size=20,
        command_timeout=60
    )
    print("Database pool created")


async def close_db():
    """Закрыть connection pool."""
    global _db_pool
    if _db_pool:
        await _db_pool.close()
        print("Database pool closed")


def get_db_pool() -> asyncpg.Pool:
    """Получить connection pool."""
    if _db_pool is None:
        raise RuntimeError("Database pool not initialized. Call init_db() first.")
    return _db_pool


async def create_tables():
    """Создать таблицы (для первой инициализации)."""
    pool = get_db_pool()

    async with pool.acquire() as conn:
        # Создание типа enum для приоритета
        await conn.execute("""
            DO $$ BEGIN
                CREATE TYPE priority_enum AS ENUM ('low', 'medium', 'high', 'urgent');
            EXCEPTION
                WHEN duplicate_object THEN null;
            END $$;
        """)

        # Создание таблицы todos
        await conn.execute("""
            CREATE TABLE IF NOT EXISTS todos (
                id SERIAL PRIMARY KEY,
                title VARCHAR(200) NOT NULL,
                description TEXT,
                completed BOOLEAN DEFAULT FALSE NOT NULL,
                priority priority_enum DEFAULT 'medium' NOT NULL,
                created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP NOT NULL,
                updated_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP NOT NULL
            );
        """)

        # Создание индексов
        await conn.execute("CREATE INDEX IF NOT EXISTS idx_todos_title ON todos(title);")
        await conn.execute("CREATE INDEX IF NOT EXISTS idx_todos_completed ON todos(completed);")
        await conn.execute("CREATE INDEX IF NOT EXISTS idx_todos_priority ON todos(priority);")
```

**queries/todos.py** - SQL запросы:
```python
"""SQL запросы для работы с todos."""
from typing import Optional, List, Dict, Any
from datetime import datetime
import asyncpg


class Priority:
    """Приоритеты задач."""
    LOW = "low"
    MEDIUM = "medium"
    HIGH = "high"
    URGENT = "urgent"


async def create_todo(
    conn: asyncpg.Connection,
    title: str,
    description: Optional[str] = None,
    priority: str = Priority.MEDIUM
) -> Dict[str, Any]:
    """Создать новый TODO."""
    row = await conn.fetchrow("""
        INSERT INTO todos (title, description, priority)
        VALUES ($1, $2, $3::priority_enum)
        RETURNING id, title, description, completed, priority, created_at, updated_at
    """, title, description, priority)

    return dict(row)


async def get_todo_by_id(
    conn: asyncpg.Connection,
    todo_id: int
) -> Optional[Dict[str, Any]]:
    """Получить TODO по ID."""
    row = await conn.fetchrow("""
        SELECT id, title, description, completed, priority, created_at, updated_at
        FROM todos
        WHERE id = $1
    """, todo_id)

    return dict(row) if row else None


async def list_todos(
    conn: asyncpg.Connection,
    completed: Optional[bool] = None,
    priority: Optional[str] = None,
    limit: int = 100,
    offset: int = 0
) -> List[Dict[str, Any]]:
    """Получить список TODO с фильтрацией."""
    conditions = []
    params = []
    param_count = 0

    if completed is not None:
        param_count += 1
        conditions.append(f"completed = ${param_count}")
        params.append(completed)

    if priority:
        param_count += 1
        conditions.append(f"priority = ${param_count}::priority_enum")
        params.append(priority)

    where_clause = " AND ".join(conditions) if conditions else "1=1"

    param_count += 1
    params.append(limit)
    param_count += 1
    params.append(offset)

    query = f"""
        SELECT id, title, description, completed, priority, created_at, updated_at
        FROM todos
        WHERE {where_clause}
        ORDER BY created_at DESC
        LIMIT ${param_count - 1} OFFSET ${param_count}
    """

    rows = await conn.fetch(query, *params)
    return [dict(row) for row in rows]


async def update_todo(
    conn: asyncpg.Connection,
    todo_id: int,
    title: Optional[str] = None,
    description: Optional[str] = None,
    completed: Optional[bool] = None,
    priority: Optional[str] = None
) -> Optional[Dict[str, Any]]:
    """Обновить TODO."""
    updates = []
    params = []
    param_count = 0

    if title is not None:
        param_count += 1
        updates.append(f"title = ${param_count}")
        params.append(title)

    if description is not None:
        param_count += 1
        updates.append(f"description = ${param_count}")
        params.append(description)

    if completed is not None:
        param_count += 1
        updates.append(f"completed = ${param_count}")
        params.append(completed)

    if priority is not None:
        param_count += 1
        updates.append(f"priority = ${param_count}::priority_enum")
        params.append(priority)

    if not updates:
        return await get_todo_by_id(conn, todo_id)

    # Обновляем updated_at
    param_count += 1
    updates.append(f"updated_at = CURRENT_TIMESTAMP")

    param_count += 1
    params.append(todo_id)

    query = f"""
        UPDATE todos
        SET {', '.join(updates)}
        WHERE id = ${param_count}
        RETURNING id, title, description, completed, priority, created_at, updated_at
    """

    row = await conn.fetchrow(query, *params)
    return dict(row) if row else None


async def delete_todo(
    conn: asyncpg.Connection,
    todo_id: int
) -> bool:
    """Удалить TODO."""
    result = await conn.execute(
        "DELETE FROM todos WHERE id = $1",
        todo_id
    )
    return result == "DELETE 1"
```

**config.py:**
```python
from pydantic_settings import BaseSettings


class Settings(BaseSettings):
    """Настройки приложения."""

    # Database (asyncpg использует postgresql:// без +asyncpg)
    database_url: str = "postgresql://todouser:todopass@localhost:5432/tododb"

    # Application
    debug: bool = True
    host: str = "0.0.0.0"
    port: int = 8000

    class Config:
        env_file = ".env"
        case_sensitive = False


settings = Settings()
```

### Задание 3: CRUD операции с asyncpg

**handlers/todos.py (обновленный):**
```python
from aiohttp import web
import structlog
import asyncpg

from database import get_db_pool
from queries.todos import (
    create_todo,
    get_todo_by_id,
    list_todos,
    update_todo,
    delete_todo,
    Priority
)
from schemas import TodoCreate, TodoUpdate

logger = structlog.get_logger()


async def create_todo_handler(request: web.Request) -> web.Response:
    """Создать новый TODO."""
    try:
        data = await request.json()
        todo_data = TodoCreate(**data)
    except ValueError as e:
        return web.json_response(
            {"error": "Validation error", "details": str(e)},
            status=400
        )

    pool = get_db_pool()

    async with pool.acquire() as conn:
        todo = await create_todo(
            conn,
            title=todo_data.title,
            description=todo_data.description,
            priority=todo_data.priority.value if hasattr(todo_data.priority, 'value') else todo_data.priority
        )

        logger.info("todo_created", todo_id=todo['id'])

        return web.json_response(todo, status=201)


async def list_todos_handler(request: web.Request) -> web.Response:
    """Получить список TODO с фильтрацией."""
    pool = get_db_pool()

    # Query parameters
    completed_filter = request.query.get('completed')
    priority_filter = request.query.get('priority')
    page = int(request.query.get('page', 1))
    per_page = int(request.query.get('per_page', 20))

    completed = None
    if completed_filter is not None:
        completed = completed_filter.lower() == 'true'

    if priority_filter and priority_filter not in [Priority.LOW, Priority.MEDIUM, Priority.HIGH, Priority.URGENT]:
        return web.json_response(
            {"error": f"Invalid priority: {priority_filter}"},
            status=400
        )

    async with pool.acquire() as conn:
        todos = await list_todos(
            conn,
            completed=completed,
            priority=priority_filter,
            limit=per_page,
            offset=(page - 1) * per_page
        )

        return web.json_response({
            "todos": todos,
            "total": len(todos),
            "page": page,
            "per_page": per_page
        })


async def get_todo_handler(request: web.Request) -> web.Response:
    """Получить TODO по ID."""
    try:
        todo_id = int(request.match_info['id'])
    except ValueError:
        return web.json_response({"error": "Invalid ID"}, status=400)

    pool = get_db_pool()

    async with pool.acquire() as conn:
        todo = await get_todo_by_id(conn, todo_id)

        if not todo:
            return web.json_response(
                {"error": "Todo not found"},
                status=404
            )

        return web.json_response(todo)


async def update_todo_handler(request: web.Request) -> web.Response:
    """Обновить TODO."""
    try:
        todo_id = int(request.match_info['id'])
    except ValueError:
        return web.json_response({"error": "Invalid ID"}, status=400)

    try:
        data = await request.json()
        update_data = TodoUpdate(**data)
    except ValueError as e:
        return web.json_response(
            {"error": "Validation error", "details": str(e)},
            status=400
        )

    pool = get_db_pool()

    async with pool.acquire() as conn:
        # Проверяем существование
        existing_todo = await get_todo_by_id(conn, todo_id)
        if not existing_todo:
            return web.json_response(
                {"error": "Todo not found"},
                status=404
            )

        # Обновляем только переданные поля
        update_dict = update_data.dict(exclude_unset=True)

        # Преобразуем priority если есть
        if 'priority' in update_dict and hasattr(update_dict['priority'], 'value'):
            update_dict['priority'] = update_dict['priority'].value

        todo = await update_todo(
            conn,
            todo_id,
            title=update_dict.get('title'),
            description=update_dict.get('description'),
            completed=update_dict.get('completed'),
            priority=update_dict.get('priority')
        )

        logger.info("todo_updated", todo_id=todo_id)

        return web.json_response(todo)


async def delete_todo_handler(request: web.Request) -> web.Response:
    """Удалить TODO."""
    try:
        todo_id = int(request.match_info['id'])
    except ValueError:
        return web.json_response({"error": "Invalid ID"}, status=400)

    pool = get_db_pool()

    async with pool.acquire() as conn:
        deleted = await delete_todo(conn, todo_id)

        if not deleted:
            return web.json_response(
                {"error": "Todo not found"},
                status=404
            )

        logger.info("todo_deleted", todo_id=todo_id)

        return web.Response(status=204)
```

**main.py (обновленный):**
```python
from aiohttp import web
import structlog

from routes import setup_routes
from database import init_db, close_db, create_tables
from middleware.error_handler import error_middleware
from config import settings


async def on_startup(app: web.Application):
    """Инициализация при старте."""
    logger = structlog.get_logger()
    logger.info("starting_application")

    # Инициализация БД pool
    await init_db()

    # Создание таблиц (если нужно, обычно через миграции)
    # await create_tables()


async def on_cleanup(app: web.Application):
    """Очистка при остановке."""
    logger = structlog.get_logger()
    logger.info("shutting_down")

    # Закрываем connection pool
    await close_db()


def create_app() -> web.Application:
    """Создать приложение."""
    structlog.configure(
        processors=[
            structlog.processors.TimeStamper(fmt="iso"),
            structlog.processors.add_log_level,
            structlog.processors.JSONRenderer()
        ],
        logger_factory=structlog.PrintLoggerFactory(),
    )

    app = web.Application(middlewares=[error_middleware])

    # Routes
    setup_routes(app)

    # Lifecycle
    app.on_startup.append(on_startup)
    app.on_cleanup.append(on_cleanup)

    return app


if __name__ == '__main__':
    app = create_app()
    web.run_app(app, host=settings.host, port=settings.port)
```

### Задание 4: Миграции с Alembic

**1. Инициализация Alembic:**
```bash
alembic init alembic
```

**2. Настройка alembic/env.py:**
```python
from alembic import context
from config import settings

# Alembic Config
config = context.config
# asyncpg использует postgresql:// без +asyncpg
config.set_main_option('sqlalchemy.url', settings.database_url)

# Для asyncpg можно использовать async обёртку или синхронный драйвер для миграций
# В production обычно используют синхронный psycopg2 для миграций
```

**Примечание:** Alembic по умолчанию работает синхронно. Для asyncpg можно:
- Использовать psycopg2 для миграций (рекомендуется)
- Или писать миграции вручную в SQL

**3. Создание миграции вручную (SQL):**

```python
# alembic/versions/001_create_todos.py
"""create todos table

Revision ID: 001
"""
from alembic import op
import sqlalchemy as sa

revision = '001'
down_revision = None

def upgrade():
    # Создание enum типа
    op.execute("""
        DO $$ BEGIN
            CREATE TYPE priority_enum AS ENUM ('low', 'medium', 'high', 'urgent');
        EXCEPTION
            WHEN duplicate_object THEN null;
        END $$;
    """)

    # Создание таблицы
    op.execute("""
        CREATE TABLE todos (
            id SERIAL PRIMARY KEY,
            title VARCHAR(200) NOT NULL,
            description TEXT,
            completed BOOLEAN DEFAULT FALSE NOT NULL,
            priority priority_enum DEFAULT 'medium' NOT NULL,
            created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP NOT NULL,
            updated_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP NOT NULL
        );
    """)

    # Создание индексов
    op.execute("CREATE INDEX idx_todos_title ON todos(title);")
    op.execute("CREATE INDEX idx_todos_completed ON todos(completed);")
    op.execute("CREATE INDEX idx_todos_priority ON todos(priority);")

def downgrade():
    op.execute("DROP TABLE IF EXISTS todos;")
    op.execute("DROP TYPE IF EXISTS priority_enum;")
```

**4. Применение миграций:**
```bash
# Применение
alembic upgrade head

# Откат
alembic downgrade -1
```

## Дополнительные материалы

### Полезные ссылки
- [asyncpg Documentation](https://magicstack.github.io/asyncpg/)
- [asyncpg GitHub](https://github.com/MagicStack/asyncpg)
- [Alembic Tutorial](https://alembic.sqlalchemy.org/en/latest/tutorial.html)
- [PostgreSQL Tutorial](https://www.postgresqltutorial.com/)
- [PostgreSQL Official Docs](https://www.postgresql.org/docs/)

### Примеры
- [asyncpg Examples](https://github.com/MagicStack/asyncpg/tree/master/examples)
- [SQL Best Practices](https://www.postgresql.org/docs/current/performance-tips.html)

## Вопросы для самопроверки

1. В чем разница между ORM и прямыми SQL запросами?
2. Что такое connection pool и зачем он нужен?
3. Как работают транзакции в asyncpg?
4. Что такое N+1 problem и как его избежать в SQL?
5. Почему asyncpg быстрее других драйверов PostgreSQL?
6. Как защититься от SQL injection при использовании asyncpg?

## Следующая неделя

На [Неделе 4](../week-04/README.md) изучим JWT аутентификацию и защиту endpoints! 🚀

---

**Удачи с базой данных! 🗄️**

## Важные замечания по asyncpg

### Защита от SQL injection

**✅ ПРАВИЛЬНО - параметризованные запросы:**
```python
await conn.fetch("SELECT * FROM users WHERE id = $1", user_id)
```

**❌ НЕПРАВИЛЬНО - строковая конкатенация:**
```python
await conn.fetch(f"SELECT * FROM users WHERE id = {user_id}")  # ОПАСНО!
```

### Connection Pool

- Используйте один pool на всё приложение
- Не создавайте новое подключение для каждого запроса
- Настройте min_size и max_size в зависимости от нагрузки

### Prepared Statements

asyncpg автоматически использует prepared statements для повторяющихся запросов, что ускоряет выполнение.

### Транзакции

Всегда используйте транзакции для операций, которые должны быть атомарными:
```python
async with pool.acquire() as conn:
    async with conn.transaction():
        await conn.execute("INSERT INTO ...")
        await conn.execute("UPDATE ...")
```

---

**Удачи с базами данных! 🗄️**

