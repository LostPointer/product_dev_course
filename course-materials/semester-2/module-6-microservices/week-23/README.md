# Неделя 23: Saga Pattern для распределенных транзакций

## Цели недели
- Понять проблему распределенных транзакций в микросервисах
- Изучить Saga pattern и его типы
- Освоить компенсирующие транзакции
- Научиться реализовывать Saga (Choreography и Orchestration)
- Понять когда использовать Saga pattern

## Теоретическая часть

### Проблема распределенных транзакций

#### ACID в монолите

В монолитной архитектуре транзакции обеспечивают **ACID**:
- **Atomicity** - атомарность (все или ничего)
- **Consistency** - согласованность
- **Isolation** - изолированность
- **Durability** - долговечность

```python
# Монолит - все в одной БД
BEGIN TRANSACTION;
  INSERT INTO users ...;
  INSERT INTO orders ...;
  INSERT INTO payments ...;
COMMIT;
```

#### Проблема в микросервисах

В микросервисах каждый сервис имеет свою БД:

```
User Service (DB 1) → Order Service (DB 2) → Payment Service (DB 3)
```

**Проблема:** Нет глобальной транзакции!

**Пример:**
```python
# ❌ ПРОБЛЕМА: Что если payment упал после создания order?
async def create_order_with_payment(user_id, items, payment_info):
    # 1. Создаем заказ
    order = await order_service.create_order(user_id, items)

    # 2. Списание денег - может упасть!
    payment = await payment_service.charge(payment_info)

    # Если payment упал, order уже создан - несогласованность!
```

**Что если:**
1. Order создан, но Payment упал → заказ без оплаты
2. Payment прошел, но Order упал → оплата без заказа
3. Оба созданы, но Inventory упал → заказ без товара

### 2PC (Two-Phase Commit) - почему не работает

**2PC** пытается решить проблему:
- **Phase 1:** Prepare - все сервисы готовы
- **Phase 2:** Commit/Rollback - все подтверждают или откатывают

**Проблемы 2PC:**
- ❌ Блокирующие блокировки (производительность)
- ❌ Coordinator - single point of failure
- ❌ Не подходит для long-running транзакций
- ❌ Не масштабируется

**Вывод:** 2PC не подходит для микросервисов.

## Saga Pattern

### Что такое Saga?

**Saga** - последовательность локальных транзакций, где каждая имеет компенсирующую транзакцию.

**Принцип:**
- Каждый шаг Saga - локальная транзакция в своем сервисе
- Если шаг провалился, выполняем компенсирующие транзакции для предыдущих шагов
- Вместо отката - компенсация

**Ключевая идея:** Вместо `ROLLBACK` используем компенсирующие операции!

### Типы Saga

#### 1. Choreography Saga (Хореография)

**Принцип:** Каждый сервис знает, что делать дальше.

```
Step 1: Create Order → Event: order.created
Step 2: Reserve Inventory (слушает order.created) → Event: inventory.reserved
Step 3: Charge Payment (слушает inventory.reserved) → Event: payment.processed
Step 4: Complete Order (слушает payment.processed)
```

**Преимущества:**
- ✅ Нет центрального координатора
- ✅ Слабая связанность
- ✅ Простота добавления новых шагов

**Недостатки:**
- ❌ Сложно отследить состояние Saga
- ❌ Сложная отладка
- ❌ Могут быть циклы событий

#### 2. Orchestration Saga (Оркестрация)

**Принцип:** Центральный оркестратор управляет шагами.

```
Orchestrator → Step 1: Create Order
             → Step 2: Reserve Inventory
             → Step 3: Charge Payment
             → Step 4: Complete Order
```

**Преимущества:**
- ✅ Централизованное управление
- ✅ Легко отслеживать состояние
- ✅ Проще отладка
- ✅ Нет циклов событий

**Недостатки:**
- ❌ Single point of failure (оркестратор)
- ❌ Дополнительный сервис

## Choreography Saga (Пример)

### Сценарий: Создание эксперимента с ресурсами

**Шаги:**
1. Create Experiment (Experiment Service)
2. Allocate Resources (Resource Service)
3. Initialize Metrics (Metrics Service)
4. Notify User (Notification Service)

**Если шаг провалился:**
- Шаг 3 упал → компенсировать шаги 2, 1

### Реализация Choreography

**Event Bus для событий:**
```python
# shared/event_bus.py (из недели 22)
from shared.event_bus import EventBus

event_bus = EventBus("amqp://admin:admin@rabbitmq:5672/")
```

**Step 1: Create Experiment**
```python
# experiment-service/handlers/experiments.py
async def create_experiment_handler(request: web.Request):
    """Создание эксперимента."""
    data = await request.json()

    try:
        # Создаем эксперимент
        experiment = await experiment_service.create(data)

        # Публикуем событие успеха
        await event_bus.publish("experiment.created", {
            "experiment_id": experiment['id'],
            "user_id": experiment['user_id'],
            "resources_needed": experiment['resources'],
            "saga_id": experiment['id']  # ID для отслеживания Saga
        })

        return web.json_response(experiment, status=201)

    except Exception as e:
        # Компенсация не нужна - ничего не создано
        return web.json_response({"error": str(e)}, status=400)
```

**Step 2: Allocate Resources (Subscriber)**
```python
# resource-service/event_handlers.py
async def handle_experiment_created(event_type: str, event_data: dict):
    """Резервирование ресурсов для эксперимента."""
    experiment_id = event_data['experiment_id']
    resources = event_data['resources_needed']

    try:
        # Резервируем ресурсы
        allocation = await resource_service.allocate(
            experiment_id=experiment_id,
            resources=resources
        )

        # Публикуем событие успеха
        await event_bus.publish("resources.allocated", {
            "experiment_id": experiment_id,
            "allocation_id": allocation['id'],
            "saga_id": experiment_id
        })

    except Exception as e:
        # Компенсируем шаг 1
        await event_bus.publish("experiment.creation.failed", {
            "experiment_id": experiment_id,
            "reason": str(e),
            "saga_id": experiment_id
        })
```

**Step 3: Initialize Metrics (Subscriber)**
```python
# metrics-service/event_handlers.py
async def handle_resources_allocated(event_type: str, event_data: dict):
    """Инициализация метрик."""
    experiment_id = event_data['experiment_id']

    try:
        # Инициализируем метрики
        await metrics_service.initialize(experiment_id)

        # Публикуем событие успеха
        await event_bus.publish("metrics.initialized", {
            "experiment_id": experiment_id,
            "saga_id": experiment_id
        })

    except Exception as e:
        # Компенсируем шаги 2 и 1
        await event_bus.publish("experiment.creation.failed", {
            "experiment_id": experiment_id,
            "reason": str(e),
            "saga_id": experiment_id
        })
```

**Компенсирующие транзакции:**

```python
# experiment-service/event_handlers.py
async def handle_creation_failed(event_type: str, event_data: dict):
    """Компенсация создания эксперимента."""
    experiment_id = event_data['experiment_id']

    # Удаляем эксперимент (компенсация шага 1)
    await experiment_service.delete(experiment_id)

# resource-service/event_handlers.py
async def handle_creation_failed(event_type: str, event_data: dict):
    """Компенсация резервирования ресурсов."""
    experiment_id = event_data['experiment_id']

    # Освобождаем ресурсы (компенсация шага 2)
    await resource_service.release(experiment_id)
```

### Проблема Choreography

**Проблема:** Как отследить, что все шаги выполнены?

**Решение:** State Machine или таймауты.

```python
# experiment-service/saga_tracker.py
import asyncio
from datetime import datetime, timedelta

class SagaTracker:
    """Отслеживание состояния Saga."""

    def __init__(self):
        self.active_sagas = {}  # saga_id -> {state, steps, created_at}

    async def start_saga(self, saga_id: str, steps: list):
        """Начало Saga."""
        self.active_sagas[saga_id] = {
            "state": "started",
            "steps": {step: False for step in steps},
            "created_at": datetime.now()
        }

        # Таймаут для Saga
        asyncio.create_task(self._check_timeout(saga_id))

    async def complete_step(self, saga_id: str, step: str):
        """Завершение шага."""
        if saga_id in self.active_sagas:
            self.active_sagas[saga_id]["steps"][step] = True

            # Проверяем, все ли шаги выполнены
            if all(self.active_sagas[saga_id]["steps"].values()):
                self.active_sagas[saga_id]["state"] = "completed"
                await event_bus.publish("saga.completed", {"saga_id": saga_id})

    async def _check_timeout(self, saga_id: str, timeout_seconds=300):
        """Проверка таймаута Saga."""
        await asyncio.sleep(timeout_seconds)

        if saga_id in self.active_sagas:
            saga = self.active_sagas[saga_id]
            if saga["state"] != "completed":
                # Saga не завершилась - компенсация
                await event_bus.publish("saga.timeout", {
                    "saga_id": saga_id,
                    "reason": "timeout"
                })
```

## Orchestration Saga

### Центральный оркестратор

**Принцип:** Отдельный сервис управляет шагами Saga.

```
Saga Orchestrator:
  1. Create Experiment → wait for result
  2. If success → Allocate Resources → wait
  3. If success → Initialize Metrics → wait
  4. If success → Notify User → Complete
  5. If any failed → Compensate
```

### Реализация Orchestration Saga

**Saga Orchestrator Service:**
```python
# saga-orchestrator/saga_engine.py
from enum import Enum
from typing import Dict, List, Callable
import asyncio

class SagaStatus(Enum):
    PENDING = "pending"
    IN_PROGRESS = "in_progress"
    COMPLETED = "completed"
    FAILED = "failed"
    COMPENSATING = "compensating"

class SagaStep:
    """Шаг Saga."""
    def __init__(
        self,
        name: str,
        execute: Callable,
        compensate: Callable,
        service_name: str
    ):
        self.name = name
        self.execute = execute
        self.compensate = compensate
        self.service_name = service_name
        self.completed = False

class SagaOrchestrator:
    """Оркестратор Saga."""

    def __init__(self):
        self.active_sagas: Dict[str, Dict] = {}

    async def execute_saga(
        self,
        saga_id: str,
        steps: List[SagaStep],
        initial_data: dict
    ):
        """Выполнение Saga."""
        self.active_sagas[saga_id] = {
            "status": SagaStatus.IN_PROGRESS,
            "steps": steps,
            "data": initial_data,
            "completed_steps": []
        }

        executed_steps = []

        try:
            for step in steps:
                # Выполняем шаг
                result = await step.execute(initial_data)

                # Сохраняем результат
                initial_data.update(result)
                executed_steps.append(step)
                step.completed = True

                # Обновляем состояние
                self.active_sagas[saga_id]["completed_steps"].append(step.name)

            # Все шаги выполнены
            self.active_sagas[saga_id]["status"] = SagaStatus.COMPLETED
            return {"status": "completed", "data": initial_data}

        except Exception as e:
            # Компенсация
            await self._compensate(saga_id, executed_steps, initial_data)
            raise

    async def _compensate(
        self,
        saga_id: str,
        executed_steps: List[SagaStep],
        initial_data: dict
    ):
        """Компенсация выполненных шагов."""
        self.active_sagas[saga_id]["status"] = SagaStatus.COMPENSATING

        # Компенсируем в обратном порядке
        for step in reversed(executed_steps):
            try:
                await step.compensate(initial_data)
            except Exception as e:
                # Логируем ошибку компенсации
                print(f"Compensation failed for {step.name}: {e}")

        self.active_sagas[saga_id]["status"] = SagaStatus.FAILED

    def get_saga_status(self, saga_id: str) -> Dict:
        """Получение статуса Saga."""
        if saga_id not in self.active_sagas:
            return {"error": "Saga not found"}

        saga = self.active_sagas[saga_id]
        return {
            "saga_id": saga_id,
            "status": saga["status"].value,
            "completed_steps": saga["completed_steps"]
        }
```

### Использование Orchestrator

**Определение Saga:**
```python
# saga-orchestrator/sagas/create_experiment_saga.py
from saga_engine import SagaOrchestrator, SagaStep
import httpx

async def create_experiment_step(data: dict) -> dict:
    """Шаг 1: Создание эксперимента."""
    async with httpx.AsyncClient() as client:
        response = await client.post(
            "http://experiment-service:8000/experiments",
            json={
                "name": data["name"],
                "project_id": data["project_id"],
                "user_id": data["user_id"]
            }
        )
        response.raise_for_status()
        result = response.json()
        return {"experiment_id": result["id"]}

async def compensate_create_experiment(data: dict):
    """Компенсация: Удаление эксперимента."""
    experiment_id = data.get("experiment_id")
    if experiment_id:
        async with httpx.AsyncClient() as client:
            await client.delete(
                f"http://experiment-service:8000/experiments/{experiment_id}"
            )

async def allocate_resources_step(data: dict) -> dict:
    """Шаг 2: Резервирование ресурсов."""
    async with httpx.AsyncClient() as client:
        response = await client.post(
            "http://resource-service:8001/allocations",
            json={
                "experiment_id": data["experiment_id"],
                "resources": data.get("resources", {})
            }
        )
        response.raise_for_status()
        result = response.json()
        return {"allocation_id": result["id"]}

async def compensate_allocate_resources(data: dict):
    """Компенсация: Освобождение ресурсов."""
    allocation_id = data.get("allocation_id")
    if allocation_id:
        async with httpx.AsyncClient() as client:
            await client.delete(
                f"http://resource-service:8001/allocations/{allocation_id}"
            )

# Определяем Saga
def create_experiment_saga(orchestrator: SagaOrchestrator):
    """Создание Saga для создания эксперимента."""
    steps = [
        SagaStep(
            name="create_experiment",
            execute=create_experiment_step,
            compensate=compensate_create_experiment,
            service_name="experiment-service"
        ),
        SagaStep(
            name="allocate_resources",
            execute=allocate_resources_step,
            compensate=compensate_allocate_resources,
            service_name="resource-service"
        ),
        # ... другие шаги
    ]
    return steps
```

**API для запуска Saga:**
```python
# saga-orchestrator/handlers/saga_handler.py
from aiohttp import web
from saga_engine import SagaOrchestrator
from sagas.create_experiment_saga import create_experiment_saga

orchestrator = SagaOrchestrator()

async def start_create_experiment_saga(request: web.Request):
    """Запуск Saga создания эксперимента."""
    data = await request.json()
    saga_id = f"experiment_{data['user_id']}_{int(time.time())}"

    steps = create_experiment_saga(orchestrator)

    try:
        result = await orchestrator.execute_saga(
            saga_id=saga_id,
            steps=steps,
            initial_data=data
        )
        return web.json_response({
            "saga_id": saga_id,
            "status": "completed",
            "result": result
        })
    except Exception as e:
        status = orchestrator.get_saga_status(saga_id)
        return web.json_response({
            "saga_id": saga_id,
            "status": status["status"],
            "error": str(e)
        }, status=500)

async def get_saga_status_handler(request: web.Request):
    """Получение статуса Saga."""
    saga_id = request.match_info['saga_id']
    status = orchestrator.get_saga_status(saga_id)
    return web.json_response(status)
```

## Сравнение подходов

### Choreography vs Orchestration

| Критерий | Choreography | Orchestration |
|----------|-------------|---------------|
| Связанность | Слабая | Сильная (с оркестратором) |
| Сложность | Средняя | Низкая-Средняя |
| Отладка | Сложная | Легкая |
| Масштабируемость | Высокая | Средняя |
| Single Point of Failure | Нет | Да (оркестратор) |
| Когда использовать | Простые Saga | Сложные Saga |

**Рекомендация:**
- **Choreography** - для простых Saga с 2-3 шагами
- **Orchestration** - для сложных Saga с зависимостями

## Компенсирующие транзакции

### Принципы компенсации

**Цель:** Вернуть систему в состояние "до начала Saga".

**Типы компенсаций:**

1. **Простая отмена:**
   ```python
   # Создали ресурс → удалить
   await create_resource() → await delete_resource()
   ```

2. **Обратная операция:**
   ```python
   # Списали деньги → вернуть
   await charge_payment(100) → await refund_payment(100)
   ```

3. **Уведомление:**
   ```python
   # Отправили email → отправить отмену
   await send_email() → await send_cancellation_email()
   ```

### Идемпотентность компенсации

**Важно:** Компенсация должна быть идемпотентной!

```python
# ❌ ПЛОХО - не идемпотентно
async def compensate_delete_resource(resource_id):
    await resource_service.delete(resource_id)  # Может упасть, если уже удален

# ✅ ХОРОШО - идемпотентно
async def compensate_delete_resource(resource_id):
    try:
        resource = await resource_service.get(resource_id)
        if resource and resource['status'] != 'deleted':
            await resource_service.delete(resource_id)
    except NotFound:
        pass  # Уже удален - это нормально
```

## Практический пример: Создание Run

### Сценарий

При создании Run эксперимента нужно:
1. Создать Run (Experiment Service)
2. Резервировать ресурсы (Resource Service)
3. Инициализировать метрики (Metrics Service)
4. Запустить worker (Worker Service)

### Реализация (Orchestration)

```python
# saga-orchestrator/sagas/create_run_saga.py

async def create_run_step(data: dict) -> dict:
    """Шаг 1: Создание Run."""
    async with httpx.AsyncClient(timeout=30.0) as client:
        response = await client.post(
            f"http://experiment-service:8000/experiments/{data['experiment_id']}/runs",
            json={"config": data.get("config", {})}
        )
        response.raise_for_status()
        return {"run_id": response.json()["id"]}

async def compensate_create_run(data: dict):
    """Компенсация: Удаление Run."""
    run_id = data.get("run_id")
    if run_id:
        async with httpx.AsyncClient() as client:
            await client.delete(
                f"http://experiment-service:8000/runs/{run_id}"
            )

async def reserve_resources_step(data: dict) -> dict:
    """Шаг 2: Резервирование ресурсов."""
    async with httpx.AsyncClient(timeout=30.0) as client:
        response = await client.post(
            "http://resource-service:8001/reservations",
            json={
                "run_id": data["run_id"],
                "experiment_id": data["experiment_id"],
                "resources": data.get("resources", {})
            }
        )
        response.raise_for_status()
        result = response.json()
        return {"reservation_id": result["id"]}

async def compensate_reserve_resources(data: dict):
    """Компенсация: Освобождение ресурсов."""
    reservation_id = data.get("reservation_id")
    if reservation_id:
        async with httpx.AsyncClient() as client:
            await client.delete(
                f"http://resource-service:8001/reservations/{reservation_id}"
            )

async def initialize_metrics_step(data: dict) -> dict:
    """Шаг 3: Инициализация метрик."""
    async with httpx.AsyncClient(timeout=30.0) as client:
        response = await client.post(
            "http://metrics-service:8002/metrics/initialize",
            json={
                "run_id": data["run_id"],
                "experiment_id": data["experiment_id"]
            }
        )
        response.raise_for_status()
        return {}

async def compensate_initialize_metrics(data: dict):
    """Компенсация: Очистка метрик."""
    run_id = data.get("run_id")
    if run_id:
        async with httpx.AsyncClient() as client:
            await client.delete(
                f"http://metrics-service:8002/metrics/runs/{run_id}"
            )

async def start_worker_step(data: dict) -> dict:
    """Шаг 4: Запуск worker."""
    async with httpx.AsyncClient(timeout=30.0) as client:
        response = await client.post(
            "http://worker-service:8003/workers/start",
            json={
                "run_id": data["run_id"],
                "experiment_id": data["experiment_id"]
            }
        )
        response.raise_for_status()
        return {"worker_id": response.json()["id"]}

async def compensate_start_worker(data: dict):
    """Компенсация: Остановка worker."""
    worker_id = data.get("worker_id")
    if worker_id:
        async with httpx.AsyncClient() as client:
            await client.post(
                f"http://worker-service:8003/workers/{worker_id}/stop"
            )

def create_run_saga(orchestrator: SagaOrchestrator):
    """Создание Saga для запуска Run."""
    return [
        SagaStep(
            name="create_run",
            execute=create_run_step,
            compensate=compensate_create_run,
            service_name="experiment-service"
        ),
        SagaStep(
            name="reserve_resources",
            execute=reserve_resources_step,
            compensate=compensate_reserve_resources,
            service_name="resource-service"
        ),
        SagaStep(
            name="initialize_metrics",
            execute=initialize_metrics_step,
            compensate=compensate_initialize_metrics,
            service_name="metrics-service"
        ),
        SagaStep(
            name="start_worker",
            execute=start_worker_step,
            compensate=compensate_start_worker,
            service_name="worker-service"
        )
    ]
```

## Best Practices

### 1. Идемпотентность операций

Все операции должны быть идемпотентными.

```python
# Проверка перед выполнением
if await resource_exists(resource_id):
    return {"id": resource_id}  # Уже создан
else:
    return await create_resource(data)
```

### 2. Таймауты

Устанавливайте таймауты для всех вызовов.

```python
async with httpx.AsyncClient(timeout=30.0) as client:
    response = await client.post(...)
```

### 3. Retry логика

Добавляйте retry для временных ошибок.

```python
from tenacity import retry, stop_after_attempt, wait_exponential

@retry(
    stop=stop_after_attempt(3),
    wait=wait_exponential(multiplier=1, min=2, max=10)
)
async def create_run_step(data: dict) -> dict:
    # ...
```

### 4. Логирование

Логируйте все шаги Saga.

```python
logger.info("saga_step_started", saga_id=saga_id, step=step.name)
logger.info("saga_step_completed", saga_id=saga_id, step=step.name)
logger.error("saga_step_failed", saga_id=saga_id, step=step.name, error=str(e))
```

### 5. Мониторинг

Отслеживайте состояние Saga.

```python
# Prometheus метрики
saga_duration.observe(duration)
saga_success_total.inc()
saga_failure_total.inc()
```

## Дополнительные материалы

### Полезные ссылки
- [Saga Pattern](https://microservices.io/patterns/data/saga.html)
- [Choreography vs Orchestration](https://www.oreilly.com/library/view/microservices-patterns/9781617294549/)
- [Distributed Transactions](https://martinfowler.com/articles/patterns-of-distributed-systems/)

### Библиотеки
- [Temporal](https://temporal.io/) - Workflow engine для Saga
- [Cadence](https://cadenceworkflow.io/) - Альтернатива Temporal
- [Saga Python](https://github.com/lyft/python-saga) - Библиотека для Saga

### Статьи
- [Saga Pattern Explained](https://www.baeldung.com/cs/saga-pattern-microservices)
- [Compensating Transactions](https://www.infoq.com/articles/compensating-transactions-microservices/)

## Вопросы для самопроверки

1. В чем разница между 2PC и Saga pattern?
2. Когда использовать Choreography, а когда Orchestration?
3. Что такое компенсирующая транзакция?
4. Почему компенсации должны быть идемпотентными?
5. Какие проблемы решает Saga pattern в микросервисах?

## Следующая неделя

На [Неделе 24](../week-24/README.md) изучим Resilience patterns: Circuit Breaker, retries, timeouts и Bulkhead! 🚀

---

**Удачи с Saga pattern! 🔄**

