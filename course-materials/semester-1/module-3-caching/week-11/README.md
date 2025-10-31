# Неделя 11: Оптимизация БД, N+1 problem, индексы и профилирование

## Цели недели
- Понять проблему N+1 запросов и как её избежать
- Освоить создание и использование индексов в PostgreSQL
- Научиться профилировать SQL запросы с EXPLAIN
- Оптимизировать медленные запросы
- Применять паттерны эффективной работы с БД
- Использовать инструменты для мониторинга производительности

## Теория

### Почему оптимизация БД важна?

**Проблема:**
```
Медленный запрос → Медленный response → Плохой UX → Потеря пользователей
```

**Статистика:**
- 1 секунда задержки = -7% конверсии
- 47% пользователей ожидают загрузку < 2 секунд
- 40% покидают сайт, если загрузка > 3 секунд

**Где обычно узкое горло:**
- 🔴 База данных - 70% проблем
- 🟠 Сеть/API - 20%
- 🟡 Код приложения - 10%

### N+1 Problem - главный враг производительности

**N+1 Problem** - это когда делается 1 запрос для получения N элементов, а затем N дополнительных запросов для каждого элемента.

#### Плохой пример

```python
# ❌ ПЛОХО - N+1 проблема
import asyncpg
from database import get_db_pool

async def get_users_with_posts():
    pool = get_db_pool()
    async with pool.acquire() as conn:
        # 1 запрос - получаем пользователей
        users = await conn.fetch("SELECT * FROM users LIMIT 10")

        result = []
        for user in users:  # 10 итераций
            # N запросов - для каждого пользователя!
            posts = await conn.fetch(
                "SELECT * FROM posts WHERE user_id = $1",
                user['id']
            )

            result.append({
                "user": dict(user),
                "posts": [dict(p) for p in posts]
            })

        return result
    # Итого: 1 + 10 = 11 запросов! 😱
```

**Как это выглядит в логах:**
```sql
SELECT * FROM users LIMIT 10;                    -- 1 запрос
SELECT * FROM posts WHERE user_id = 1;           -- запрос 1
SELECT * FROM posts WHERE user_id = 2;           -- запрос 2
SELECT * FROM posts WHERE user_id = 3;           -- запрос 3
...
SELECT * FROM posts WHERE user_id = 10;          -- запрос 10
-- Итого 11 запросов вместо 2!
```

#### Хороший пример - Batch Loading

```python
# ✅ ХОРОШО - Batch Loading
import asyncpg
from database import get_db_pool

async def get_users_with_posts():
    pool = get_db_pool()
    async with pool.acquire() as conn:
        # 1 запрос - получаем пользователей
        users = await conn.fetch("SELECT * FROM users LIMIT 10")

        if not users:
            return []

        # 1 запрос - получаем все посты для этих пользователей
        user_ids = [u['id'] for u in users]
        posts = await conn.fetch(
            "SELECT * FROM posts WHERE user_id = ANY($1::int[])",
            user_ids
        )

        # Группируем посты по пользователям
        posts_by_user = {}
        for post in posts:
            user_id = post['user_id']
            if user_id not in posts_by_user:
                posts_by_user[user_id] = []
            posts_by_user[user_id].append(dict(post))

        # Формируем результат
        return [
            {
                "user": dict(user),
                "posts": posts_by_user.get(user['id'], [])
            }
            for user in users
        ]
    # Итого: 2 запроса! ✅
```

**SQL запросы:**
```sql
SELECT * FROM users LIMIT 10;                              -- 1 запрос
SELECT * FROM posts WHERE user_id IN (1,2,3,4,5,6,7,8,9,10); -- 1 запрос
-- Итого 2 запроса!
```

### Стратегии загрузки в asyncpg (избегание N+1)

#### 1. Batch Loading (SELECT IN)

```python
# Загружает связанные объекты отдельным SELECT IN запросом
async def get_users_with_posts():
    pool = get_db_pool()
    async with pool.acquire() as conn:
        # 1 запрос - пользователи
        users = await conn.fetch("SELECT * FROM users LIMIT 10")

        if users:
            user_ids = [u['id'] for u in users]
            # 1 запрос - все посты
            posts = await conn.fetch(
                "SELECT * FROM posts WHERE user_id = ANY($1::int[])",
                user_ids
            )
            # Группируем
            ...

        return result

# 2 запроса:
# SELECT * FROM users
# SELECT * FROM posts WHERE user_id = ANY(ARRAY[...])
```

#### 2. JOIN запрос

```python
# Загружает связанные объекты через JOIN
async def get_users_with_posts_join():
    pool = get_db_pool()
    async with pool.acquire() as conn:
        # 1 запрос с JOIN
        rows = await conn.fetch("""
            SELECT
                u.id as user_id, u.username, u.email,
                p.id as post_id, p.title, p.content
            FROM users u
            LEFT JOIN posts p ON p.user_id = u.id
            LIMIT 10
        """)

        # Группируем результаты
        users_dict = {}
        for row in rows:
            user_id = row['user_id']
            if user_id not in users_dict:
                users_dict[user_id] = {
                    'id': user_id,
                    'username': row['username'],
                    'email': row['email'],
                    'posts': []
                }
            if row['post_id']:
                users_dict[user_id]['posts'].append({
                    'id': row['post_id'],
                    'title': row['title'],
                    'content': row['content']
                })

        return list(users_dict.values())

# 1 запрос:
# SELECT ... FROM users LEFT JOIN posts ON ...
```

#### Когда что использовать?

| Метод | Когда использовать | Запросов |
|-------|-------------------|----------|
| **selectinload** | Один-ко-многим (1:N) | 2 |
| **joinedload** | Один-к-одному (1:1) или малое кол-во связей | 1 |
| **subqueryload** | Сложные условия, ограничения | 2 |
| **lazy="select"** | Редко используемые связи | N+1 |

### Вложенные загрузки

```python
# Загрузка с несколькими уровнями
result = await session.execute(
    select(User)
    .options(
        selectinload(User.posts).selectinload(Post.comments)
    )
)
users = result.scalars().all()

# 3 запроса:
# 1. SELECT * FROM users
# 2. SELECT * FROM posts WHERE user_id IN (...)
# 3. SELECT * FROM comments WHERE post_id IN (...)
```

## Индексы в PostgreSQL

### Что такое индекс?

**Индекс** - это структура данных, которая ускоряет поиск в таблице.

**Аналогия:** Индекс в БД = Оглавление в книге

**Без индекса:**
```sql
-- Sequential Scan - проверяет каждую строку
SELECT * FROM users WHERE email = 'john@example.com';
-- Проверено 1,000,000 строк за 500ms
```

**С индексом:**
```sql
-- Index Scan - использует индекс
SELECT * FROM users WHERE email = 'john@example.com';
-- Проверено 1 строка за 2ms
```

### Типы индексов в PostgreSQL

#### 1. B-tree индекс (по умолчанию)

**Лучший выбор для большинства случаев.**

```sql
-- Создание индекса
CREATE INDEX idx_users_email ON users(email);

-- Эффективен для:
-- Равенство
SELECT * FROM users WHERE email = 'john@example.com';

-- Сравнения
SELECT * FROM users WHERE created_at > '2024-01-01';

-- LIKE с началом строки
SELECT * FROM users WHERE username LIKE 'john%';

-- Сортировка
SELECT * FROM users ORDER BY email;
```

#### 2. Hash индекс

**Только для проверки на равенство.**

```sql
CREATE INDEX idx_users_email_hash ON users USING HASH (email);

-- Эффективен для:
SELECT * FROM users WHERE email = 'john@example.com';

-- НЕ эффективен для:
-- Сравнений, LIKE, сортировки
```

#### 3. GIN индекс (Generalized Inverted Index)

**Для полнотекстового поиска, массивов, JSONB.**

```sql
-- Для массивов
CREATE INDEX idx_posts_tags ON posts USING GIN (tags);
SELECT * FROM posts WHERE tags @> ARRAY['python', 'asyncio'];

-- Для JSONB
CREATE INDEX idx_users_metadata ON users USING GIN (metadata);
SELECT * FROM users WHERE metadata @> '{"country": "US"}';

-- Для полнотекстового поиска
CREATE INDEX idx_posts_content ON posts USING GIN (to_tsvector('english', content));
SELECT * FROM posts WHERE to_tsvector('english', content) @@ to_tsquery('python & asyncio');
```

#### 4. Partial индекс

**Индекс только для части данных.**

```sql
-- Индекс только для активных пользователей
CREATE INDEX idx_users_active_email
ON users(email)
WHERE is_active = true;

-- Эффективен для:
SELECT * FROM users WHERE email = 'john@example.com' AND is_active = true;
```

#### 5. Composite индекс

**Индекс по нескольким колонкам.**

```sql
-- Порядок колонок важен!
CREATE INDEX idx_posts_user_created
ON posts(user_id, created_at DESC);

-- Эффективен для:
SELECT * FROM posts WHERE user_id = 1 ORDER BY created_at DESC;

-- Также эффективен для:
SELECT * FROM posts WHERE user_id = 1;

-- НЕ эффективен для:
SELECT * FROM posts WHERE created_at > '2024-01-01';  -- только вторая колонка
```

### Создание индексов в PostgreSQL (через SQL)

```python
# Создание индексов через SQL или миграции

# Простой индекс
await conn.execute("CREATE INDEX idx_users_email ON users(email);")
await conn.execute("CREATE INDEX idx_users_username ON users(username);")

# Composite индекс
await conn.execute("""
    CREATE INDEX idx_users_active_created
    ON users(is_active, created_at);
""")

# Partial индекс (только для активных пользователей)
await conn.execute("""
    CREATE INDEX idx_users_active_email
    ON users(email)
    WHERE is_active = TRUE;
""")

# Unique constraint (автоматически создает индекс)
await conn.execute("""
    ALTER TABLE users
    ADD CONSTRAINT uq_users_email UNIQUE (email);
""")
# Примеры индексов для таблицы posts
await conn.execute("""
    CREATE INDEX idx_posts_user_id ON posts(user_id);
""")

await conn.execute("""
    CREATE INDEX idx_posts_tags ON posts USING GIN (tags);
""")

await conn.execute("""
    CREATE INDEX idx_posts_user_created ON posts(user_id, created_at);
""")
```

### Миграция для добавления индексов

```python
# alembic/versions/xxx_add_indexes.py
from alembic import op

def upgrade():
    # Создание индексов
    op.create_index(
        'idx_users_email',
        'users',
        ['email']
    )

    op.create_index(
        'idx_posts_user_created',
        'posts',
        ['user_id', 'created_at']
    )

    # Concurrent создание (не блокирует таблицу)
    op.create_index(
        'idx_posts_content',
        'posts',
        ['content'],
        postgresql_concurrently=True
    )

def downgrade():
    op.drop_index('idx_users_email')
    op.drop_index('idx_posts_user_created')
    op.drop_index('idx_posts_content')
```

## EXPLAIN - анализ запросов

### Что такое EXPLAIN?

**EXPLAIN** показывает план выполнения запроса PostgreSQL.

### Базовое использование

```sql
EXPLAIN SELECT * FROM users WHERE email = 'john@example.com';
```

**Вывод:**
```
Seq Scan on users  (cost=0.00..18334.00 rows=1 width=100)
  Filter: (email = 'john@example.com')
```

**Расшифровка:**
- `Seq Scan` - последовательное сканирование (плохо!)
- `cost=0.00..18334.00` - оценка стоимости
- `rows=1` - ожидаемое количество строк
- `width=100` - средний размер строки

### EXPLAIN ANALYZE

**EXPLAIN ANALYZE** - выполняет запрос и показывает реальную статистику.

```sql
EXPLAIN ANALYZE SELECT * FROM users WHERE email = 'john@example.com';
```

**Вывод:**
```
Index Scan using idx_users_email on users  (cost=0.42..8.44 rows=1 width=100) (actual time=0.025..0.026 rows=1 loops=1)
  Index Cond: (email = 'john@example.com')
Planning Time: 0.105 ms
Execution Time: 0.051 ms
```

**Что искать:**
- ✅ `Index Scan` - использует индекс (хорошо!)
- ❌ `Seq Scan` - полное сканирование (плохо!)
- ✅ `actual time` - реальное время
- ✅ `Planning Time` + `Execution Time` - общее время

### Типы Scan

| Тип | Описание | Скорость |
|-----|----------|----------|
| **Index Scan** | Использует индекс | ✅ Быстро |
| **Index Only Scan** | Только по индексу (не читает таблицу) | ✅✅ Очень быстро |
| **Bitmap Index Scan** | Использует несколько индексов | ✅ Быстро |
| **Seq Scan** | Последовательное сканирование | ❌ Медленно |

### EXPLAIN в asyncpg

```python
import asyncpg

async def explain_query(sql_query: str, *params):
    """Получить EXPLAIN для запроса."""
    pool = get_db_pool()
    async with pool.acquire() as conn:
        # Выполняем EXPLAIN ANALYZE
        explain_sql = f"EXPLAIN ANALYZE {sql_query}"
        result = await conn.fetch(explain_sql, *params)

        # Печатаем результат
        for row in result:
            print(row['QUERY PLAN'])


# Использование
await explain_query(
    "SELECT * FROM users WHERE email = $1",
    "john@example.com"
)
```

## Оптимизация запросов

### 1. Используйте Select Only Needed Columns

```python
# ❌ ПЛОХО - загружаем все колонки
users = await session.execute(select(User))

# ✅ ХОРОШО - только нужные колонки
users = await session.execute(
    select(User.id, User.username, User.email)
)
```

### 2. Используйте LIMIT

```python
# ❌ ПЛОХО - загружаем все
users = await session.execute(select(User))

# ✅ ХОРОШО - ограничиваем
users = await session.execute(
    select(User).limit(100)
)
```

### 3. Избегайте SELECT COUNT(*)

```python
# ❌ МЕДЛЕННО - полное сканирование
count = await session.scalar(
    select(func.count()).select_from(User)
)

# ✅ БЫСТРЕЕ - приблизительная оценка
result = await session.execute(
    text("SELECT reltuples::bigint FROM pg_class WHERE relname = 'users'")
)
approx_count = result.scalar()
```

### 4. Используйте EXISTS вместо COUNT

```python
# ❌ МЕДЛЕННО
count = await session.scalar(
    select(func.count()).select_from(Post).where(Post.user_id == user_id)
)
has_posts = count > 0

# ✅ БЫСТРО
exists_query = select(1).where(Post.user_id == user_id).exists()
has_posts = await session.scalar(select(exists_query))
```

### 5. Batch операции

```python
# ❌ ПЛОХО - N запросов
for user_data in users_data:
    user = User(**user_data)
    session.add(user)
    await session.commit()  # Каждый раз!

# ✅ ХОРОШО - 1 запрос
users = [User(**data) for data in users_data]
session.add_all(users)
await session.commit()  # Один раз!
```

### 6. Bulk операции

```python
# Bulk insert
await session.execute(
    insert(User),
    [
        {"username": "user1", "email": "user1@example.com"},
        {"username": "user2", "email": "user2@example.com"},
        {"username": "user3", "email": "user3@example.com"},
    ]
)

# Bulk update
await session.execute(
    update(User)
    .where(User.is_active == False)
    .values(deleted_at=func.now())
)
```

## Профилирование в Python

### 1. Логирование SQL запросов

```python
# src/db/session.py
import logging

# Включаем логирование SQL
logging.basicConfig()
# Для asyncpg можно логировать через callback или middleware

# Теперь все SQL запросы будут в логах
```

### 2. Подсчет запросов

```python
class QueryCounter:
    """Подсчет количества SQL запросов."""

    def __init__(self):
        self.count = 0

    def __enter__(self):
        # Для asyncpg можно использовать wrapper или middleware
        # для подсчета запросов
        return self

    def __exit__(self, *args):
        print(f"Total queries: {self.count}")


# Использование
async def get_users():
    with QueryCounter() as counter:
        users = await session.execute(
            select(User).options(selectinload(User.posts))
        )
        users = users.scalars().all()
    # Total queries: 2
```

### 3. Middleware для профилирования

```python
# src/middleware/profiler.py
from aiohttp import web
import time


@web.middleware
async def profiler_middleware(request, handler):
    """Middleware для профилирования запросов."""
    start_time = time.time()

    # Счетчик SQL запросов
    request['sql_queries'] = 0

    try:
        response = await handler(request)

        # Добавляем headers с метриками
        duration = time.time() - start_time
        response.headers['X-Response-Time'] = f"{duration:.3f}s"
        response.headers['X-SQL-Queries'] = str(request.get('sql_queries', 0))

        # Логируем медленные запросы
        if duration > 1.0:
            print(f"⚠️ Slow request: {request.path} took {duration:.3f}s")

        return response

    except Exception as e:
        duration = time.time() - start_time
        print(f"❌ Error in {request.path} after {duration:.3f}s: {e}")
        raise
```

### 4. py-spy для профилирования

```bash
# Установка
pip install py-spy

# Профилирование работающего процесса
py-spy top --pid <PID>

# Flame graph
py-spy record -o profile.svg -- python app.py
```

## Connection Pooling

### Настройка pool в asyncpg

```python
import asyncpg

pool = await asyncpg.create_pool(
    DATABASE_URL,

    # Pool settings
    min_size=10,                # Минимальное количество соединений
    max_size=20,                # Максимальное количество соединений
    max_queries=50000,          # Макс запросов на соединение
    max_inactive_connection_lifetime=300,  # Время жизни неактивного соединения
    command_timeout=60,         # Таймаут выполнения команды

    # Настройки подключения
    timeout=10,                 # Таймаут подключения
)

# Итого: макс 20 соединений
```

### Мониторинг pool

```python
async def get_pool_stats():
    """Статистика connection pool."""
    return {
        "size": engine.pool.size(),
        "checked_in": engine.pool.checkedin(),
        "checked_out": engine.pool.checkedout(),
        "overflow": engine.pool.overflow(),
    }
```

## Best Practices

### 1. Всегда добавляйте индексы для Foreign Keys

```python
class Post(Base):
    __tablename__ = "posts"

    user_id = Column(Integer, ForeignKey('users.id'))

    __table_args__ = (
        # Индекс для FK
        Index('idx_posts_user_id', 'user_id'),
    )
```

### 2. Используйте Composite индексы правильно

```python
# Если часто делаете:
# WHERE user_id = ? ORDER BY created_at DESC

# Создайте индекс:
Index('idx_posts_user_created', 'user_id', 'created_at')

# НЕ наоборот!
```

### 3. Не создавайте избыточные индексы

```python
# ❌ ПЛОХО - избыточные индексы
Index('idx_users_email', 'email')
Index('idx_users_email_active', 'email', 'is_active')  # Избыточно!

# ✅ ХОРОШО - один составной индекс
Index('idx_users_email_active', 'email', 'is_active')
```

### 4. Используйте Read Replicas для чтения

```python
# Master для записи
master_engine = create_async_engine(MASTER_URL)

# Replica для чтения
replica_engine = create_async_engine(REPLICA_URL)


async def get_users():
    """Чтение из replica."""
    async with AsyncSession(replica_engine) as session:
        result = await session.execute(select(User))
        return result.scalars().all()


async def create_user(data):
    """Запись в master."""
    async with AsyncSession(master_engine) as session:
        user = User(**data)
        session.add(user)
        await session.commit()
        return user
```

## Дополнительные материалы

### Документация
- [PostgreSQL Indexes](https://www.postgresql.org/docs/current/indexes.html)
- [asyncpg Documentation](https://magicstack.github.io/asyncpg/)
- [PostgreSQL Performance](https://www.postgresql.org/docs/current/performance-tips.html)
- [PostgreSQL EXPLAIN](https://www.postgresql.org/docs/current/sql-explain.html)

### Статьи
- [Use The Index, Luke!](https://use-the-index-luke.com/)
- [PostgreSQL Performance Tuning](https://wiki.postgresql.org/wiki/Performance_Optimization)
- [N+1 Queries Explained](https://stackoverflow.com/questions/97197/what-is-the-n1-selects-problem)

### Книги
- "High Performance PostgreSQL" - Gregory Smith
- "The Art of PostgreSQL" - Dimitri Fontaine

### Видео
- [Indexes in PostgreSQL](https://www.youtube.com/watch?v=HubezKbFL7E)
- [Query Optimization](https://www.youtube.com/watch?v=q8jwVNk6Y7A)

## Следующая неделя

На [Неделе 12](../../module-4-api-contracts/week-12/README.md) изучим OpenAPI/Swagger для документирования API! 📚

---

**Удачи с оптимизацией БД! 🚀**


