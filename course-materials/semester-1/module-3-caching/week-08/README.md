# Неделя 8: Redis и кэширование

## Цели недели
- Понять принципы кэширования и когда его применять
- Освоить Redis - in-memory database
- Изучить стратегии кэширования (Cache-Aside, Write-Through, Write-Behind)
- Реализовать кэширование в aiohttp приложении
- Научиться работать с TTL (Time To Live)
- Понять паттерны инвалидации кэша

## Теория

### Что такое кэширование?

**Кэширование** - это сохранение результатов дорогих операций для повторного использования.

```
БЕЗ КЭША:
User Request → API → Database Query (500ms) → Response
User Request → API → Database Query (500ms) → Response
User Request → API → Database Query (500ms) → Response

С КЭШЕМ:
User Request → API → Database Query (500ms) → Cache → Response
User Request → API → Cache (5ms) → Response
User Request → API → Cache (5ms) → Response

Ускорение: 100x! 🚀
```

**Когда применять кэширование:**
- ✅ Данные редко меняются
- ✅ Дорогие вычисления
- ✅ Медленные запросы к БД
- ✅ API вызовы к внешним сервисам
- ✅ Статические данные (конфигурация, справочники)

**Когда НЕ применять:**
- ❌ Данные постоянно меняются
- ❌ Требуется абсолютная актуальность
- ❌ Персональные данные
- ❌ Финансовые транзакции

### Что такое Redis?

**Redis** (REmote DIctionary Server) - это in-memory key-value хранилище данных.

**Особенности:**
- ⚡ Очень быстрый (in-memory)
- 🔑 Key-value хранилище
- 📊 Поддержка структур данных (strings, lists, sets, hashes, sorted sets)
- ⏰ TTL (Time To Live) для ключей
- 💾 Персистентность (опционально)
- 📡 Pub/Sub messaging
- 🔐 Атомарные операции

**Типичные use cases:**
- Кэширование
- Сессии пользователей
- Rate limiting
- Leaderboards (рейтинги)
- Real-time analytics
- Message queues

### Архитектура с Redis

```
┌─────────────┐
│   Client    │
└──────┬──────┘
       │
       ▼
┌─────────────────────┐
│   API Server        │
│   (aiohttp)         │
└──────┬──────┬───────┘
       │      │
       │      └─────────────┐
       │                    │
       ▼                    ▼
┌──────────────┐    ┌──────────────┐
│    Redis     │    │  PostgreSQL  │
│  (Cache)     │    │  (Database)  │
│   <1ms       │    │   ~50ms      │
└──────────────┘    └──────────────┘
```

## Установка и настройка Redis

### Docker (рекомендуется)

```bash
# Запуск Redis в Docker
docker run --name redis-dev \
  -p 6379:6379 \
  -d redis:7-alpine

# Проверка
docker exec -it redis-dev redis-cli ping
# PONG
```

### docker-compose.yml

```yaml
version: '3.8'

services:
  redis:
    image: redis:7-alpine
    container_name: redis-dev
    ports:
      - "6379:6379"
    volumes:
      - redis_data:/data
    command: redis-server --appendonly yes
    healthcheck:
      test: ["CMD", "redis-cli", "ping"]
      interval: 10s
      timeout: 3s
      retries: 3

volumes:
  redis_data:
```

### Redis CLI - основные команды

```bash
# Подключение к Redis
redis-cli

# SET/GET
SET key "value"
GET key

# SET с TTL (в секундах)
SETEX key 60 "value"  # Истечет через 60 секунд

# Проверить TTL
TTL key  # Возвращает оставшееся время в секундах

# Удалить ключ
DEL key

# Проверить существование
EXISTS key

# Получить все ключи (осторожно в продакшене!)
KEYS *

# Информация о Redis
INFO

# Очистить всю БД (осторожно!)
FLUSHDB
```

## Работа с Redis в Python

### Установка библиотек

```bash
pip install redis aioredis
```

### Базовое подключение

```python
# src/redis_client.py
import redis.asyncio as redis
from typing import Optional


class RedisClient:
    """Клиент для работы с Redis."""

    def __init__(self, url: str = "redis://localhost:6379"):
        self.url = url
        self.client: Optional[redis.Redis] = None

    async def connect(self):
        """Подключение к Redis."""
        self.client = await redis.from_url(
            self.url,
            encoding="utf-8",
            decode_responses=True
        )
        print("✅ Connected to Redis")

    async def disconnect(self):
        """Отключение от Redis."""
        if self.client:
            await self.client.close()
            print("❌ Disconnected from Redis")

    async def get(self, key: str) -> Optional[str]:
        """Получить значение по ключу."""
        return await self.client.get(key)

    async def set(
        self,
        key: str,
        value: str,
        ttl: Optional[int] = None
    ):
        """Установить значение."""
        if ttl:
            await self.client.setex(key, ttl, value)
        else:
            await self.client.set(key, value)

    async def delete(self, key: str):
        """Удалить ключ."""
        await self.client.delete(key)

    async def exists(self, key: str) -> bool:
        """Проверить существование ключа."""
        return bool(await self.client.exists(key))


# Глобальный экземпляр
redis_client = RedisClient()
```

### Интеграция с aiohttp

```python
# src/app.py
from aiohttp import web
from src.redis_client import redis_client


async def on_startup(app: web.Application):
    """Callback при запуске приложения."""
    await redis_client.connect()
    app['redis'] = redis_client


async def on_cleanup(app: web.Application):
    """Callback при остановке приложения."""
    await redis_client.disconnect()


def create_app() -> web.Application:
    app = web.Application()

    # Регистрируем lifecycle callbacks
    app.on_startup.append(on_startup)
    app.on_cleanup.append(on_cleanup)

    # Настройка routes
    setup_routes(app)

    return app


if __name__ == '__main__':
    app = create_app()
    web.run_app(app, host='0.0.0.0', port=8000)
```

## Стратегии кэширования

### 1. Cache-Aside (Lazy Loading)

**Самая популярная стратегия.**

```
┌────────┐  1. Request   ┌────────┐
│ Client │──────────────→│  API   │
└────────┘               └───┬────┘
                             │
                        2. Check Cache
                             │
                             ▼
                      ┌──────────────┐
                      │    Redis     │
                      └──────┬───────┘
                             │
                     ┌───────┴────────┐
              Cache  │                │  Cache
              Hit    │                │  Miss
                     ▼                ▼
            ┌───────────┐      ┌──────────┐
            │  Return   │      │ Query DB │
            └───────────┘      └─────┬────┘
                                     │
                              4. Save to Cache
                                     │
                              5. Return Data
```

**Реализация:**

```python
# src/services/user_service.py
import json
from typing import Optional
from src.redis_client import redis_client
from src.models.user import User


class UserService:
    """Сервис для работы с пользователями."""

    CACHE_TTL = 300  # 5 минут

    async def get_by_id(self, user_id: int) -> Optional[User]:
        """Получить пользователя по ID с кэшированием."""
        cache_key = f"user:{user_id}"

        # 1. Проверяем кэш
        cached = await redis_client.get(cache_key)
        if cached:
            print(f"✅ Cache HIT for {cache_key}")
            # Десериализуем JSON
            data = json.loads(cached)
            return User(**data)

        print(f"❌ Cache MISS for {cache_key}")

        # 2. Запрос к БД
        async with async_session() as session:
            result = await session.execute(
                select(User).where(User.id == user_id)
            )
            user = result.scalar_one_or_none()

        if not user:
            return None

        # 3. Сохраняем в кэш
        user_dict = {
            "id": user.id,
            "username": user.username,
            "email": user.email,
            "is_active": user.is_active,
        }
        await redis_client.set(
            cache_key,
            json.dumps(user_dict),
            ttl=self.CACHE_TTL
        )

        return user

    async def update(self, user_id: int, data: dict) -> User:
        """Обновить пользователя и инвалидировать кэш."""
        async with async_session() as session:
            result = await session.execute(
                select(User).where(User.id == user_id)
            )
            user = result.scalar_one_or_none()

            if not user:
                raise ValueError("User not found")

            # Обновляем данные
            for key, value in data.items():
                setattr(user, key, value)

            await session.commit()
            await session.refresh(user)

        # Инвалидируем кэш
        cache_key = f"user:{user_id}"
        await redis_client.delete(cache_key)
        print(f"🗑️ Invalidated cache: {cache_key}")

        return user
```

**Преимущества:**
- ✅ Простая реализация
- ✅ Кэш заполняется по требованию
- ✅ Подходит для большинства случаев

**Недостатки:**
- ❌ Cache miss penalty (первый запрос медленный)
- ❌ Возможны устаревшие данные

### 2. Write-Through

**Данные записываются одновременно в кэш и БД.**

```python
class UserService:

    async def create(self, data: dict) -> User:
        """Создать пользователя с write-through кэшированием."""
        # 1. Создаем в БД
        async with async_session() as session:
            user = User(**data)
            session.add(user)
            await session.commit()
            await session.refresh(user)

        # 2. Одновременно сохраняем в кэш
        cache_key = f"user:{user.id}"
        user_dict = {
            "id": user.id,
            "username": user.username,
            "email": user.email,
        }
        await redis_client.set(
            cache_key,
            json.dumps(user_dict),
            ttl=self.CACHE_TTL
        )

        return user
```

**Преимущества:**
- ✅ Нет cache miss для новых данных
- ✅ Данные всегда актуальны

**Недостатки:**
- ❌ Медленнее запись (две операции)
- ❌ Может кэшировать ненужные данные

### 3. Write-Behind (Write-Back)

**Данные сначала пишутся в кэш, потом асинхронно в БД.**

```python
import asyncio
from typing import List


class UserService:

    def __init__(self):
        self.write_queue: List[dict] = []
        self.is_flushing = False

    async def create_async(self, data: dict) -> dict:
        """Создать пользователя с write-behind."""
        # 1. Генерируем временный ID
        temp_id = f"temp_{len(self.write_queue)}"

        # 2. Сохраняем в кэш
        cache_key = f"user:{temp_id}"
        await redis_client.set(
            cache_key,
            json.dumps(data),
            ttl=60
        )

        # 3. Добавляем в очередь на запись в БД
        self.write_queue.append(data)

        # 4. Запускаем фоновую запись
        if not self.is_flushing:
            asyncio.create_task(self.flush_to_db())

        return {"id": temp_id, **data}

    async def flush_to_db(self):
        """Фоновая запись в БД."""
        self.is_flushing = True

        while self.write_queue:
            data = self.write_queue.pop(0)

            # Записываем в БД
            async with async_session() as session:
                user = User(**data)
                session.add(user)
                await session.commit()

            await asyncio.sleep(0.1)  # Небольшая задержка

        self.is_flushing = False
```

**Преимущества:**
- ✅ Очень быстрая запись
- ✅ Снижение нагрузки на БД

**Недостатки:**
- ❌ Сложная реализация
- ❌ Риск потери данных при сбое
- ❌ Не подходит для критичных данных

## Продвинутые паттерны

### 1. Cache Warming

**Предзагрузка популярных данных в кэш.**

```python
async def warm_cache():
    """Прогрев кэша при запуске приложения."""
    print("🔥 Warming up cache...")

    # Загружаем топ-100 популярных пользователей
    async with async_session() as session:
        result = await session.execute(
            select(User)
            .order_by(User.login_count.desc())
            .limit(100)
        )
        popular_users = result.scalars().all()

    # Сохраняем в кэш
    for user in popular_users:
        cache_key = f"user:{user.id}"
        await redis_client.set(
            cache_key,
            json.dumps(user.to_dict()),
            ttl=3600
        )

    print(f"✅ Cached {len(popular_users)} users")


# В app.py
async def on_startup(app):
    await redis_client.connect()
    await warm_cache()  # Прогрев кэша
```

### 2. Cache Stampede Prevention

**Проблема:** Множество одновременных запросов при cache miss.

```
❌ БЕЗ ЗАЩИТЫ:
Cache expired → 1000 requests → 1000 DB queries! 💥

✅ С ЗАЩИТОЙ:
Cache expired → 1st request → DB query → Update cache
              → 999 requests → Wait or use stale cache
```

**Решение с Lock:**

```python
import asyncio
from typing import Optional


class CachedUserService:
    def __init__(self):
        self._locks = {}

    async def get_by_id(self, user_id: int) -> Optional[User]:
        """Получить пользователя с защитой от cache stampede."""
        cache_key = f"user:{user_id}"

        # Проверяем кэш
        cached = await redis_client.get(cache_key)
        if cached:
            return User(**json.loads(cached))

        # Используем lock для конкретного user_id
        lock_key = f"lock:{user_id}"

        if lock_key not in self._locks:
            self._locks[lock_key] = asyncio.Lock()

        async with self._locks[lock_key]:
            # Double-check после получения lock
            cached = await redis_client.get(cache_key)
            if cached:
                return User(**json.loads(cached))

            # Только один запрос пройдет сюда
            user = await self._fetch_from_db(user_id)

            if user:
                await redis_client.set(
                    cache_key,
                    json.dumps(user.to_dict()),
                    ttl=300
                )

            return user

    async def _fetch_from_db(self, user_id: int) -> Optional[User]:
        """Получить из БД."""
        async with async_session() as session:
            result = await session.execute(
                select(User).where(User.id == user_id)
            )
            return result.scalar_one_or_none()
```

### 3. Stale-While-Revalidate

**Возвращаем устаревшие данные, пока обновляем кэш в фоне.**

```python
async def get_with_stale(user_id: int) -> Optional[User]:
    """Получить с использованием stale cache."""
    cache_key = f"user:{user_id}"
    stale_key = f"stale:{user_id}"

    # Проверяем основной кэш
    cached = await redis_client.get(cache_key)
    if cached:
        return User(**json.loads(cached))

    # Проверяем stale кэш
    stale = await redis_client.get(stale_key)
    if stale:
        # Возвращаем устаревшие данные
        user = User(**json.loads(stale))

        # Асинхронно обновляем кэш
        asyncio.create_task(refresh_cache(user_id))

        return user

    # Если нет ни того, ни другого - запрос к БД
    return await fetch_and_cache(user_id)


async def refresh_cache(user_id: int):
    """Обновить кэш в фоне."""
    user = await fetch_from_db(user_id)
    if user:
        cache_key = f"user:{user_id}"
        stale_key = f"stale:{user_id}"
        data = json.dumps(user.to_dict())

        # Сохраняем в оба кэша
        await redis_client.set(cache_key, data, ttl=300)
        await redis_client.set(stale_key, data, ttl=3600)
```

## Кэширование в handlers

### Пример: Кэширование списка пользователей

```python
# src/handlers/users.py
from aiohttp import web
import json


async def get_users(request: web.Request) -> web.Response:
    """Получить список пользователей с кэшированием."""
    # Параметры pagination
    page = int(request.query.get('page', 1))
    limit = int(request.query.get('limit', 20))

    # Ключ кэша с параметрами
    cache_key = f"users:list:page={page}:limit={limit}"

    # Проверяем кэш
    redis = request.app['redis']
    cached = await redis.get(cache_key)

    if cached:
        print(f"✅ Cache HIT: {cache_key}")
        return web.json_response(
            json.loads(cached),
            headers={'X-Cache': 'HIT'}
        )

    print(f"❌ Cache MISS: {cache_key}")

    # Запрос к БД
    offset = (page - 1) * limit
    async with async_session() as session:
        result = await session.execute(
            select(User)
            .offset(offset)
            .limit(limit)
        )
        users = result.scalars().all()

    # Формируем ответ
    data = {
        "users": [
            {
                "id": u.id,
                "username": u.username,
                "email": u.email
            }
            for u in users
        ],
        "page": page,
        "limit": limit,
        "total": len(users)
    }

    # Сохраняем в кэш на 1 минуту
    await redis.set(
        cache_key,
        json.dumps(data),
        ttl=60
    )

    return web.json_response(
        data,
        headers={'X-Cache': 'MISS'}
    )
```

### Декоратор для кэширования

```python
# src/utils/cache_decorator.py
from functools import wraps
import json
from typing import Callable


def cached(
    key_pattern: str,
    ttl: int = 300
):
    """
    Декоратор для кэширования результатов функции.

    Args:
        key_pattern: Паттерн ключа, например "user:{user_id}"
        ttl: Время жизни кэша в секундах
    """
    def decorator(func: Callable):
        @wraps(func)
        async def wrapper(*args, **kwargs):
            # Формируем ключ из аргументов
            cache_key = key_pattern.format(**kwargs)

            # Проверяем кэш
            redis = redis_client
            cached = await redis.get(cache_key)

            if cached:
                print(f"✅ Cache HIT: {cache_key}")
                return json.loads(cached)

            print(f"❌ Cache MISS: {cache_key}")

            # Вызываем функцию
            result = await func(*args, **kwargs)

            # Сохраняем в кэш
            if result is not None:
                await redis.set(
                    cache_key,
                    json.dumps(result),
                    ttl=ttl
                )

            return result

        return wrapper
    return decorator


# Использование
@cached(key_pattern="user:{user_id}", ttl=300)
async def get_user_by_id(user_id: int) -> dict:
    """Получить пользователя по ID."""
    async with async_session() as session:
        result = await session.execute(
            select(User).where(User.id == user_id)
        )
        user = result.scalar_one_or_none()

        if not user:
            return None

        return {
            "id": user.id,
            "username": user.username,
            "email": user.email,
        }
```

## Мониторинг кэша

### Cache Hit Ratio

```python
# src/utils/cache_monitor.py
class CacheMonitor:
    """Мониторинг эффективности кэша."""

    def __init__(self):
        self.hits = 0
        self.misses = 0

    def record_hit(self):
        """Зафиксировать cache hit."""
        self.hits += 1

    def record_miss(self):
        """Зафиксировать cache miss."""
        self.misses += 1

    def get_hit_ratio(self) -> float:
        """Получить hit ratio (процент попаданий)."""
        total = self.hits + self.misses
        if total == 0:
            return 0.0
        return (self.hits / total) * 100

    def get_stats(self) -> dict:
        """Получить статистику."""
        return {
            "hits": self.hits,
            "misses": self.misses,
            "hit_ratio": f"{self.get_hit_ratio():.2f}%",
            "total": self.hits + self.misses
        }

    def reset(self):
        """Сбросить счетчики."""
        self.hits = 0
        self.misses = 0


# Глобальный монитор
cache_monitor = CacheMonitor()


# В коде кэширования
cached = await redis.get(cache_key)
if cached:
    cache_monitor.record_hit()
else:
    cache_monitor.record_miss()


# Endpoint для статистики
async def cache_stats(request: web.Request) -> web.Response:
    """Получить статистику кэша."""
    stats = cache_monitor.get_stats()

    # Redis info
    redis = request.app['redis']
    info = await redis.client.info('stats')

    return web.json_response({
        "cache_stats": stats,
        "redis_stats": {
            "total_connections_received": info.get('total_connections_received'),
            "total_commands_processed": info.get('total_commands_processed'),
            "keyspace_hits": info.get('keyspace_hits'),
            "keyspace_misses": info.get('keyspace_misses'),
        }
    })
```

## Best Practices

### 1. Naming Convention для ключей

```python
# ✅ Хорошо - структурированные ключи
user:123                    # Пользователь
user:123:posts             # Посты пользователя
user:123:profile           # Профиль
session:abc123             # Сессия
cache:users:list:page=1    # Список с параметрами

# ❌ Плохо
u123
user_data
cache1
```

### 2. Установка TTL

```python
# Всегда устанавливайте TTL!
await redis.set(key, value, ttl=300)  # ✅

# Без TTL кэш живет вечно
await redis.set(key, value)  # ❌ Может привести к memory leak
```

### 3. Сериализация данных

```python
import json
import pickle
from datetime import datetime


# ✅ JSON - предпочтительно
data = {"id": 1, "name": "John"}
await redis.set(key, json.dumps(data))

# Для сложных объектов можно использовать pickle
# Но JSON предпочтительнее для читаемости
user = User(id=1, name="John")
await redis.set(key, pickle.dumps(user))
```

### 4. Инвалидация кэша

```python
async def update_user(user_id: int, data: dict):
    """Обновить пользователя."""
    # 1. Обновляем в БД
    user = await db.update_user(user_id, data)

    # 2. Инвалидируем все связанные кэши
    await redis.delete(f"user:{user_id}")
    await redis.delete(f"user:{user_id}:profile")
    await redis.delete(f"user:{user_id}:posts")

    # Или используем паттерн
    keys = await redis.client.keys(f"user:{user_id}:*")
    if keys:
        await redis.client.delete(*keys)

    return user
```

## Дополнительные материалы

### Документация
- [Redis Documentation](https://redis.io/documentation)
- [redis-py Documentation](https://redis-py.readthedocs.io/)
- [Cache Strategies](https://aws.amazon.com/caching/best-practices/)

### Статьи
- [Caching Best Practices](https://redis.io/docs/manual/patterns/)
- [Cache Stampede Problem](https://en.wikipedia.org/wiki/Cache_stampede)
- [Redis Data Types](https://redis.io/docs/data-types/)

### Книги
- "Redis in Action" - Josiah Carlson
- "Designing Data-Intensive Applications" - Martin Kleppmann

### Видео
- [Redis Crash Course](https://www.youtube.com/watch?v=jgpVdJB2sKQ)
- [Caching Strategies](https://www.youtube.com/watch?v=U3RkDLtS7uY)

## Следующая неделя

На [Неделе 9](../week-09/README.md) изучим async/await и асинхронные паттерны! ⚡

---

**Удачи с кэшированием! ⚡**

