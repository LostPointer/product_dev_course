# Неделя 24: Resilience Patterns для микросервисов

## Цели недели
- Понять важность устойчивости микросервисов
- Изучить Circuit Breaker pattern
- Освоить retry механизмы
- Научиться настраивать timeouts
- Понять Bulkhead pattern
- Реализовать устойчивые интеграции

## Теоретическая часть

### Что такое Resilience?

**Resilience (Устойчивость)** - способность системы восстанавливаться после сбоев и продолжать работать.

**В микросервисах:**
- Сервисы могут падать
- Сеть может быть нестабильной
- Нагрузка может быть высокой
- Зависимости могут быть медленными

**Цель:** Изолировать сбои и предотвратить каскадные отказы.

### Проблемы без Resilience

```python
# ❌ ПРОБЛЕМА: Падение зависимого сервиса ломает наш
async def get_experiment_with_metrics(experiment_id):
    # Если metrics-service упал, вся функция падает
    experiment = await experiment_service.get(experiment_id)
    metrics = await metrics_service.get_metrics(experiment_id)  # Может упасть!
    return {"experiment": experiment, "metrics": metrics}
```

**Каскадный отказ:**
```
Metrics Service падает
  → Experiment Service ждет таймаут (30s)
    → User Service ждет ответ (30s)
      → API Gateway таймаут (30s)
        → Клиент получает ошибку через 90 секунд!
```

## Resilience Patterns

### 1. Circuit Breaker

#### Концепция

**Circuit Breaker (Автоматический выключатель)** - предотвращает вызовы неработающего сервиса.

**Состояния:**
- **Closed** - нормальная работа, вызовы проходят
- **Open** - сервис недоступен, вызовы блокируются
- **Half-Open** - тестовый режим, пробуем вызвать

```
Closed → (ошибки > threshold) → Open → (timeout) → Half-Open
  ↑                                                      ↓
  └──────────────────────────────────────────────────────┘
              (успешные вызовы)
```

#### Реализация Circuit Breaker

```python
# shared/circuit_breaker.py
from enum import Enum
import asyncio
from datetime import datetime, timedelta
from typing import Callable, Any
import time

class CircuitState(Enum):
    CLOSED = "closed"
    OPEN = "open"
    HALF_OPEN = "half_open"

class CircuitBreaker:
    """Circuit Breaker для защиты от падений сервисов."""

    def __init__(
        self,
        failure_threshold: int = 5,
        success_threshold: int = 2,
        timeout: float = 60.0,
        expected_exception: type = Exception
    ):
        self.failure_threshold = failure_threshold
        self.success_threshold = success_threshold
        self.timeout = timeout

        self.state = CircuitState.CLOSED
        self.failure_count = 0
        self.success_count = 0
        self.last_failure_time = None
        self.expected_exception = expected_exception

    async def call(self, func: Callable, *args, **kwargs) -> Any:
        """Вызов функции через Circuit Breaker."""
        if self.state == CircuitState.OPEN:
            # Проверяем, можно ли перейти в Half-Open
            if self._should_attempt_reset():
                self.state = CircuitState.HALF_OPEN
                self.success_count = 0
            else:
                raise CircuitBreakerOpenError(
                    f"Circuit breaker is OPEN. "
                    f"Last failure: {self.last_failure_time}"
                )

        # Вызываем функцию
        try:
            result = await func(*args, **kwargs)
            self._on_success()
            return result

        except self.expected_exception as e:
            self._on_failure()
            raise

    def _on_success(self):
        """Обработка успешного вызова."""
        if self.state == CircuitState.HALF_OPEN:
            self.success_count += 1
            if self.success_count >= self.success_threshold:
                self.state = CircuitState.CLOSED
                self.failure_count = 0

        elif self.state == CircuitState.CLOSED:
            self.failure_count = 0

    def _on_failure(self):
        """Обработка неудачного вызова."""
        self.failure_count += 1
        self.last_failure_time = datetime.now()

        if self.failure_count >= self.failure_threshold:
            self.state = CircuitState.OPEN

    def _should_attempt_reset(self) -> bool:
        """Проверка, можно ли попробовать reset."""
        if not self.last_failure_time:
            return True

        elapsed = (datetime.now() - self.last_failure_time).total_seconds()
        return elapsed >= self.timeout

    def reset(self):
        """Ручной reset Circuit Breaker."""
        self.state = CircuitState.CLOSED
        self.failure_count = 0
        self.success_count = 0
        self.last_failure_time = None

class CircuitBreakerOpenError(Exception):
    """Ошибка, когда Circuit Breaker открыт."""
    pass
```

#### Использование Circuit Breaker

```python
# experiment-service/client_wrapper.py
from shared.circuit_breaker import CircuitBreaker
import httpx

# Создаем Circuit Breaker для metrics-service
metrics_circuit_breaker = CircuitBreaker(
    failure_threshold=5,
    success_threshold=2,
    timeout=60.0
)

async def get_metrics_with_circuit_breaker(experiment_id: int):
    """Получение метрик с Circuit Breaker."""
    async def _call_metrics_service():
        async with httpx.AsyncClient(timeout=5.0) as client:
            response = await client.get(
                f"http://metrics-service:8002/metrics/{experiment_id}"
            )
            response.raise_for_status()
            return response.json()

    try:
        return await metrics_circuit_breaker.call(_call_metrics_service)

    except CircuitBreakerOpenError:
        # Circuit Breaker открыт - возвращаем дефолтные значения
        return {"metrics": [], "status": "unavailable"}

    except httpx.HTTPError:
        raise  # Пробрасываем другие ошибки

# Использование
async def get_experiment_handler(request: web.Request):
    """Обработчик получения эксперимента."""
    experiment_id = int(request.match_info['id'])

    experiment = await experiment_service.get(experiment_id)

    # Метрики получаем через Circuit Breaker
    try:
        metrics = await get_metrics_with_circuit_breaker(experiment_id)
    except Exception as e:
        metrics = {"metrics": [], "status": "error"}

    return web.json_response({
        "experiment": experiment,
        "metrics": metrics
    })
```

### 2. Retry Mechanism

#### Концепция

**Retry** - повторная попытка при временных ошибках.

**Стратегии:**
- **Fixed** - фиксированная задержка
- **Exponential Backoff** - экспоненциальная задержка
- **Jitter** - случайная вариация задержки

#### Реализация Retry

```python
# shared/retry.py
import asyncio
import random
from typing import Callable, Type, Tuple, List
from functools import wraps

class RetryStrategy:
    """Стратегия повторных попыток."""

    @staticmethod
    def fixed(delay: float = 1.0):
        """Фиксированная задержка."""
        def wait(retry_count: int):
            return delay
        return wait

    @staticmethod
    def exponential(base: float = 1.0, max_delay: float = 60.0):
        """Экспоненциальная задержка."""
        def wait(retry_count: int):
            delay = min(base * (2 ** retry_count), max_delay)
            return delay
        return wait

    @staticmethod
    def exponential_with_jitter(
        base: float = 1.0,
        max_delay: float = 60.0,
        jitter: float = 0.1
    ):
        """Экспоненциальная задержка с jitter."""
        def wait(retry_count: int):
            delay = min(base * (2 ** retry_count), max_delay)
            jitter_amount = delay * jitter * random.uniform(-1, 1)
            return max(0.1, delay + jitter_amount)
        return wait

def retry(
    max_attempts: int = 3,
    retry_on: Tuple[Type[Exception], ...] = (Exception,),
    strategy: Callable = None,
    on_retry: Callable = None
):
    """Декоратор для retry логики."""
    if strategy is None:
        strategy = RetryStrategy.exponential_with_jitter()

    def decorator(func: Callable):
        @wraps(func)
        async def wrapper(*args, **kwargs):
            last_exception = None

            for attempt in range(1, max_attempts + 1):
                try:
                    return await func(*args, **kwargs)

                except retry_on as e:
                    last_exception = e

                    if attempt == max_attempts:
                        # Последняя попытка провалилась
                        raise

                    # Вычисляем задержку
                    delay = strategy(attempt)

                    # Callback перед retry
                    if on_retry:
                        await on_retry(attempt, delay, e)

                    # Ждем перед следующей попыткой
                    await asyncio.sleep(delay)

            # Не должно доходить сюда, но на всякий случай
            raise last_exception

        return wrapper
    return decorator
```

#### Использование Retry

```python
# experiment-service/client_wrapper.py
from shared.retry import retry, RetryStrategy
import httpx

@retry(
    max_attempts=3,
    retry_on=(httpx.NetworkError, httpx.TimeoutException, httpx.HTTPStatusError),
    strategy=RetryStrategy.exponential_with_jitter(base=0.5, max_delay=10.0),
    on_retry=lambda attempt, delay, exc: print(
        f"Retry {attempt} after {delay:.2f}s: {exc}"
    )
)
async def call_resource_service(method: str, endpoint: str, **kwargs):
    """Вызов resource-service с retry."""
    async with httpx.AsyncClient(timeout=5.0) as client:
        response = await client.request(
            method,
            f"http://resource-service:8001{endpoint}",
            **kwargs
        )
        response.raise_for_status()
        return response.json()

# Использование
async def allocate_resources(experiment_id: int, resources: dict):
    """Резервирование ресурсов с retry."""
    return await call_resource_service(
        "POST",
        "/allocations",
        json={"experiment_id": experiment_id, "resources": resources}
    )
```

### 3. Timeouts

#### Концепция

**Timeout** - ограничение времени на выполнение операции.

**Зачем:**
- Предотвращение бесконечного ожидания
- Быстрый fallback при проблемах
- Защита от медленных зависимостей

#### Реализация Timeout

```python
# shared/timeout.py
import asyncio
from typing import Any, Callable

class TimeoutError(Exception):
    """Ошибка таймаута."""
    pass

async def with_timeout(
    func: Callable,
    timeout: float,
    *args,
    **kwargs
) -> Any:
    """Выполнение функции с таймаутом."""
    try:
        return await asyncio.wait_for(
            func(*args, **kwargs),
            timeout=timeout
        )
    except asyncio.TimeoutError:
        raise TimeoutError(f"Operation timed out after {timeout}s")

# Использование
async def get_experiment_with_timeout(experiment_id: int):
    """Получение эксперимента с таймаутом."""
    try:
        return await with_timeout(
            experiment_service.get,
            timeout=5.0,
            experiment_id=experiment_id
        )
    except TimeoutError:
        return None  # или raise HTTP 504 Gateway Timeout
```

#### Timeout в HTTP клиентах

```python
# experiment-service/client_wrapper.py
import httpx

async def call_service_with_timeout(
    service_url: str,
    method: str,
    endpoint: str,
    timeout: float = 5.0,
    **kwargs
):
    """Вызов сервиса с таймаутом."""
    timeout_config = httpx.Timeout(
        connect=2.0,      # Таймаут подключения
        read=timeout,     # Таймаут чтения
        write=2.0,        # Таймаут записи
        pool=5.0          # Таймаут получения соединения из pool
    )

    async with httpx.AsyncClient(timeout=timeout_config) as client:
        try:
            response = await client.request(
                method,
                f"{service_url}{endpoint}",
                **kwargs
            )
            response.raise_for_status()
            return response.json()

        except httpx.TimeoutException:
            raise TimeoutError(f"Service {service_url} timed out")
```

### 4. Bulkhead Pattern

#### Концепция

**Bulkhead (Переборка)** - изоляция ресурсов для предотвращения каскадных отказов.

**Идея:** Разделить ресурсы на изолированные пулы.

```
Пул 1: Важные запросы → Выделенные ресурсы
Пул 2: Менее важные → Отдельные ресурсы
Пул 3: Фоновые задачи → Свои ресурсы
```

#### Реализация Bulkhead

```python
# shared/bulkhead.py
import asyncio
from typing import Callable, Any
from collections import defaultdict

class Bulkhead:
    """Bulkhead для изоляции ресурсов."""

    def __init__(self, max_concurrent: int = 10, name: str = "default"):
        self.semaphore = asyncio.Semaphore(max_concurrent)
        self.name = name
        self.active_count = 0

    async def execute(self, func: Callable, *args, **kwargs) -> Any:
        """Выполнение функции через Bulkhead."""
        async with self.semaphore:
            self.active_count += 1
            try:
                return await func(*args, **kwargs)
            finally:
                self.active_count -= 1

class BulkheadPool:
    """Пул Bulkheads для разных типов операций."""

    def __init__(self):
        self.bulkheads = {}

    def get_bulkhead(self, name: str, max_concurrent: int = 10) -> Bulkhead:
        """Получение или создание Bulkhead."""
        if name not in self.bulkheads:
            self.bulkheads[name] = Bulkhead(max_concurrent, name)
        return self.bulkheads[name]
```

#### Использование Bulkhead

```python
# experiment-service/client_wrapper.py
from shared.bulkhead import BulkheadPool

# Создаем пул Bulkheads
bulkhead_pool = BulkheadPool()

# Разные Bulkheads для разных операций
critical_bulkhead = bulkhead_pool.get_bulkhead("critical", max_concurrent=20)
normal_bulkhead = bulkhead_pool.get_bulkhead("normal", max_concurrent=10)
background_bulkhead = bulkhead_pool.get_bulkhead("background", max_concurrent=5)

async def get_experiment_critical(experiment_id: int):
    """Критичная операция - больше ресурсов."""
    async def _fetch():
        return await experiment_service.get(experiment_id)

    return await critical_bulkhead.execute(_fetch)

async def get_experiment_metrics(experiment_id: int):
    """Обычная операция - стандартные ресурсы."""
    async def _fetch():
        return await metrics_service.get_metrics(experiment_id)

    return await normal_bulkhead.execute(_fetch)

async def generate_report_background(project_id: int):
    """Фоновая задача - ограниченные ресурсы."""
    async def _generate():
        return await report_service.generate(project_id)

    return await background_bulkhead.execute(_generate)
```

## Комбинирование паттернов

### Полная защита сервиса

```python
# experiment-service/resilient_client.py
from shared.circuit_breaker import CircuitBreaker
from shared.retry import retry, RetryStrategy
from shared.bulkhead import BulkheadPool
from shared.timeout import with_timeout
import httpx

class ResilientServiceClient:
    """Клиент сервиса с полной защитой."""

    def __init__(
        self,
        service_url: str,
        service_name: str,
        timeout: float = 5.0
    ):
        self.service_url = service_url
        self.service_name = service_name
        self.timeout = timeout

        # Circuit Breaker
        self.circuit_breaker = CircuitBreaker(
            failure_threshold=5,
            success_threshold=2,
            timeout=60.0
        )

        # Bulkhead
        self.bulkhead_pool = BulkheadPool()
        self.bulkhead = self.bulkhead_pool.get_bulkhead(
            service_name,
            max_concurrent=20
        )

    async def call(
        self,
        method: str,
        endpoint: str,
        **kwargs
    ):
        """Вызов сервиса с полной защитой."""
        async def _make_request():
            return await with_timeout(
                self._request,
                self.timeout,
                method,
                endpoint,
                **kwargs
            )

        async def _request_with_retry():
            @retry(
                max_attempts=3,
                retry_on=(httpx.NetworkError, httpx.TimeoutException),
                strategy=RetryStrategy.exponential_with_jitter()
            )
            async def _retry_request():
                return await _make_request()

            return await _retry_request()

        async def _request_with_circuit_breaker():
            return await self.circuit_breaker.call(_request_with_retry)

        return await self.bulkhead.execute(_request_with_circuit_breaker)

    async def _request(self, method: str, endpoint: str, **kwargs):
        """Базовый HTTP запрос."""
        async with httpx.AsyncClient(
            timeout=httpx.Timeout(connect=2.0, read=self.timeout)
        ) as client:
            response = await client.request(
                method,
                f"{self.service_url}{endpoint}",
                **kwargs
            )
            response.raise_for_status()
            return response.json()

# Использование
metrics_client = ResilientServiceClient(
    service_url="http://metrics-service:8002",
    service_name="metrics",
    timeout=5.0
)

async def get_experiment_metrics(experiment_id: int):
    """Получение метрик с полной защитой."""
    return await metrics_client.call(
        "GET",
        f"/metrics/{experiment_id}"
    )
```

## Практический пример: Устойчивый API

### Обработчик с Resilience

```python
# experiment-service/handlers/experiments.py
from aiohttp import web
from experiment_service.resilient_client import ResilientServiceClient

# Клиенты сервисов с защитой
metrics_client = ResilientServiceClient(
    "http://metrics-service:8002",
    "metrics",
    timeout=3.0
)

resource_client = ResilientServiceClient(
    "http://resource-service:8001",
    "resources",
    timeout=5.0
)

async def get_experiment_handler(request: web.Request):
    """Получение эксперимента с метриками."""
    experiment_id = int(request.match_info['id'])

    # Получаем эксперимент (локальная БД - быстрая операция)
    experiment = await experiment_service.get(experiment_id)
    if not experiment:
        raise web.HTTPNotFound()

    # Получаем метрики с защитой
    metrics = None
    try:
        metrics = await metrics_client.call(
            "GET",
            f"/metrics/{experiment_id}"
        )
    except Exception as e:
        # Логируем, но не падаем
        logger.warning(f"Failed to get metrics: {e}")
        metrics = {"status": "unavailable"}

    # Получаем ресурсы с защитой
    resources = None
    try:
        resources = await resource_client.call(
            "GET",
            f"/allocations?experiment_id={experiment_id}"
        )
    except Exception as e:
        logger.warning(f"Failed to get resources: {e}")
        resources = []

    return web.json_response({
        "experiment": experiment,
        "metrics": metrics,
        "resources": resources
    })
```

## Best Practices

### 1. Настройка параметров

```python
# Конфигурация для разных типов операций
RESILIENCE_CONFIG = {
    "critical": {
        "circuit_breaker": {
            "failure_threshold": 3,
            "timeout": 30.0
        },
        "retry": {
            "max_attempts": 5,
            "strategy": "exponential"
        },
        "timeout": 10.0,
        "bulkhead": {"max_concurrent": 50}
    },
    "normal": {
        "circuit_breaker": {
            "failure_threshold": 5,
            "timeout": 60.0
        },
        "retry": {
            "max_attempts": 3,
            "strategy": "exponential_with_jitter"
        },
        "timeout": 5.0,
        "bulkhead": {"max_concurrent": 20}
    }
}
```

### 2. Мониторинг

```python
# Метрики для Circuit Breaker
circuit_breaker_state_gauge = Gauge(
    'circuit_breaker_state',
    'Circuit breaker state',
    ['service']
)

circuit_breaker_failures_total = Counter(
    'circuit_breaker_failures_total',
    'Total circuit breaker failures',
    ['service']
)

# Обновление метрик
def update_circuit_breaker_metrics(service_name: str, state: str):
    circuit_breaker_state_gauge.labels(service=service_name).set(
        1 if state == "open" else 0
    )
```

### 3. Логирование

```python
import structlog

logger = structlog.get_logger()

async def call_with_logging(func, *args, **kwargs):
    """Вызов с логированием."""
    logger.info("service_call_start", function=func.__name__)

    try:
        result = await func(*args, **kwargs)
        logger.info("service_call_success", function=func.__name__)
        return result

    except Exception as e:
        logger.error(
            "service_call_failed",
            function=func.__name__,
            error=str(e)
        )
        raise
```

### 4. Fallback значения

```python
async def get_experiment_with_fallback(experiment_id: int):
    """Получение с fallback."""
    try:
        return await metrics_client.call("GET", f"/metrics/{experiment_id}")
    except Exception:
        # Fallback на кэш
        cached = await cache.get(f"metrics:{experiment_id}")
        if cached:
            return cached

        # Fallback на дефолтные значения
        return {"metrics": [], "status": "default"}
```

## Дополнительные материалы

### Полезные ссылки
- [Circuit Breaker Pattern](https://martinfowler.com/bliki/CircuitBreaker.html)
- [Retry Pattern](https://docs.microsoft.com/en-us/azure/architecture/patterns/retry)
- [Bulkhead Pattern](https://docs.microsoft.com/en-us/azure/architecture/patterns/bulkhead)

### Библиотеки
- [tenacity](https://tenacity.readthedocs.io/) - Retry библиотека для Python
- [circuitbreaker](https://pypi.org/project/circuitbreaker/) - Circuit Breaker для Python
- [aiohttp](https://docs.aiohttp.org/) - HTTP клиент с поддержкой timeout

### Статьи
- [Resilience Patterns](https://microservices.io/patterns/reliability/)
- [Circuit Breaker Implementation](https://www.baeldung.com/resilience4j)

## Вопросы для самопроверки

1. В чем разница между состояниями Circuit Breaker?
2. Когда использовать retry, а когда Circuit Breaker?
3. Зачем нужен Bulkhead pattern?
4. Как выбрать параметры для timeout?
5. Как комбинировать несколько resilience паттернов?

## Следующая неделя

На [Неделе 25](../week-25/README.md) изучим Observability: distributed tracing, centralized logging и мониторинг! 🚀

---

**Удачи с resilience patterns! 🛡️**

