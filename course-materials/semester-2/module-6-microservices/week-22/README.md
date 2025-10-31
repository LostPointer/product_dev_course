# Неделя 22: Event-driven архитектура с RabbitMQ

## Цели недели
- Понять принципы event-driven архитектуры
- Изучить RabbitMQ и его возможности
- Освоить publish/subscribe паттерны
- Научиться проектировать асинхронную коммуникацию
- Реализовать event-driven интеграцию между сервисами

## Теоретическая часть

### Что такое Event-driven Architecture?

**Event-driven Architecture (EDA)** - архитектурный паттерн, где сервисы общаются через события.

**Событие (Event)** - факт того, что что-то произошло в системе.

**Характеристики:**
- Слабая связанность сервисов
- Асинхронная обработка
- Масштабируемость
- Отказоустойчивость

```
┌─────────────┐
│   Service A │
└──────┬──────┘
       │ Publish Event
       ▼
┌─────────────┐
│  Event Bus  │ (RabbitMQ)
│   / Queue   │
└──────┬──────┘
       │ Subscribe
   ┌───┴───┬──────────┐
   ▼       ▼          ▼
┌─────┐ ┌─────┐   ┌──────┐
│  B  │ │  C  │   │  D   │
└─────┘ └─────┘   └──────┘
```

### Преимущества Event-driven

- ✅ **Слабая связанность** - сервисы не знают друг о друге
- ✅ **Масштабируемость** - легко добавить новых подписчиков
- ✅ **Отказоустойчивость** - если сервис упал, события сохраняются
- ✅ **Гибкость** - легко изменять обработчики
- ✅ **Производительность** - неблокирующая обработка

### Недостатки

- ❌ Сложность отладки (асинхронность)
- ❌ Eventual consistency
- ❌ Дублирование событий (нужна идемпотентность)
- ❌ Порядок событий может быть потерян

## RabbitMQ основы

### Что такое RabbitMQ?

**RabbitMQ** - message broker, реализующий AMQP (Advanced Message Queuing Protocol).

**Основные концепции:**

1. **Producer** - отправляет сообщения
2. **Consumer** - получает сообщения
3. **Queue** - очередь сообщений
4. **Exchange** - маршрутизатор сообщений
5. **Binding** - связь между exchange и queue
6. **Routing Key** - ключ для маршрутизации

### Архитектура RabbitMQ

```
Producer → Exchange → Binding → Queue → Consumer
           (routing)
```

**Exchange типы:**
- **Direct** - точное совпадение routing key
- **Topic** - паттерн matching routing key
- **Fanout** - broadcast всем queues
- **Headers** - по headers (не routing key)

## Установка и настройка

### Docker Compose для RabbitMQ

```yaml
# docker-compose.yml
version: '3.8'

services:
  rabbitmq:
    image: rabbitmq:3-management-alpine
    ports:
      - "5672:5672"   # AMQP порт
      - "15672:15672" # Management UI
    environment:
      RABBITMQ_DEFAULT_USER: admin
      RABBITMQ_DEFAULT_PASS: admin
    volumes:
      - rabbitmq_data:/var/lib/rabbitmq
    healthcheck:
      test: ["CMD", "rabbitmq-diagnostics", "ping"]
      interval: 10s
      timeout: 5s
      retries: 5

volumes:
  rabbitmq_data:
```

**Management UI:** http://localhost:15672 (admin/admin)

### Установка Python клиента

```bash
pip install aio-pika
```

## Паттерны RabbitMQ

### 1. Work Queue (Producer-Consumer)

**Использование:** Распределение задач между воркерами.

```
Producer → Queue → Consumer 1
                   Consumer 2
                   Consumer 3
```

**Producer:**
```python
# producer.py
import aio_pika
import asyncio

async def publish_task(task_data: dict):
    """Отправка задачи в очередь."""
    connection = await aio_pika.connect_robust(
        "amqp://admin:admin@localhost/"
    )

    async with connection:
        channel = await connection.channel()

        # Объявляем очередь
        queue = await channel.declare_queue('tasks', durable=True)

        # Отправляем сообщение
        await channel.default_exchange.publish(
            aio_pika.Message(
                json.dumps(task_data).encode(),
                delivery_mode=aio_pika.DeliveryMode.PERSISTENT
            ),
            routing_key='tasks'
        )

        print(f"Task published: {task_data}")

# Использование
asyncio.run(publish_task({"type": "process_image", "file_id": 123}))
```

**Consumer:**
```python
# consumer.py
import aio_pika
import json
import asyncio

async def process_task(message: aio_pika.IncomingMessage):
    """Обработка задачи."""
    async with message.process():
        task_data = json.loads(message.body.decode())
        print(f"Processing task: {task_data}")

        # Обработка задачи
        # await process_image(task_data['file_id'])

        # Подтверждение обработки
        message.ack()

async def consume_tasks():
    """Потребление задач из очереди."""
    connection = await aio_pika.connect_robust(
        "amqp://admin:admin@localhost/"
    )

    async with connection:
        channel = await connection.channel()

        # Ограничиваем количество неподтвержденных сообщений
        await channel.set_qos(prefetch_count=10)

        # Объявляем очередь
        queue = await channel.declare_queue('tasks', durable=True)

        # Подписываемся на очередь
        await queue.consume(process_task)

        print("Waiting for tasks...")
        await asyncio.Future()  # Бесконечное ожидание

asyncio.run(consume_tasks())
```

### 2. Publish/Subscribe (Fanout Exchange)

**Использование:** Broadcast сообщений всем подписчикам.

```
Producer → Fanout Exchange → Queue 1 → Consumer 1
                          → Queue 2 → Consumer 2
                          → Queue 3 → Consumer 3
```

**Producer:**
```python
# publisher.py
async def publish_event(event: dict):
    """Публикация события."""
    connection = await aio_pika.connect_robust(
        "amqp://admin:admin@localhost/"
    )

    async with connection:
        channel = await connection.channel()

        # Создаем fanout exchange
        exchange = await channel.declare_exchange(
            'events',
            aio_pika.ExchangeType.FANOUT
        )

        # Публикуем событие
        await exchange.publish(
            aio_pika.Message(
                json.dumps(event).encode(),
                content_type='application/json'
            ),
            routing_key=''  # Для fanout не используется
        )
```

**Subscriber:**
```python
# subscriber.py
async def subscribe_to_events(service_name: str, handler):
    """Подписка на события."""
    connection = await aio_pika.connect_robust(
        "amqp://admin:admin@localhost/"
    )

    async with connection:
        channel = await connection.channel()

        # Объявляем exchange
        exchange = await channel.declare_exchange(
            'events',
            aio_pika.ExchangeType.FANOUT
        )

        # Создаем временную очередь для этого сервиса
        queue = await channel.declare_queue(
            '',  # RabbitMQ создаст уникальное имя
            exclusive=True  # Удалится при отключении
        )

        # Связываем очередь с exchange
        await queue.bind(exchange)

        # Подписываемся
        await queue.consume(handler)

        print(f"{service_name} subscribed to events")
        await asyncio.Future()
```

### 3. Routing (Direct Exchange)

**Использование:** Селективная доставка по routing key.

```
Producer → Direct Exchange → Queue (key: "user.created") → Consumer
                          → Queue (key: "order.created") → Consumer
```

**Producer:**
```python
async def publish_routed_event(event_type: str, event_data: dict):
    """Публикация события с routing key."""
    connection = await aio_pika.connect_robust(
        "amqp://admin:admin@localhost/"
    )

    async with connection:
        channel = await connection.channel()

        exchange = await channel.declare_exchange(
            'events',
            aio_pika.ExchangeType.DIRECT
        )

        await exchange.publish(
            aio_pika.Message(
                json.dumps(event_data).encode()
            ),
            routing_key=event_type  # "user.created", "order.created"
        )
```

**Consumer с routing:**
```python
async def subscribe_to_user_events(handler):
    """Подписка на события пользователей."""
    connection = await aio_pika.connect_robust(...)

    async with connection:
        channel = await connection.channel()

        exchange = await channel.declare_exchange(
            'events',
            aio_pika.ExchangeType.DIRECT
        )

        queue = await channel.declare_queue('user_events', durable=True)

        # Подписываемся только на события пользователей
        await queue.bind(exchange, routing_key='user.created')
        await queue.bind(exchange, routing_key='user.updated')
        await queue.bind(exchange, routing_key='user.deleted')

        await queue.consume(handler)
        await asyncio.Future()
```

### 4. Topics (Topic Exchange)

**Использование:** Паттерн-матчинг routing keys.

```
Producer → Topic Exchange → Queue (pattern: "user.*") → Consumer
                         → Queue (pattern: "*.created") → Consumer
```

**Wildcards:**
- `*` - одно слово
- `#` - ноль или более слов

**Примеры:**
- `user.created` - событие создания пользователя
- `user.updated` - событие обновления пользователя
- `order.created` - событие создания заказа
- `experiment.run.started` - запуск эксперимента

**Producer:**
```python
async def publish_topic_event(routing_key: str, event_data: dict):
    """Публикация с topic routing."""
    connection = await aio_pika.connect_robust(...)

    async with connection:
        channel = await connection.channel()

        exchange = await channel.declare_exchange(
            'events',
            aio_pika.ExchangeType.TOPIC
        )

        await exchange.publish(
            aio_pika.Message(json.dumps(event_data).encode()),
            routing_key=routing_key  # "user.created", "experiment.run.started"
        )
```

**Consumer с topics:**
```python
async def subscribe_to_topics(patterns: List[str], handler):
    """Подписка на события по паттернам."""
    connection = await aio_pika.connect_robust(...)

    async with connection:
        channel = await connection.channel()

        exchange = await channel.declare_exchange(
            'events',
            aio_pika.ExchangeType.TOPIC
        )

        queue = await channel.declare_queue('topic_events', durable=True)

        # Подписываемся на паттерны
        for pattern in patterns:
            await queue.bind(exchange, routing_key=pattern)

        await queue.consume(handler)
        await asyncio.Future()
```

## Event-driven интеграция сервисов

### Пример: Experiment Tracking Platform

**Сценарий:** При создании эксперимента нужно:
1. Сохранить в Experiment Service
2. Отправить уведомление (Notification Service)
3. Обновить статистику (Analytics Service)
4. Создать начальные метрики (Metrics Service)

**Без событий (синхронно):**
```python
# ❌ ПЛОХО - синхронные вызовы
async def create_experiment(data):
    # Создаем эксперимент
    experiment = await experiment_service.create(data)

    # Синхронные вызовы
    await notification_service.send_notification(...)  # Медленно!
    await analytics_service.update_stats(...)  # Блокируем!
    await metrics_service.init_metrics(...)  # Еще медленнее!

    return experiment
```

**С событиями (асинхронно):**
```python
# ✅ ХОРОШО - публикация события
async def create_experiment(data):
    # Создаем эксперимент
    experiment = await experiment_service.create(data)

    # Публикуем событие
    await event_bus.publish("experiment.created", {
        "experiment_id": experiment['id'],
        "user_id": experiment['user_id'],
        "project_id": experiment['project_id'],
        "created_at": experiment['created_at']
    })

    # Сразу возвращаем ответ
    return experiment
```

### Реализация Event Bus

```python
# shared/event_bus.py
import aio_pika
import json
from typing import Callable, Dict, List
import asyncio

class EventBus:
    """Event Bus для публикации и подписки на события."""

    def __init__(self, connection_url: str):
        self.connection_url = connection_url
        self.connection: aio_pika.Connection = None
        self.channel: aio_pika.Channel = None
        self.exchange: aio_pika.Exchange = None

    async def connect(self):
        """Подключение к RabbitMQ."""
        self.connection = await aio_pika.connect_robust(
            self.connection_url
        )
        self.channel = await self.connection.channel()

        # Создаем topic exchange для гибкой маршрутизации
        self.exchange = await self.channel.declare_exchange(
            'events',
            aio_pika.ExchangeType.TOPIC,
            durable=True
        )

    async def disconnect(self):
        """Отключение от RabbitMQ."""
        if self.connection:
            await self.connection.close()

    async def publish(self, event_type: str, event_data: dict):
        """Публикация события."""
        if not self.exchange:
            await self.connect()

        message = aio_pika.Message(
            json.dumps(event_data).encode(),
            content_type='application/json',
            delivery_mode=aio_pika.DeliveryMode.PERSISTENT,
            timestamp=datetime.now().timestamp()
        )

        await self.exchange.publish(
            message,
            routing_key=event_type
        )

    async def subscribe(
        self,
        event_patterns: List[str],
        queue_name: str,
        handler: Callable
    ):
        """Подписка на события."""
        if not self.exchange:
            await self.connect()

        # Создаем очередь
        queue = await self.channel.declare_queue(
            queue_name,
            durable=True
        )

        # Подписываемся на паттерны
        for pattern in event_patterns:
            await queue.bind(self.exchange, routing_key=pattern)

        # Обработчик сообщений
        async def message_handler(message: aio_pika.IncomingMessage):
            async with message.process():
                try:
                    event_data = json.loads(message.body.decode())
                    event_type = message.routing_key

                    await handler(event_type, event_data)

                except Exception as e:
                    print(f"Error processing event: {e}")
                    # Можно отправить в dead letter queue
                    message.nack(requeue=False)

        await queue.consume(message_handler)
```

### Использование в сервисах

**Experiment Service (Publisher):**
```python
# experiment-service/handlers/experiments.py
from shared.event_bus import EventBus

event_bus = EventBus("amqp://admin:admin@rabbitmq:5672/")

async def create_experiment_handler(request: web.Request):
    """Создание эксперимента."""
    data = await request.json()

    # Создаем эксперимент
    experiment = await create_experiment_in_db(data)

    # Публикуем событие
    await event_bus.publish("experiment.created", {
        "experiment_id": experiment['id'],
        "user_id": experiment['user_id'],
        "project_id": experiment['project_id'],
        "name": experiment['name'],
        "created_at": experiment['created_at'].isoformat()
    })

    return web.json_response(experiment, status=201)
```

**Notification Service (Subscriber):**
```python
# notification-service/event_handlers.py
from shared.event_bus import EventBus

event_bus = EventBus("amqp://admin:admin@rabbitmq:5672/")

async def handle_experiment_created(event_type: str, event_data: dict):
    """Обработка события создания эксперимента."""
    experiment_id = event_data['experiment_id']
    user_id = event_data['user_id']

    # Отправляем уведомление
    await send_notification(
        user_id=user_id,
        message=f"Experiment {experiment_id} created"
    )

# Подписка при старте сервиса
async def start_event_consumers(app: web.Application):
    """Запуск подписчиков на события."""
    await event_bus.connect()

    await event_bus.subscribe(
        event_patterns=['experiment.created', 'experiment.completed'],
        queue_name='notification_service',
        handler=handle_experiment_created
    )

    app['event_bus'] = event_bus

async def stop_event_consumers(app: web.Application):
    """Остановка подписчиков."""
    if 'event_bus' in app:
        await app['event_bus'].disconnect()
```

**Analytics Service (Subscriber):**
```python
# analytics-service/event_handlers.py
async def handle_experiment_created(event_type: str, event_data: dict):
    """Обновление статистики при создании эксперимента."""
    project_id = event_data['project_id']

    # Обновляем статистику проекта
    await update_project_stats(project_id, increment_experiments=True)
```

## Dead Letter Queue (DLQ)

### Обработка ошибок

**Проблема:** Что делать, если обработка события провалилась?

**Решение:** Dead Letter Queue - очередь для неудачно обработанных сообщений.

```python
async def setup_queue_with_dlq(channel: aio_pika.Channel):
    """Настройка очереди с DLQ."""
    # Основная очередь
    queue = await channel.declare_queue(
        'experiment_events',
        durable=True,
        arguments={
            'x-dead-letter-exchange': 'dlx',  # Dead Letter Exchange
            'x-dead-letter-routing-key': 'failed.experiment_events'
        }
    )

    # Dead Letter Queue
    dlq = await channel.declare_queue('dlq_experiment_events', durable=True)
    dlx = await channel.declare_exchange('dlx', aio_pika.ExchangeType.DIRECT)
    await dlq.bind(dlx, routing_key='failed.experiment_events')
```

**Обработка с retry:**
```python
async def message_handler(message: aio_pika.IncomingMessage):
    """Обработчик с retry логикой."""
    max_retries = 3

    try:
        event_data = json.loads(message.body.decode())
        await process_event(event_data)
        message.ack()

    except Exception as e:
        # Получаем количество попыток
        retry_count = message.headers.get('x-retry-count', 0) if message.headers else 0

        if retry_count < max_retries:
            # Повторная попытка
            await message.reject(requeue=True)
        else:
            # Отправляем в DLQ
            message.nack(requeue=False)
```

## Message Durability

### Сохранение сообщений

```python
# Публикация с персистентностью
message = aio_pika.Message(
    json.dumps(event_data).encode(),
    delivery_mode=aio_pika.DeliveryMode.PERSISTENT  # Сохранить на диск
)

# Долговечная очередь
queue = await channel.declare_queue(
    'events',
    durable=True  # Очередь переживет перезапуск RabbitMQ
)
```

## Idempotency (Идемпотентность)

### Обработка дубликатов

**Проблема:** Сообщение может быть доставлено дважды.

**Решение:** Идемпотентная обработка.

```python
# Использование Redis для отслеживания обработанных событий
import redis.asyncio as redis

redis_client = await redis.from_url("redis://localhost:6379")

async def handle_event_idempotent(event_type: str, event_data: dict):
    """Идемпотентная обработка события."""
    # Создаем уникальный ключ
    event_id = event_data.get('event_id') or f"{event_type}:{event_data.get('id')}"

    # Проверяем, обрабатывали ли уже
    processed = await redis_client.get(f"event:{event_id}")
    if processed:
        print(f"Event {event_id} already processed, skipping")
        return

    # Обрабатываем
    await process_event(event_data)

    # Отмечаем как обработанное (TTL 24 часа)
    await redis_client.setex(
        f"event:{event_id}",
        86400,
        "processed"
    )
```

## Best Practices

### 1. Используйте отдельные очереди для каждого сервиса

```python
# Каждый сервис имеет свою очередь
queue_name = f"{service_name}_events"
```

### 2. Обрабатывайте ошибки

```python
try:
    await process_event(event_data)
    message.ack()
except Exception as e:
    message.nack(requeue=True)  # Или в DLQ
```

### 3. Используйте prefetch для контроля нагрузки

```python
await channel.set_qos(prefetch_count=10)  # Макс 10 неподтвержденных
```

### 4. Логируйте события

```python
logger.info("event_received", event_type=event_type, event_id=event_id)
logger.info("event_processed", event_type=event_type, duration=duration)
```

### 5. Мониторьте очереди

```python
# Проверка размера очереди
queue_info = await channel.queue_declare('events', passive=True)
queue_length = queue_info.message_count
```

## Дополнительные материалы

### Полезные ссылки
- [RabbitMQ Tutorial](https://www.rabbitmq.com/tutorials/tutorial-one-python.html)
- [aio-pika Documentation](https://aio-pika.readthedocs.io/)
- [Event-Driven Architecture](https://martinfowler.com/articles/201701-event-driven.html)

### Инструменты
- [RabbitMQ Management UI](http://localhost:15672)
- [RabbitMQ CLI Tools](https://www.rabbitmq.com/rabbitmqctl.8.html)

### Статьи
- [RabbitMQ Best Practices](https://www.cloudamqp.com/blog/part1-rabbitmq-best-practice.html)
- [Event Sourcing vs Event-Driven](https://martinfowler.com/articles/201701-event-driven.html)

## Вопросы для самопроверки

1. В чем разница между синхронной и event-driven коммуникацией?
2. Какие типы exchanges есть в RabbitMQ и когда их использовать?
3. Что такое Dead Letter Queue и зачем она нужна?
4. Как обеспечить идемпотентность обработки событий?
5. В чем разница между Queue и Exchange?

## Следующая неделя

На [Неделе 23](../week-23/README.md) изучим Saga pattern для управления распределенными транзакциями! 🚀

---

**Удачи с event-driven архитектурой! 📡**

