# Неделя 21: Межсервисная коммуникация

## Цели недели
- Изучить способы коммуникации между микросервисами
- Понять разницу между HTTP и gRPC
- Научиться проектировать и реализовывать API Gateway
- Освоить Service Discovery паттерн
- Научиться выбирать правильный способ коммуникации

## Теоретическая часть

### Типы межсервисной коммуникации

#### 1. Синхронная коммуникация

**Синхронная** - клиент ждет ответа от сервиса.

**Примеры:**
- HTTP/REST
- gRPC
- GraphQL

```
Client → Request → Service A
        ← Response ←
```

**Характеристики:**
- ✅ Простота реализации
- ✅ Прямое взаимодействие
- ❌ Зависимость от доступности сервиса
- ❌ Задержка = сумма задержек всех сервисов

#### 2. Асинхронная коммуникация

**Асинхронная** - клиент не ждет ответа, использует события/очереди.

**Примеры:**
- Message Queues (RabbitMQ, Kafka)
- Events/Event Bus
- Pub/Sub

```
Client → Event → Queue → Service A (async)
                   ↓
                 Service B (async)
```

**Характеристики:**
- ✅ Слабая связанность
- ✅ Отказоустойчивость
- ✅ Масштабируемость
- ❌ Сложность отладки
- ❌ Eventual consistency

## HTTP/REST vs gRPC

### HTTP/REST

**REST (Representational State Transfer)** - архитектурный стиль для веб-сервисов.

**Преимущества:**
- ✅ Простота и универсальность
- ✅ Человекочитаемый формат (JSON)
- ✅ Легкая отладка (можно через curl/browser)
- ✅ Широкая поддержка
- ✅ Кэширование из коробки

**Недостатки:**
- ❌ Overhead (JSON текстовый формат)
- ❌ Нет streaming из коробки
- ❌ Нет типизации (схемы отдельно)
- ❌ Over-fetching/under-fetching

**Пример:**
```python
# HTTP клиент для вызова другого сервиса
import aiohttp

async def get_user_from_auth_service(user_id: int):
    async with aiohttp.ClientSession() as session:
        async with session.get(
            f"http://auth-service:8001/api/v1/users/{user_id}"
        ) as response:
            if response.status == 200:
                return await response.json()
            else:
                raise Exception(f"Failed to get user: {response.status}")
```

### gRPC

**gRPC (gRPC Remote Procedure Calls)** - высокопроизводительный RPC фреймворк.

**Преимущества:**
- ✅ Высокая производительность (бинарный Protocol Buffers)
- ✅ Строгая типизация (из proto файлов)
- ✅ Streaming поддержка (unary, server, client, bidirectional)
- ✅ Меньше overhead
- ✅ Версионирование встроено

**Недостатки:**
- ❌ Сложнее чем REST
- ❌ Нет прямого доступа через браузер
- ❌ Нужны специальные клиенты
- ❌ Меньше инструментов для отладки

**Пример proto файла:**
```protobuf
// user.proto
syntax = "proto3";

service UserService {
  rpc GetUser(UserRequest) returns (UserResponse);
  rpc ListUsers(ListUsersRequest) returns (stream UserResponse);
}

message UserRequest {
  int32 user_id = 1;
}

message UserResponse {
  int32 id = 1;
  string username = 2;
  string email = 3;
}
```

**Python сервер (gRPC):**
```python
import grpc
from concurrent import futures
import user_pb2
import user_pb2_grpc

class UserService(user_pb2_grpc.UserServiceServicer):
    async def GetUser(self, request, context):
        user = await get_user_from_db(request.user_id)
        return user_pb2.UserResponse(
            id=user['id'],
            username=user['username'],
            email=user['email']
        )

def serve():
    server = grpc.aio.server(futures.ThreadPoolExecutor(max_workers=10))
    user_pb2_grpc.add_UserServiceServicer_to_server(UserService(), server)
    server.add_insecure_port('[::]:50051')
    server.start()
    server.wait_for_termination()
```

### Когда что использовать?

**Используйте HTTP/REST когда:**
- ✅ Публичный API для внешних клиентов
- ✅ Нужна простота и универсальность
- ✅ Клиенты - веб/мобильные приложения
- ✅ Нет жестких требований к производительности

**Используйте gRPC когда:**
- ✅ Внутренняя коммуникация между сервисами
- ✅ Нужна высокая производительность
- ✅ Streaming данных
- ✅ Сильная типизация критична
- ✅ Низкая латентность важна

## API Gateway

### Что такое API Gateway?

**API Gateway** - единая точка входа для всех клиентских запросов к микросервисам.

```
┌─────────────┐
│   Clients   │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│ API Gateway │
└───┬──────┬──┘
    │      │
    ▼      ▼
┌──────┐ ┌──────┐
│Auth  │ │Orders│
│Service│ │Service│
└──────┘ └──────┘
```

**Функции API Gateway:**
1. **Роутинг** - маршрутизация запросов к сервисам
2. **Агрегация** - объединение ответов от нескольких сервисов
3. **Аутентификация** - проверка токенов
4. **Rate Limiting** - ограничение запросов
5. **Load Balancing** - распределение нагрузки
6. **Мониторинг** - логирование и метрики
7. **Преобразование протоколов** - HTTP ↔ gRPC

### Преимущества API Gateway

- ✅ Единая точка входа для клиентов
- ✅ Централизованная аутентификация
- ✅ Возможность агрегации запросов
- ✅ Изоляция внутренней архитектуры
- ✅ Упрощение клиентского кода

### Недостатки

- ❌ Еще один компонент для поддержки
- ❌ Потенциальное узкое место (bottleneck)
- ❌ Сложность настройки

## Реализация API Gateway

### Простой API Gateway на aiohttp

```python
# gateway/main.py
from aiohttp import web
import aiohttp
from aiohttp.web import middleware
import json
import time

# Конфигурация сервисов
SERVICES = {
    'auth': 'http://auth-service:8001',
    'users': 'http://user-service:8002',
    'experiments': 'http://experiment-service:8003',
    'metrics': 'http://metrics-service:8004',
}

# HTTP клиент для проксирования
http_session = None

async def init_http_client(app):
    """Инициализация HTTP клиента."""
    global http_session
    http_session = aiohttp.ClientSession()

async def cleanup_http_client(app):
    """Закрытие HTTP клиента."""
    global http_session
    if http_session:
        await http_session.close()

# Middleware для логирования
@middleware
async def logging_middleware(request, handler):
    """Логирование запросов."""
    start_time = time.time()

    response = await handler(request)

    duration = time.time() - start_time
    print(f"{request.method} {request.path} - {response.status} - {duration:.3f}s")

    return response

# Middleware для аутентификации
@middleware
async def auth_middleware(request, handler):
    """Проверка аутентификации для защищенных endpoints."""
    # Публичные endpoints
    public_paths = ['/api/v1/auth/login', '/api/v1/auth/register', '/health']

    if request.path in public_paths:
        return await handler(request)

    # Проверка токена
    auth_header = request.headers.get('Authorization')
    if not auth_header:
        raise web.HTTPUnauthorized(reason="Missing Authorization header")

    # Проверка токена через Auth Service
    try:
        async with http_session.get(
            f"{SERVICES['auth']}/api/v1/auth/verify",
            headers={"Authorization": auth_header}
        ) as resp:
            if resp.status != 200:
                raise web.HTTPUnauthorized(reason="Invalid token")

            user_data = await resp.json()
            request['user'] = user_data

    except Exception as e:
        raise web.HTTPUnauthorized(reason=f"Auth verification failed: {e}")

    return await handler(request)

# Роутинг
async def proxy_request(request: web.Request, service_name: str):
    """Проксирование запроса к сервису."""
    service_url = SERVICES.get(service_name)
    if not service_url:
        raise web.HTTPNotFound(reason=f"Service {service_name} not found")

    # Формируем URL для сервиса
    path = request.path.replace(f"/api/v1/{service_name}", "")
    url = f"{service_url}{path}"

    # Проксируем query параметры
    if request.query_string:
        url = f"{url}?{request.query_string}"

    # Проксируем тело запроса
    body = None
    if request.can_read_body:
        body = await request.read()

    # Проксируем заголовки (убираем не нужные)
    headers = dict(request.headers)
    headers.pop('Host', None)
    headers.pop('Content-Length', None)

    # Выполняем запрос
    async with http_session.request(
        method=request.method,
        url=url,
        headers=headers,
        data=body,
        timeout=aiohttp.ClientTimeout(total=30)
    ) as resp:
        # Читаем ответ
        response_body = await resp.read()

        # Создаем response
        response = web.Response(
            body=response_body,
            status=resp.status,
            headers=dict(resp.headers)
        )

        return response

# Handlers
async def auth_handler(request: web.Request):
    """Обработка запросов к Auth Service."""
    return await proxy_request(request, 'auth')

async def users_handler(request: web.Request):
    """Обработка запросов к User Service."""
    return await proxy_request(request, 'users')

async def experiments_handler(request: web.Request):
    """Обработка запросов к Experiment Service."""
    return await proxy_request(request, 'experiments')

async def metrics_handler(request: web.Request):
    """Обработка запросов к Metrics Service."""
    return await proxy_request(request, 'metrics')

# Агрегированный endpoint
async def user_experiments_handler(request: web.Request):
    """Получить пользователя с его экспериментами (агрегация)."""
    user_id = request['user']['id']

    # Параллельные запросы к двум сервисам
    async with aiohttp.ClientSession() as session:
        # Запрос к User Service
        async with session.get(
            f"{SERVICES['users']}/api/v1/users/{user_id}"
        ) as user_resp:
            user_data = await user_resp.json()

        # Запрос к Experiment Service
        async with session.get(
            f"{SERVICES['experiments']}/api/v1/experiments?user_id={user_id}"
        ) as exp_resp:
            experiments = await exp_resp.json()

    return web.json_response({
        "user": user_data,
        "experiments": experiments
    })

async def health_handler(request: web.Request):
    """Health check для Gateway и всех сервисов."""
    health_status = {
        "gateway": "healthy",
        "services": {}
    }

    # Проверяем здоровье каждого сервиса
    async with aiohttp.ClientSession() as session:
        for service_name, service_url in SERVICES.items():
            try:
                async with session.get(
                    f"{service_url}/health",
                    timeout=aiohttp.ClientTimeout(total=2)
                ) as resp:
                    if resp.status == 200:
                        health_status["services"][service_name] = "healthy"
                    else:
                        health_status["services"][service_name] = "unhealthy"
            except Exception:
                health_status["services"][service_name] = "unreachable"

    # Если все сервисы здоровы
    all_healthy = all(
        status == "healthy"
        for status in health_status["services"].values()
    )

    status_code = 200 if all_healthy else 503

    return web.json_response(health_status, status=status_code)

# Создание приложения
def create_app():
    app = web.Application(middlewares=[logging_middleware, auth_middleware])

    # Routes
    app.router.add_get('/health', health_handler)
    app.router.add_get('/api/v1/auth/{path:.*}', auth_handler)
    app.router.add_post('/api/v1/auth/{path:.*}', auth_handler)
    app.router.add_get('/api/v1/users/{path:.*}', users_handler)
    app.router.add_get('/api/v1/users/me/experiments', user_experiments_handler)
    app.router.add_get('/api/v1/experiments/{path:.*}', experiments_handler)
    app.router.add_get('/api/v1/metrics/{path:.*}', metrics_handler)

    # Lifecycle
    app.on_startup.append(init_http_client)
    app.on_cleanup.append(cleanup_http_client)

    return app

if __name__ == '__main__':
    app = create_app()
    web.run_app(app, host='0.0.0.0', port=8000)
```

### Улучшенный API Gateway с кэшированием

```python
# gateway/middleware/cache.py
from aiohttp import web
import json
import hashlib
import time

cache_storage = {}  # В production использовать Redis

def cache_key(request: web.Request) -> str:
    """Генерация ключа кэша."""
    key_data = {
        'method': request.method,
        'path': request.path,
        'query': dict(request.query)
    }
    key_str = json.dumps(key_data, sort_keys=True)
    return hashlib.md5(key_str.encode()).hexdigest()

@web.middleware
async def cache_middleware(request, handler):
    """Middleware для кэширования GET запросов."""
    # Кэшируем только GET запросы
    if request.method != 'GET':
        return await handler(request)

    # Проверяем кэш
    key = cache_key(request)
    cached = cache_storage.get(key)

    if cached and time.time() - cached['timestamp'] < 60:  # 60 секунд
        return web.json_response(cached['data'])

    # Выполняем запрос
    response = await handler(request)

    # Кэшируем успешные ответы
    if response.status == 200:
        data = await response.json()
        cache_storage[key] = {
            'data': data,
            'timestamp': time.time()
        }

        # Возвращаем новый response (старый уже прочитан)
        return web.json_response(data)

    return response
```

## Service Discovery

### Что такое Service Discovery?

**Service Discovery** - механизм автоматического обнаружения сервисов в распределенной системе.

**Проблема без Service Discovery:**
```python
# ❌ Жестко закодированные адреса
auth_service = "http://auth-service:8001"  # Что если IP изменится?
```

**С Service Discovery:**
```python
# ✅ Динамическое обнаружение
auth_service = service_discovery.get("auth-service")
```

### Паттерны Service Discovery

#### 1. Client-side Discovery

**Клиент сам запрашивает registry:**
```
Client → Service Registry → Get address → Client → Service
```

**Пример:**
```python
# gateway/service_discovery.py
import aiohttp
from typing import Dict, Optional
import time

class ServiceDiscovery:
    """Простой Service Discovery."""

    def __init__(self, registry_url: str = "http://registry:8500"):
        self.registry_url = registry_url
        self.cache: Dict[str, str] = {}
        self.cache_ttl = 30  # секунды
        self.last_update: Dict[str, float] = {}

    async def get_service(self, service_name: str) -> Optional[str]:
        """Получить адрес сервиса."""
        # Проверяем кэш
        if service_name in self.cache:
            if time.time() - self.last_update[service_name] < self.cache_ttl:
                return self.cache[service_name]

        # Запрашиваем из registry
        try:
            async with aiohttp.ClientSession() as session:
                async with session.get(
                    f"{self.registry_url}/v1/catalog/service/{service_name}"
                ) as resp:
                    if resp.status == 200:
                        data = await resp.json()
                        if data:
                            # Берем первый здоровый инстанс
                            service_url = f"http://{data[0]['ServiceAddress']}:{data[0]['ServicePort']}"
                            self.cache[service_name] = service_url
                            self.last_update[service_name] = time.time()
                            return service_url
        except Exception as e:
            print(f"Service discovery failed: {e}")
            # Fallback на кэш
            return self.cache.get(service_name)

        return None
```

#### 2. Server-side Discovery

**Load balancer запрашивает registry:**
```
Client → Load Balancer → Service Registry → Load Balancer → Service
```

#### 3. Service Registry Pattern

**Пример с Consul:**

```python
# Регистрация сервиса
import consul

c = consul.Consul()

# Регистрация при старте
def register_service(service_name: str, address: str, port: int):
    c.agent.service.register(
        service_name,
        address=address,
        port=port,
        check=consul.Check.http(f'http://{address}:{port}/health', interval="10s")
    )

# Поиск сервиса
def discover_service(service_name: str):
    services = c.health.service(service_name, passing=True)
    if services:
        service = services[1][0]  # Первый здоровый
        return f"http://{service['Service']['Address']}:{service['Service']['Port']}"
    return None
```

### DNS-based Discovery

**Простой подход через DNS:**
```python
# Использование DNS имен сервисов
auth_service = "http://auth-service:8001"  # Docker Compose создает DNS
```

**Docker Compose автоматически создает DNS:**
```yaml
services:
  gateway:
    # Может обращаться к auth-service по имени
    environment:
      - AUTH_SERVICE=http://auth-service:8001

  auth-service:
    # Доступен как auth-service в сети
```

## Load Balancing

### Client-side Load Balancing

```python
class LoadBalancer:
    """Простой load balancer."""

    def __init__(self):
        self.services: Dict[str, List[str]] = {}
        self.current: Dict[str, int] = {}

    def add_service(self, service_name: str, instances: List[str]):
        """Добавить инстансы сервиса."""
        self.services[service_name] = instances
        self.current[service_name] = 0

    def get_next(self, service_name: str) -> Optional[str]:
        """Получить следующий инстанс (round-robin)."""
        if service_name not in self.services:
            return None

        instances = self.services[service_name]
        if not instances:
            return None

        instance = instances[self.current[service_name]]
        self.current[service_name] = (self.current[service_name] + 1) % len(instances)
        return instance
```

### Использование в Gateway

```python
# В API Gateway
lb = LoadBalancer()
lb.add_service('auth', [
    'http://auth-service-1:8001',
    'http://auth-service-2:8001',
    'http://auth-service-3:8001'
])

async def proxy_to_auth(request):
    service_url = lb.get_next('auth')
    # Проксируем на service_url
```

## Circuit Breaker Pattern

### Защита от каскадных сбоев

```python
from enum import Enum
from datetime import datetime, timedelta

class CircuitState(Enum):
    CLOSED = "closed"      # Работает нормально
    OPEN = "open"          # Не работает, не пытаемся
    HALF_OPEN = "half_open"  # Проверяем восстановление

class CircuitBreaker:
    """Circuit Breaker для защиты сервисов."""

    def __init__(self, failure_threshold: int = 5, timeout: int = 60):
        self.failure_threshold = failure_threshold
        self.timeout = timeout
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
        except Exception as e:
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

# Использование в Gateway
circuit_breaker = CircuitBreaker(failure_threshold=5, timeout=60)

async def proxy_with_circuit_breaker(request, service_name):
    """Проксирование с circuit breaker."""
    try:
        return await circuit_breaker.call(
            proxy_request,
            request,
            service_name
        )
    except Exception as e:
        # Fallback ответ
        return web.json_response({
            "error": "Service temporarily unavailable",
            "service": service_name
        }, status=503)
```

## Timeouts и Retries

### Настройка таймаутов

```python
# aiohttp ClientSession с таймаутами
timeout = aiohttp.ClientTimeout(
    total=30,        # Общий таймаут запроса
    connect=5,       # Таймаут подключения
    sock_read=10     # Таймаут чтения
)

async with aiohttp.ClientSession(timeout=timeout) as session:
    async with session.get(url) as resp:
        ...
```

### Retry механизм

```python
import asyncio
from typing import Callable, Any

async def retry(
    func: Callable,
    max_retries: int = 3,
    delay: float = 1.0,
    backoff: float = 2.0,
    *args,
    **kwargs
) -> Any:
    """Retry механизм с exponential backoff."""
    last_exception = None

    for attempt in range(max_retries):
        try:
            return await func(*args, **kwargs)
        except Exception as e:
            last_exception = e
            if attempt < max_retries - 1:
                wait_time = delay * (backoff ** attempt)
                await asyncio.sleep(wait_time)
            else:
                raise last_exception

    raise last_exception
```

## Best Practices

### 1. Используйте connection pooling

```python
# Один session на всё приложение
http_session = aiohttp.ClientSession(
    connector=aiohttp.TCPConnector(limit=100),
    timeout=aiohttp.ClientTimeout(total=30)
)
```

### 2. Добавляйте circuit breakers

```python
# Защита от каскадных сбоев
circuit_breaker = CircuitBreaker()
```

### 3. Используйте таймауты

```python
# Всегда устанавливайте таймауты
timeout = aiohttp.ClientTimeout(total=30)
```

### 4. Логируйте межсервисные вызовы

```python
logger.info("calling_service", service="auth", endpoint="/users", duration=0.123)
```

### 5. Мониторьте метрики

```python
# Метрики для Prometheus
requests_total.labels(service="auth", status="success").inc()
request_duration.labels(service="auth").observe(duration)
```

## Дополнительные материалы

### Полезные ссылки
- [API Gateway Pattern](https://microservices.io/patterns/apigateway.html)
- [Service Discovery Pattern](https://microservices.io/patterns/service-registry.html)
- [gRPC Documentation](https://grpc.io/docs/)

### Инструменты
- [Consul](https://www.consul.io/) - Service Discovery
- [Kong](https://konghq.com/) - API Gateway
- [Traefik](https://traefik.io/) - Reverse proxy и load balancer

### Статьи
- [Microservices Communication](https://microservices.io/patterns/communication-style.html)
- [API Gateway Best Practices](https://www.nginx.com/blog/building-microservices-using-an-api-gateway/)

## Вопросы для самопроверки

1. В чем разница между синхронной и асинхронной коммуникацией?
2. Когда использовать HTTP/REST, а когда gRPC?
3. Какие функции выполняет API Gateway?
4. Зачем нужен Service Discovery?
5. Что такое Circuit Breaker и зачем он нужен?

## Следующая неделя

На [Неделе 22](../week-22/README.md) изучим Event-driven архитектуру: RabbitMQ, message queues и асинхронную коммуникацию! 🚀

---

**Удачи с межсервисной коммуникацией! 🔗**

