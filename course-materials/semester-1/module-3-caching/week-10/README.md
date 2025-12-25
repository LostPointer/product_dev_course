# Неделя 10: Background jobs с Celery и arq

## Цели недели
- Понять когда и зачем нужны background jobs
- Освоить Celery для асинхронных задач
- Изучить arq как легковесную альтернативу
- Научиться работать с task queues и workers
- Реализовать периодические задачи (cron jobs)
- Настроить мониторинг и обработку ошибок

## Теория

### Что такое Background Jobs?

**Background Job** - это задача, которая выполняется асинхронно, отдельно от основного потока выполнения приложения.

### Зачем нужны Background Jobs?

**Без background jobs:**
```python
async def send_welcome_email(request):
    user = await create_user(request)
    await send_email(user.email)  # Блокируем! Ждем 5 секунд
    return web.json_response({"id": user.id})
# Пользователь ждет 5+ секунд ответа
```

**С background jobs:**
```python
async def send_welcome_email(request):
    user = await create_user(request)
    send_email_task.delay(user.email)  # Запускаем в фоне!
    return web.json_response({"id": user.id})
# Пользователь получает ответ моментально
```

### Типичные use cases:

1. **Отправка email/SMS** - не блокируем response
2. **Обработка файлов** - ресайз изображений, конвертация видео
3. **Генерация отчетов** - долгие вычисления
4. **Парсинг данных** - scraping, API calls
5. **Периодические задачи** - очистка БД, обновление кэша
6. **Уведомления** - push notifications
7. **Экспорт данных** - CSV, PDF генерация

### Архитектура с Task Queue

```
┌──────────────┐
│   Client     │
└──────┬───────┘
       │ HTTP Request
       ▼
┌──────────────────┐
│   API Server     │
│   (aiohttp)      │
└──────┬───────────┘
       │ Task → Queue
       ▼
┌──────────────────┐
│   Message Broker │
│   (Redis/RabbitMQ)│
└──────┬───────────┘
       │ Tasks
       ▼
┌──────────────────┐
│   Workers        │
│   (Celery/arq)   │
└──────┬───────────┘
       │
       ▼
┌──────────────────┐
│   External APIs  │
│   SMTP, Storage  │
└──────────────────┘
```

## Celery - мощный task queue

### Что такое Celery?

**Celery** - это распределенная система обработки асинхронных задач на Python.

**Особенности:**
- ✅ Мощный и feature-rich
- ✅ Поддержка разных брокеров (Redis, RabbitMQ, Amazon SQS)
- ✅ Периодические задачи (celery beat)
- ✅ Retry механизм
- ✅ Цепочки задач (chains, groups, chords)
- ✅ Мониторинг (Flower)
- ❌ Сложная настройка
- ❌ Не нативно async

### Установка

```bash
pip install celery redis
```

### Базовая настройка Celery

```python
# src/celery_app.py
from celery import Celery

# Создаем Celery приложение
celery_app = Celery(
    'tasks',
    broker='redis://localhost:6379/0',
    backend='redis://localhost:6379/0'
)

# Конфигурация
celery_app.conf.update(
    task_serializer='json',
    accept_content=['json'],
    result_serializer='json',
    timezone='UTC',
    enable_utc=True,
    task_track_started=True,
    task_time_limit=30 * 60,  # 30 минут максимум
    task_soft_time_limit=25 * 60,  # 25 минут soft limit
)
```

### Создание задач

```python
# src/tasks/email_tasks.py
from src.celery_app import celery_app
import time


@celery_app.task
def send_email(to: str, subject: str, body: str):
    """Отправка email."""
    print(f"Sending email to {to}")
    time.sleep(5)  # Имитация отправки
    print(f"Email sent to {to}")
    return {"status": "sent", "to": to}


@celery_app.task(bind=True, max_retries=3)
def send_email_with_retry(self, to: str, subject: str, body: str):
    """Отправка email с retry."""
    try:
        # Попытка отправить
        print(f"Sending email to {to}")
        # Если ошибка - симулируем
        if to.startswith("bad"):
            raise Exception("SMTP Error")

        time.sleep(2)
        return {"status": "sent"}

    except Exception as exc:
        # Retry через 10 секунд
        raise self.retry(exc=exc, countdown=10)


@celery_app.task
def process_image(image_path: str, size: tuple):
    """Обработка изображения."""
    print(f"Processing {image_path} to size {size}")
    time.sleep(3)
    return {"path": image_path, "size": size, "status": "processed"}


@celery_app.task
def generate_report(user_id: int):
    """Генерация отчета."""
    print(f"Generating report for user {user_id}")
    time.sleep(10)
    return {"user_id": user_id, "report": "report_123.pdf"}
```

### Использование в handlers

```python
# src/handlers/users.py
from aiohttp import web
from src.tasks.email_tasks import send_email, generate_report


async def register_user(request: web.Request) -> web.Response:
    """Регистрация пользователя."""
    data = await request.json()

    # Создаем пользователя в БД
    user = await create_user(data)

    # Отправляем welcome email асинхронно
    send_email.delay(
        to=user.email,
        subject="Welcome!",
        body=f"Hello {user.username}, welcome to our service!"
    )

    # Возвращаем ответ сразу
    return web.json_response({
        "id": user.id,
        "username": user.username
    }, status=201)


async def request_report(request: web.Request) -> web.Response:
    """Запрос генерации отчета."""
    user_id = request['user'].id

    # Запускаем задачу
    task = generate_report.delay(user_id)

    return web.json_response({
        "task_id": task.id,
        "status": "processing",
        "message": "Report is being generated"
    })


async def check_task_status(request: web.Request) -> web.Response:
    """Проверить статус задачи."""
    task_id = request.match_info['task_id']

    # Получаем результат
    from celery.result import AsyncResult
    task = AsyncResult(task_id, app=celery_app)

    if task.ready():
        return web.json_response({
            "task_id": task_id,
            "status": "completed",
            "result": task.result
        })
    else:
        return web.json_response({
            "task_id": task_id,
            "status": "processing",
            "progress": task.info.get('progress', 0) if task.info else 0
        })
```

### Запуск Celery Worker

```bash
# Запуск worker
celery -A src.celery_app worker --loglevel=info

# С несколькими воркерами
celery -A src.celery_app worker --loglevel=info --concurrency=4

# На Windows
celery -A src.celery_app worker --loglevel=info --pool=solo
```

### Периодические задачи (Celery Beat)

```python
# src/celery_app.py
from celery.schedules import crontab

celery_app.conf.beat_schedule = {
    # Каждые 5 минут
    'cleanup-old-sessions': {
        'task': 'src.tasks.cleanup_tasks.cleanup_old_sessions',
        'schedule': 300.0,  # секунды
    },

    # Каждый день в 00:00
    'daily-report': {
        'task': 'src.tasks.report_tasks.generate_daily_report',
        'schedule': crontab(hour=0, minute=0),
    },

    # Каждый понедельник в 09:00
    'weekly-newsletter': {
        'task': 'src.tasks.email_tasks.send_weekly_newsletter',
        'schedule': crontab(hour=9, minute=0, day_of_week=1),
    },

    # Каждые 30 минут
    'refresh-cache': {
        'task': 'src.tasks.cache_tasks.refresh_popular_data',
        'schedule': crontab(minute='*/30'),
    },
}
```

**Запуск beat scheduler:**
```bash
# В отдельном терминале
celery -A src.celery_app beat --loglevel=info
```

### Продвинутые паттерны Celery

#### 1. Цепочки задач (Chains)

```python
from celery import chain

# Последовательное выполнение
@celery_app.task
def download_file(url):
    print(f"Downloading {url}")
    return "/tmp/file.jpg"

@celery_app.task
def resize_image(path):
    print(f"Resizing {path}")
    return "/tmp/file_resized.jpg"

@celery_app.task
def upload_to_s3(path):
    print(f"Uploading {path}")
    return "https://s3.amazonaws.com/file.jpg"

# Создаем цепочку
workflow = chain(
    download_file.s("https://example.com/image.jpg"),
    resize_image.s(),
    upload_to_s3.s()
)

# Запускаем
result = workflow.apply_async()
```

#### 2. Группы задач (Groups)

```python
from celery import group

# Параллельное выполнение
job = group(
    send_email.s("user1@example.com", "Hello", "Body"),
    send_email.s("user2@example.com", "Hello", "Body"),
    send_email.s("user3@example.com", "Hello", "Body"),
)

result = job.apply_async()
```

#### 3. Chord - группа + callback

```python
from celery import chord

# Выполнить группу, потом callback
@celery_app.task
def process_chunk(chunk_id):
    print(f"Processing chunk {chunk_id}")
    return chunk_id

@celery_app.task
def finalize(results):
    print(f"All chunks processed: {results}")
    return {"status": "completed", "chunks": results}

# Обработать 10 чанков, потом finalize
job = chord(
    [process_chunk.s(i) for i in range(10)]
)(finalize.s())

result = job.apply_async()
```

#### 4. Progress tracking

```python
@celery_app.task(bind=True)
def long_task(self, items):
    """Задача с прогрессом."""
    total = len(items)

    for i, item in enumerate(items):
        # Обрабатываем
        process_item(item)

        # Обновляем прогресс
        self.update_state(
            state='PROGRESS',
            meta={
                'current': i + 1,
                'total': total,
                'percent': int((i + 1) / total * 100)
            }
        )

    return {"status": "completed", "processed": total}
```

## arq - легковесная альтернатива

### Что такое arq?

**arq** - это быстрая, простая и асинхронная task queue на Python, использующая Redis.

**Преимущества arq:**
- ✅ Нативно async/await
- ✅ Простая настройка
- ✅ Легковесный
- ✅ Быстрый
- ✅ Отлично работает с aiohttp/FastAPI
- ❌ Меньше features чем Celery
- ❌ Только Redis как брокер

### Установка

```bash
pip install arq
```

### Настройка arq

```python
# src/arq_worker.py
from arq import create_pool
from arq.connections import RedisSettings


async def send_email(ctx, to: str, subject: str, body: str):
    """Отправка email."""
    print(f"Sending email to {to}")
    await asyncio.sleep(2)
    print(f"Email sent to {to}")
    return {"status": "sent", "to": to}


async def process_image(ctx, image_path: str, size: tuple):
    """Обработка изображения."""
    print(f"Processing {image_path}")
    await asyncio.sleep(3)
    return {"path": image_path, "processed": True}


async def generate_report(ctx, user_id: int):
    """Генерация отчета."""
    print(f"Generating report for user {user_id}")
    await asyncio.sleep(10)
    return {"user_id": user_id, "report": "report.pdf"}


# Конфигурация
class WorkerSettings:
    """Настройки arq worker."""

    redis_settings = RedisSettings(
        host='localhost',
        port=6379,
        database=0
    )

    functions = [
        send_email,
        process_image,
        generate_report,
    ]

    # Cron jobs
    cron_jobs = [
        # Каждые 5 минут
        {
            'func': cleanup_old_sessions,
            'minute': {0, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55},
        },
        # Каждый день в 00:00
        {
            'func': daily_report,
            'hour': 0,
            'minute': 0,
        },
    ]

    max_jobs = 10
    job_timeout = 300  # 5 минут
```

### Использование arq в aiohttp

```python
# src/app.py
from aiohttp import web
from arq import create_pool
from arq.connections import RedisSettings


async def startup(app: web.Application):
    """Создание arq pool при старте."""
    app['arq'] = await create_pool(
        RedisSettings(host='localhost', port=6379)
    )


async def cleanup(app: web.Application):
    """Закрытие arq pool."""
    await app['arq'].close()


# src/handlers/users.py
async def register_user(request: web.Request) -> web.Response:
    """Регистрация с arq."""
    data = await request.json()
    user = await create_user(data)

    # Отправляем задачу в arq
    job = await request.app['arq'].enqueue_job(
        'send_email',
        to=user.email,
        subject='Welcome!',
        body=f'Hello {user.username}!'
    )

    return web.json_response({
        "id": user.id,
        "email_job_id": job.job_id
    }, status=201)


async def check_job_status(request: web.Request) -> web.Response:
    """Проверить статус задачи."""
    job_id = request.match_info['job_id']

    from arq.jobs import Job
    job = Job(job_id, request.app['arq'])

    info = await job.info()

    if info is None:
        raise web.HTTPNotFound(reason="Job not found")

    return web.json_response({
        "job_id": job_id,
        "status": info.job_status,
        "result": info.result
    })
```

### Запуск arq worker

```bash
# Запуск worker
arq src.arq_worker.WorkerSettings

# С несколькими воркерами
arq src.arq_worker.WorkerSettings --worker-count 4
```

## Сравнение Celery vs arq

| Характеристика | Celery | arq |
|---------------|--------|-----|
| Async/await | ❌ (нужен gevent) | ✅ Нативно |
| Брокеры | Redis, RabbitMQ, SQS | Redis только |
| Периодические задачи | ✅ Celery Beat | ✅ Cron jobs |
| Retry механизм | ✅ Мощный | ✅ Простой |
| Цепочки задач | ✅ Chains, groups, chords | ❌ |
| Мониторинг | ✅ Flower | ❌ (только логи) |
| Сложность | Высокая | Низкая |
| Производительность | Хорошая | Отличная |
| Use case | Сложные workflows | Простые async задачи |

**Рекомендация:**
- **arq** - для async приложений (aiohttp, FastAPI) с простыми задачами
- **Celery** - для сложных workflows, если нужны chains/groups/chords

## Мониторинг и отладка

### Flower - мониторинг для Celery

**Установка:**
```bash
pip install flower
```

**Запуск:**
```bash
celery -A src.celery_app flower
# Открыть http://localhost:5555
```

**Возможности Flower:**
- 📊 Статистика задач
- 🔍 Мониторинг воркеров
- 📈 Графики производительности
- 🔄 Retry/revoke задач
- 📝 Логи в реальном времени

### Логирование

```python
import logging
from celery.utils.log import get_task_logger

logger = get_task_logger(__name__)


@celery_app.task
def send_email(to: str, subject: str):
    logger.info(f"Sending email to {to}")

    try:
        # Send email
        logger.debug(f"SMTP connection established")
        # ...
        logger.info(f"Email sent successfully to {to}")

    except Exception as e:
        logger.error(f"Failed to send email to {to}: {e}")
        raise
```

### Метрики с Prometheus

```python
# src/celery_app.py
from celery.signals import task_success, task_failure
from prometheus_client import Counter, Histogram

# Метрики
task_success_counter = Counter(
    'celery_task_success_total',
    'Total successful tasks',
    ['task_name']
)

task_failure_counter = Counter(
    'celery_task_failure_total',
    'Total failed tasks',
    ['task_name']
)

task_duration = Histogram(
    'celery_task_duration_seconds',
    'Task duration',
    ['task_name']
)


@task_success.connect
def task_success_handler(sender=None, **kwargs):
    """Обработка успешного завершения."""
    task_success_counter.labels(task_name=sender.name).inc()


@task_failure.connect
def task_failure_handler(sender=None, **kwargs):
    """Обработка ошибки."""
    task_failure_counter.labels(task_name=sender.name).inc()
```

## Best Practices

### 1. Идемпотентность

**Задачи должны быть идемпотентными** - повторное выполнение не должно менять результат.

```python
# ❌ ПЛОХО - не идемпотентно
@celery_app.task
def increment_counter(user_id):
    user = get_user(user_id)
    user.counter += 1  # Если задача запустится дважды - счетчик увеличится дважды!
    save_user(user)


# ✅ ХОРОШО - идемпотентно
@celery_app.task
def set_counter(user_id, value):
    user = get_user(user_id)
    user.counter = value  # Повторное выполнение даст тот же результат
    save_user(user)
```

### 2. Таймауты

```python
# Всегда устанавливайте таймауты
@celery_app.task(time_limit=300, soft_time_limit=240)
def long_task():
    # Максимум 5 минут
    ...
```

### 3. Retry логика

```python
@celery_app.task(
    bind=True,
    max_retries=3,
    default_retry_delay=60  # 1 минута между попытками
)
def unreliable_task(self):
    try:
        # Попытка выполнить
        external_api_call()

    except Exception as exc:
        # Exponential backoff
        self.retry(
            exc=exc,
            countdown=2 ** self.request.retries * 60
        )
```

### 4. Небольшие задачи

```python
# ❌ ПЛОХО - одна большая задача
@celery_app.task
def process_all_users():
    users = User.query.all()  # 100,000 пользователей!
    for user in users:
        send_email(user.email)


# ✅ ХОРОШО - разбить на маленькие задачи
@celery_app.task
def process_users_batch(user_ids):
    users = User.query.filter(User.id.in_(user_ids))
    for user in users:
        send_email(user.email)

# В handler
async def trigger_mass_email():
    user_ids = await get_all_user_ids()

    # Разбиваем на батчи по 100
    for i in range(0, len(user_ids), 100):
        batch = user_ids[i:i+100]
        process_users_batch.delay(batch)
```

### 5. Обработка результатов

```python
# Используйте ignore_result если результат не нужен
@celery_app.task(ignore_result=True)
def send_notification():
    # Результат не нужен
    ...
```

## Дополнительные материалы

### Документация
- [Celery Documentation](https://docs.celeryq.dev/)
- [arq Documentation](https://arq-docs.helpmanual.io/)
- [Redis Documentation](https://redis.io/docs/)

### Статьи
- [Celery Best Practices](https://blog.balthazar-rouberol.com/celery-best-practices)
- [arq vs Celery](https://arq-docs.helpmanual.io/#why-use-arq)
- [Task Queue Patterns](https://www.cloudamqp.com/blog/part1-rabbitmq-best-practice.html)

### Инструменты
- [Flower](https://flower.readthedocs.io/) - Celery monitoring
- [Redis Commander](https://joeferner.github.io/redis-commander/) - Redis GUI
- [Celery Exporter](https://github.com/danihodovic/celery-exporter) - Prometheus metrics

### Видео
- [Celery in Practice](https://www.youtube.com/watch?v=THxCy-6EnQM)
- [Background Jobs Best Practices](https://www.youtube.com/watch?v=ceJ-vy7fvXo)

## Следующая неделя

На [Неделе 11](../week-11/README.md) изучим оптимизацию БД: индексы, N+1 problem, query optimization! 🚀

---

**Удачи с background jobs! 🔄**

