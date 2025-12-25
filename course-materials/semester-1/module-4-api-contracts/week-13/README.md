# Неделя 13: API Versioning, CORS и Rate Limiting

## Цели недели
- Понять необходимость версионирования API
- Изучить различные стратегии версионирования
- Научиться настраивать CORS для безопасности
- Освоить Rate Limiting для защиты от злоупотреблений
- Реализовать версионированные endpoints (v1, v2)
- Интегрировать внешние сервисы безопасно

## Теория

### Зачем нужно версионирование API?

**Проблема без версионирования:**
```
Версия 1.0: GET /api/users → возвращает {id, name}
Версия 2.0: GET /api/users → возвращает {id, name, email, avatar}
→ Все старые клиенты ломаются! 😱
```

**С версионированием:**
```
v1: GET /api/v1/users → стабильная версия
v2: GET /api/v2/users → новая версия
→ Старые клиенты продолжают работать ✅
```

### Когда нужна новая версия API?

**Breaking changes (требуют новой версии):**
- Удаление или переименование поля
- Изменение типа поля
- Удаление endpoint
- Изменение обязательности параметра
- Изменение формата ответа

**Non-breaking changes (не требуют версии):**
- Добавление нового поля
- Добавление нового endpoint
- Добавление опционального параметра
- Исправление багов

### Стратегии версионирования

#### 1. URL Versioning (Рекомендуется)

```python
# Версии в URL
/api/v1/users
/api/v2/users
```

**Преимущества:**
- ✅ Понятно и явно
- ✅ Легко кэшировать
- ✅ Можно запускать разные версии на разных серверах
- ✅ Просто в роутинге

**Недостатки:**
- ❌ Много дублирования кода
- ❌ URL становится длиннее

#### 2. Header Versioning

```python
# Версия в заголовке
GET /api/users
Accept: application/vnd.api.v1+json
```

**Преимущества:**
- ✅ Чистый URL
- ✅ RESTful подход

**Недостатки:**
- ❌ Сложнее для отладки
- ❌ Менее очевидно
- ❌ Кэширование сложнее

#### 3. Query Parameter Versioning

```python
# Версия в query параметре
/api/users?version=1
/api/users?version=2
```

**Преимущества:**
- ✅ Просто
- ✅ Легко тестировать

**Недостатки:**
- ❌ Не RESTful
- ❌ Параметры часто меняются
- ❌ Менее очевидно

#### 4. Media Type Versioning

```python
# Версия в Content-Type
Content-Type: application/vnd.example.v1+json
```

**Рекомендация:** Используйте **URL Versioning** для большинства случаев.

## Реализация версионирования в aiohttp

### Подход 1: Разные роутеры для версий

```python
# src/api/v1/routes.py
from aiohttp import web
from aiohttp.web import RouteDef

async def get_users_v1(request: web.Request) -> web.Response:
    """Получить пользователей v1."""
    users = await get_users_from_db()

    # v1 формат - только базовые поля
    return web.json_response({
        "users": [
            {
                "id": u.id,
                "name": u.username
            }
            for u in users
        ]
    })


async def create_user_v1(request: web.Request) -> web.Response:
    """Создать пользователя v1."""
    data = await request.json()

    # v1 валидация
    user = await create_user(
        username=data["name"],  # v1 использует "name"
        email=data["email"]
    )

    return web.json_response({
        "id": user.id,
        "name": user.username
    }, status=201)


# src/api/v2/routes.py
async def get_users_v2(request: web.Request) -> web.Response:
    """Получить пользователей v2."""
    users = await get_users_from_db()

    # v2 формат - больше полей
    return web.json_response({
        "users": [
            {
                "id": u.id,
                "username": u.username,
                "email": u.email,
                "avatar": u.avatar_url,
                "created_at": u.created_at.isoformat()
            }
            for u in users
        ]
    })


async def create_user_v2(request: web.Request) -> web.Response:
    """Создать пользователя v2."""
    data = await request.json()

    # v2 валидация - использует "username"
    user = await create_user(
        username=data["username"],
        email=data["email"],
        avatar_url=data.get("avatar_url")
    )

    return web.json_response({
        "id": user.id,
        "username": user.username,
        "email": user.email,
        "avatar": user.avatar_url,
        "created_at": user.created_at.isoformat()
    }, status=201)


# src/app.py
from aiohttp import web
from src.api.v1.routes import get_users_v1, create_user_v1
from src.api.v2.routes import get_users_v2, create_user_v2

app = web.Application()

# v1 routes
app.router.add_get("/api/v1/users", get_users_v1)
app.router.add_post("/api/v1/users", create_user_v1)

# v2 routes
app.router.add_get("/api/v2/users", get_users_v2)
app.router.add_post("/api/v2/users", create_user_v2)
```

### Подход 2: Версионирование через middleware

```python
# src/middleware/versioning.py
from aiohttp import web
from aiohttp.web import middleware

@middleware
async def versioning_middleware(request, handler):
    """Middleware для определения версии API."""
    path = request.path

    # Определяем версию из URL
    if path.startswith("/api/v1/"):
        request["api_version"] = "v1"
        # Убираем версию из пути для обработчика
        request.match_info = {}
        request.match_info["handler_path"] = path.replace("/api/v1", "")
    elif path.startswith("/api/v2/"):
        request["api_version"] = "v2"
        request.match_info = {}
        request.match_info["handler_path"] = path.replace("/api/v2", "")
    else:
        request["api_version"] = "v1"  # По умолчанию

    response = await handler(request)
    return response


# src/handlers/users.py
async def get_users(request: web.Request) -> web.Response:
    """Универсальный handler для разных версий."""
    version = request.get("api_version", "v1")
    users = await get_users_from_db()

    if version == "v1":
        return web.json_response({
            "users": [{"id": u.id, "name": u.username} for u in users]
        })
    elif version == "v2":
        return web.json_response({
            "users": [
                {
                    "id": u.id,
                    "username": u.username,
                    "email": u.email,
                    "avatar": u.avatar_url
                }
                for u in users
            ]
        })


# src/app.py
app = web.Application(middlewares=[versioning_middleware])
app.router.add_get("/api/v1/users", get_users)
app.router.add_get("/api/v2/users", get_users)
```

### Подход 3: Версионирование через классы

```python
# src/api/base.py
from abc import ABC, abstractmethod
from aiohttp import web

class APIHandler(ABC):
    """Базовый класс для версионированных handlers."""

    @abstractmethod
    async def get_users(self, request: web.Request) -> web.Response:
        """Получить пользователей."""
        pass

    @abstractmethod
    async def create_user(self, request: web.Request) -> web.Response:
        """Создать пользователя."""
        pass


# src/api/v1/handlers.py
from src.api.base import APIHandler
from aiohttp import web

class V1Handler(APIHandler):
    """Handlers для API v1."""

    async def get_users(self, request: web.Request) -> web.Response:
        users = await get_users_from_db()
        return web.json_response({
            "users": [{"id": u.id, "name": u.username} for u in users]
        })

    async def create_user(self, request: web.Request) -> web.Response:
        data = await request.json()
        user = await create_user(
            username=data["name"],
            email=data["email"]
        )
        return web.json_response({
            "id": user.id,
            "name": user.username
        }, status=201)


# src/api/v2/handlers.py
class V2Handler(APIHandler):
    """Handlers для API v2."""

    async def get_users(self, request: web.Request) -> web.Response:
        users = await get_users_from_db()
        return web.json_response({
            "users": [
                {
                    "id": u.id,
                    "username": u.username,
                    "email": u.email,
                    "avatar": u.avatar_url
                }
                for u in users
            ]
        })

    async def create_user(self, request: web.Request) -> web.Response:
        data = await request.json()
        user = await create_user(
            username=data["username"],
            email=data["email"],
            avatar_url=data.get("avatar_url")
        )
        return web.json_response({
            "id": user.id,
            "username": user.username,
            "email": user.email
        }, status=201)


# src/app.py
v1_handler = V1Handler()
v2_handler = V2Handler()

app.router.add_get("/api/v1/users", v1_handler.get_users)
app.router.add_post("/api/v1/users", v1_handler.create_user)
app.router.add_get("/api/v2/users", v2_handler.get_users)
app.router.add_post("/api/v2/users", v2_handler.create_user)
```

### Deprecation (устаревание старых версий)

```python
# src/api/v1/routes.py
from aiohttp import web
from datetime import datetime, timedelta

async def get_users_v1(request: web.Request) -> web.Response:
    """Получить пользователей v1 (deprecated)."""
    # Добавляем заголовки о deprecated версии
    response = web.json_response({
        "users": [{"id": u.id, "name": u.username} for u in users]
    })

    # Указываем что версия устарела
    sunset_date = datetime.now() + timedelta(days=180)
    response.headers.add(
        "Sunset",
        sunset_date.strftime("%a, %d %b %Y %H:%M:%S GMT")
    )
    response.headers.add("Deprecation", "true")
    response.headers.add(
        "Link",
        '</api/v2/users>; rel="successor-version"'
    )

    return response
```

## CORS (Cross-Origin Resource Sharing)

### Что такое CORS?

**CORS** - механизм, позволяющий браузеру запрашивать ресурсы с другого домена.

**Проблема:**
```
Frontend: https://myapp.com
Backend:  https://api.myapp.com

Браузер блокирует запросы между разными доменами!
```

**Решение:** CORS headers.

### CORS Headers

**Основные заголовки:**
- `Access-Control-Allow-Origin` - разрешенные домены
- `Access-Control-Allow-Methods` - разрешенные HTTP методы
- `Access-Control-Allow-Headers` - разрешенные заголовки
- `Access-Control-Allow-Credentials` - разрешить cookies
- `Access-Control-Max-Age` - кэширование preflight запросов

### Простой CORS

```python
# src/middleware/cors.py
from aiohttp import web

@web.middleware
async def cors_middleware(request, handler):
    """Простой CORS middleware."""

    # Разрешаем все домены (НЕ для production!)
    response = await handler(request)
    response.headers["Access-Control-Allow-Origin"] = "*"
    response.headers["Access-Control-Allow-Methods"] = "GET, POST, PUT, DELETE, OPTIONS"
    response.headers["Access-Control-Allow-Headers"] = "Content-Type, Authorization"

    return response


# src/app.py
app = web.Application(middlewares=[cors_middleware])
```

### Безопасный CORS

```python
# src/middleware/cors.py
from aiohttp import web
from urllib.parse import urlparse

ALLOWED_ORIGINS = [
    "https://myapp.com",
    "https://www.myapp.com",
    "http://localhost:3000",  # Для разработки
    "http://localhost:8080"
]

@web.middleware
async def cors_middleware(request, handler):
    """Безопасный CORS middleware."""

    # Получаем Origin из запроса
    origin = request.headers.get("Origin")

    # Если это OPTIONS (preflight), обрабатываем отдельно
    if request.method == "OPTIONS":
        response = web.Response()

        if origin and origin in ALLOWED_ORIGINS:
            response.headers["Access-Control-Allow-Origin"] = origin
            response.headers["Access-Control-Allow-Methods"] = "GET, POST, PUT, DELETE, PATCH, OPTIONS"
            response.headers["Access-Control-Allow-Headers"] = "Content-Type, Authorization, X-Requested-With"
            response.headers["Access-Control-Allow-Credentials"] = "true"
            response.headers["Access-Control-Max-Age"] = "3600"

        return response

    # Обычный запрос
    response = await handler(request)

    if origin and origin in ALLOWED_ORIGINS:
        response.headers["Access-Control-Allow-Origin"] = origin
        response.headers["Access-Control-Allow-Credentials"] = "true"

    return response


# src/app.py
app = web.Application(middlewares=[cors_middleware])
```

### CORS с aiohttp-cors

```python
# Установка
# pip install aiohttp-cors

# src/app.py
from aiohttp import web
from aiohttp_cors import setup as cors_setup, ResourceOptions

app = web.Application()

# Настройка CORS
cors = cors_setup(app, defaults={
    "*": ResourceOptions(
        allow_credentials=True,
        expose_headers="*",
        allow_headers="*",
        allow_methods="*"
    )
})

# Для конкретных routes
cors.add(app.router.add_get("/api/users", get_users))

# Или для всех routes
for route in list(app.router.routes()):
    cors.add(route)
```

### Настройка CORS в production

```python
# src/config.py
import os

# Из переменных окружения
ALLOWED_ORIGINS = os.getenv(
    "CORS_ALLOWED_ORIGINS",
    "https://myapp.com,https://www.myapp.com"
).split(",")

CORS_CREDENTIALS = os.getenv("CORS_ALLOW_CREDENTIALS", "true").lower() == "true"
CORS_MAX_AGE = int(os.getenv("CORS_MAX_AGE", "3600"))


# src/middleware/cors.py
@web.middleware
async def cors_middleware(request, handler):
    """Production-ready CORS."""
    origin = request.headers.get("Origin")

    if request.method == "OPTIONS":
        response = web.Response()

        if origin in ALLOWED_ORIGINS:
            response.headers["Access-Control-Allow-Origin"] = origin
            response.headers["Access-Control-Allow-Methods"] = "GET, POST, PUT, DELETE, OPTIONS"
            response.headers["Access-Control-Allow-Headers"] = "Content-Type, Authorization"
            response.headers["Access-Control-Allow-Credentials"] = str(CORS_CREDENTIALS).lower()
            response.headers["Access-Control-Max-Age"] = str(CORS_MAX_AGE)

        return response

    response = await handler(request)

    if origin in ALLOWED_ORIGINS:
        response.headers["Access-Control-Allow-Origin"] = origin
        if CORS_CREDENTIALS:
            response.headers["Access-Control-Allow-Credentials"] = "true"

    return response
```

## Rate Limiting

### Зачем нужен Rate Limiting?

**Проблемы без rate limiting:**
- DDoS атаки
- Злоупотребление API
- Несправедливое использование ресурсов
- Высокая нагрузка на сервер

**Rate Limiting решает:**
- ✅ Защита от злоупотреблений
- ✅ Справедливое распределение ресурсов
- ✅ Защита от DDoS
- ✅ Предсказуемая нагрузка

### Стратегии Rate Limiting

#### 1. Fixed Window (Фиксированное окно)

```
Временное окно: 1 минута
Лимит: 100 запросов

[00:00 - 01:00] → 100 запросов
[01:00 - 02:00] → новый лимит (100 запросов)
```

**Проблема:** Burst в начале окна может превысить лимит.

#### 2. Sliding Window (Скользящее окно)

```
Смотрим последние 60 секунд
Если запросов >= 100 → блокируем
```

**Преимущество:** Более справедливо.

#### 3. Token Bucket (Ведро токенов)

```
Ведро на 100 токенов
Каждый запрос = 1 токен
Токены пополняются со скоростью 1/сек
```

**Преимущество:** Позволяет "накопить" токены.

### Реализация Rate Limiting

#### Простой Rate Limiter

```python
# src/middleware/rate_limit.py
from aiohttp import web
from collections import defaultdict
from datetime import datetime, timedelta
import asyncio

# Простое хранилище в памяти
rate_limit_storage = defaultdict(list)

def get_client_id(request: web.Request) -> str:
    """Получить идентификатор клиента."""
    # По IP адресу
    return request.remote

    # Или по API ключу
    # return request.headers.get("X-API-Key", request.remote)


@web.middleware
async def rate_limit_middleware(request, handler):
    """Простой rate limiter."""
    client_id = get_client_id(request)
    now = datetime.now()

    # Очищаем старые записи
    rate_limit_storage[client_id] = [
        timestamp for timestamp in rate_limit_storage[client_id]
        if now - timestamp < timedelta(minutes=1)
    ]

    # Проверяем лимит (100 запросов в минуту)
    if len(rate_limit_storage[client_id]) >= 100:
        return web.json_response({
            "error": "Rate limit exceeded",
            "message": "Too many requests. Please try again later.",
            "retry_after": 60
        }, status=429)

    # Добавляем текущий запрос
    rate_limit_storage[client_id].append(now)

    # Обрабатываем запрос
    response = await handler(request)

    # Добавляем заголовки с информацией о лимите
    response.headers["X-RateLimit-Limit"] = "100"
    response.headers["X-RateLimit-Remaining"] = str(
        100 - len(rate_limit_storage[client_id])
    )
    response.headers["X-RateLimit-Reset"] = str(
        int((now + timedelta(minutes=1)).timestamp())
    )

    return response
```

#### Rate Limiting с Redis

```python
# src/middleware/rate_limit.py
import redis.asyncio as redis
from aiohttp import web
import json

redis_client = None

async def init_redis():
    """Инициализация Redis."""
    global redis_client
    redis_client = await redis.from_url("redis://localhost:6379")


def get_client_id(request: web.Request) -> str:
    """Получить идентификатор клиента."""
    # По API ключу, если есть
    api_key = request.headers.get("X-API-Key")
    if api_key:
        return f"api_key:{api_key}"

    # Иначе по IP
    return f"ip:{request.remote}"


@web.middleware
async def rate_limit_middleware(request, handler):
    """Rate limiter с Redis."""
    if not redis_client:
        await init_redis()

    client_id = get_client_id(request)

    # Разные лимиты для разных endpoint'ов
    endpoint = request.path
    if endpoint.startswith("/api/v1/auth"):
        limit = 5  # Логин - меньше лимит
        window = 60  # 1 минута
    elif endpoint.startswith("/api/v1/users"):
        limit = 100
        window = 60
    else:
        limit = 1000
        window = 3600  # 1 час

    key = f"rate_limit:{client_id}:{endpoint}"

    # Получаем текущее количество запросов
    current = await redis_client.get(key)

    if current and int(current) >= limit:
        reset_time = await redis_client.ttl(key)

        return web.json_response({
            "error": "Rate limit exceeded",
            "message": f"Too many requests. Limit: {limit} per {window}s",
            "retry_after": reset_time
        }, status=429, headers={
            "Retry-After": str(reset_time),
            "X-RateLimit-Limit": str(limit),
            "X-RateLimit-Remaining": "0",
            "X-RateLimit-Reset": str(reset_time)
        })

    # Увеличиваем счетчик
    pipe = redis_client.pipeline()
    pipe.incr(key)
    pipe.expire(key, window)
    await pipe.execute()

    # Обрабатываем запрос
    response = await handler(request)

    # Добавляем заголовки
    current_count = await redis_client.get(key)
    remaining = max(0, limit - int(current_count or 0))

    response.headers["X-RateLimit-Limit"] = str(limit)
    response.headers["X-RateLimit-Remaining"] = str(remaining)
    response.headers["X-RateLimit-Reset"] = str(await redis_client.ttl(key))

    return response
```

#### Rate Limiting с разными лимитами

```python
# src/config/rate_limits.py
RATE_LIMITS = {
    "default": {
        "limit": 100,
        "window": 60  # секунды
    },
    "/api/v1/auth/login": {
        "limit": 5,
        "window": 60
    },
    "/api/v1/auth/register": {
        "limit": 3,
        "window": 3600  # 1 час
    },
    "/api/v1/users": {
        "limit": 100,
        "window": 60
    },
    "/api/v1/posts": {
        "limit": 200,
        "window": 60
    }
}


# src/middleware/rate_limit.py
def get_rate_limit(request: web.Request) -> dict:
    """Получить настройки rate limit для endpoint."""
    path = request.path

    # Ищем точное совпадение
    if path in RATE_LIMITS:
        return RATE_LIMITS[path]

    # Ищем по префиксу
    for endpoint_path, config in RATE_LIMITS.items():
        if path.startswith(endpoint_path):
            return config

    # Дефолтный лимит
    return RATE_LIMITS["default"]
```

### Rate Limiting для аутентифицированных пользователей

```python
# src/middleware/rate_limit.py
async def rate_limit_middleware(request, handler):
    """Rate limiter с учетом аутентификации."""
    # Проверяем авторизован ли пользователь
    user = request.get("user")

    if user:
        # Авторизованные пользователи - больше лимит
        client_id = f"user:{user.id}"
        limit = 1000
        window = 3600
    else:
        # Анонимные - меньше лимит
        client_id = f"ip:{request.remote}"
        limit = 100
        window = 60

    # ... остальная логика
```

## Интеграция с внешними сервисами

### HTTP клиент с retry

```python
# src/services/http_client.py
from aiohttp import ClientSession, ClientError
import asyncio
from typing import Optional

class HTTPClient:
    """HTTP клиент с retry логикой."""

    def __init__(self, base_url: str, timeout: int = 10):
        self.base_url = base_url
        self.timeout = timeout
        self.session: Optional[ClientSession] = None

    async def __aenter__(self):
        self.session = ClientSession(
            timeout=aiohttp.ClientTimeout(total=self.timeout)
        )
        return self

    async def __aexit__(self, *args):
        if self.session:
            await self.session.close()

    async def get(self, path: str, retries: int = 3, **kwargs):
        """GET запрос с retry."""
        for attempt in range(retries):
            try:
                async with self.session.get(
                    f"{self.base_url}{path}",
                    **kwargs
                ) as response:
                    if response.status < 500:
                        return await response.json()
                    elif attempt == retries - 1:
                        response.raise_for_status()

            except ClientError as e:
                if attempt == retries - 1:
                    raise

                # Exponential backoff
                await asyncio.sleep(2 ** attempt)

    async def post(self, path: str, data: dict, retries: int = 3):
        """POST запрос с retry."""
        for attempt in range(retries):
            try:
                async with self.session.post(
                    f"{self.base_url}{path}",
                    json=data
                ) as response:
                    if response.status < 500:
                        return await response.json()
                    elif attempt == retries - 1:
                        response.raise_for_status()

            except ClientError as e:
                if attempt == retries - 1:
                    raise

                await asyncio.sleep(2 ** attempt)


# Использование
async def call_external_api():
    async with HTTPClient("https://api.external.com") as client:
        data = await client.get("/endpoint")
        return data
```

### Circuit Breaker Pattern

```python
# src/services/circuit_breaker.py
from enum import Enum
from datetime import datetime, timedelta
import asyncio

class CircuitState(Enum):
    CLOSED = "closed"  # Работает нормально
    OPEN = "open"      # Не работает, не пытаемся
    HALF_OPEN = "half_open"  # Проверяем восстановление


class CircuitBreaker:
    """Circuit Breaker для защиты от каскадных сбоев."""

    def __init__(
        self,
        failure_threshold: int = 5,
        timeout: int = 60,
        expected_exception: type = Exception
    ):
        self.failure_threshold = failure_threshold
        self.timeout = timeout
        self.expected_exception = expected_exception

        self.failure_count = 0
        self.last_failure_time = None
        self.state = CircuitState.CLOSED

    async def call(self, func, *args, **kwargs):
        """Вызвать функцию через circuit breaker."""
        if self.state == CircuitState.OPEN:
            if datetime.now() - self.last_failure_time > timedelta(seconds=self.timeout):
                self.state = CircuitState.HALF_OPEN
            else:
                raise Exception("Circuit breaker is OPEN")

        try:
            result = await func(*args, **kwargs)
            self.on_success()
            return result

        except self.expected_exception as e:
            self.on_failure()
            raise

    def on_success(self):
        """Обработка успешного вызова."""
        self.failure_count = 0
        self.state = CircuitState.CLOSED

    def on_failure(self):
        """Обработка неуспешного вызова."""
        self.failure_count += 1
        self.last_failure_time = datetime.now()

        if self.failure_count >= self.failure_threshold:
            self.state = CircuitState.OPEN


# Использование
circuit_breaker = CircuitBreaker(failure_threshold=5, timeout=60)

async def call_external_service():
    """Вызов внешнего сервиса через circuit breaker."""
    try:
        result = await circuit_breaker.call(
            external_api_client.get,
            "/endpoint"
        )
        return result
    except Exception as e:
        # Сервис недоступен - используем fallback
        return {"status": "fallback", "data": "cached"}
```

## Best Practices

### 1. Версионирование

- ✅ Используйте URL versioning
- ✅ Поддерживайте минимум 2 версии одновременно
- ✅ Документируйте deprecated версии
- ✅ Давайте достаточно времени для миграции (3-6 месяцев)
- ✅ Используйте семантическое версионирование (MAJOR.MINOR.PATCH)

### 2. CORS

- ✅ Настраивайте конкретные домены, не "*"
- ✅ Используйте переменные окружения для allowed origins
- ✅ Ограничивайте методы и заголовки
- ✅ Тестируйте CORS в разных браузерах

### 3. Rate Limiting

- ✅ Используйте Redis для распределенных систем
- ✅ Разные лимиты для разных endpoints
- ✅ Больше лимит для авторизованных пользователей
- ✅ Добавляйте заголовки с информацией о лимитах
- ✅ Используйте sliding window для справедливости

### 4. Интеграция

- ✅ Используйте retry с exponential backoff
- ✅ Реализуйте circuit breaker для защиты
- ✅ Используйте таймауты
- ✅ Логируйте все внешние вызовы
- ✅ Кэшируйте результаты где возможно

## Дополнительные материалы

### Документация
- [aiohttp CORS](https://docs.aiohttp.org/en/stable/web_advanced.html#cors-support)
- [Redis Rate Limiting](https://redis.io/docs/manual/patterns/rate-limiting/)
- [HTTP Status 429](https://developer.mozilla.org/en-US/docs/Web/HTTP/Status/429)

### Статьи
- [API Versioning Best Practices](https://www.baeldung.com/rest-api-versioning)
- [CORS Explained](https://developer.mozilla.org/en-US/docs/Web/HTTP/CORS)
- [Rate Limiting Patterns](https://www.alexedwards.net/blog/how-to-rate-limit-http-requests)

### Видео
- [API Versioning Strategies](https://www.youtube.com/watch?v=0oRL8riO7tI)
- [Understanding CORS](https://www.youtube.com/watch?v=4KHiSt0oLJ0)
- [Rate Limiting Deep Dive](https://www.youtube.com/watch?v=m64SWl9bfvk)

## Следующая неделя

На [Неделе 14](../week-14/README.md) будет итоговая работа - защита проекта семестра! 🚀

---

**Удачи с API contracts! 🔗**

