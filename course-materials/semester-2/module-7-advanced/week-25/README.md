# Неделя 25: Observability в микросервисах

## Цели недели
- Понять важность observability в микросервисах
- Изучить Distributed Tracing с Jaeger
- Освоить Centralized Logging с ELK Stack
- Научиться настраивать трейсинг и логирование
- Понять как отлаживать распределенные системы

## Теоретическая часть

### Что такое Observability?

**Observability (Наблюдаемость)** - способность понимать внутреннее состояние системы по ее выходным данным.

**Три столпа Observability:**
1. **Logging** - логи событий
2. **Metrics** - числовые метрики
3. **Tracing** - распределенный трейсинг запросов

### Проблемы в микросервисах

**В монолите:**
```
Request → Application → Database
         (один процесс, все логи в одном месте)
```

**В микросервисах:**
```
Request → Gateway → Service A → Service B → Service C → Database
         (много процессов, логи разбросаны)
```

**Проблемы:**
- ❌ Не видно полный путь запроса
- ❌ Логи разбросаны по сервисам
- ❌ Сложно найти причину ошибки
- ❌ Непонятно, какой сервис медленный

## Distributed Tracing

### Что такое Distributed Tracing?

**Distributed Tracing** - отслеживание пути запроса через множество сервисов.

**Компоненты:**
- **Trace** - весь путь запроса
- **Span** - отдельный шаг в trace
- **Context** - передача trace context между сервисами

```
Trace (request_id: abc123)
├── Span 1: Gateway (10ms)
│   └── Span 2: Auth Service (5ms)
├── Span 3: Experiment Service (20ms)
│   ├── Span 4: Database Query (15ms)
│   └── Span 5: Metrics Service (10ms)
└── Span 6: Response (5ms)
```

### OpenTracing / OpenTelemetry

**OpenTelemetry** - стандарт для трейсинга:
- Агностичен к бэкенду (Jaeger, Zipkin, etc.)
- Поддержка многих языков
- Автоматическая инструментация

### Jaeger

**Jaeger** - система для distributed tracing:
- Сбор trace данных
- Визуализация трассировок
- Анализ производительности

## Настройка Jaeger

### Docker Compose для Jaeger

```yaml
# docker-compose.yml
version: '3.8'

services:
  jaeger:
    image: jaegertracing/all-in-one:latest
    ports:
      - "16686:16686"  # Jaeger UI
      - "14268:14268"  # HTTP collector
      - "6831:6831/udp"  # UDP collector
      - "6832:6832/udp"
    environment:
      - COLLECTOR_ZIPKIN_HTTP_PORT=9411
    healthcheck:
      test: ["CMD", "wget", "--quiet", "--tries=1", "--spider", "http://localhost:14269/"]
      interval: 10s
      timeout: 5s
      retries: 5
```

**Jaeger UI:** http://localhost:16686

### Установка OpenTelemetry для Python

```bash
pip install opentelemetry-api opentelemetry-sdk
pip install opentelemetry-instrumentation-aiohttp
pip install opentelemetry-exporter-jaeger
```

## Интеграция Tracing в сервисы

### Базовая настройка OpenTelemetry

```python
# shared/tracing.py
from opentelemetry import trace
from opentelemetry.exporter.jaeger import JaegerExporter
from opentelemetry.sdk.trace import TracerProvider
from opentelemetry.sdk.trace.export import BatchSpanProcessor
from opentelemetry.sdk.resources import Resource

def setup_tracing(service_name: str, jaeger_endpoint: str = "localhost:6831"):
    """Настройка OpenTelemetry для сервиса."""
    # Создаем Resource с информацией о сервисе
    resource = Resource.create({
        "service.name": service_name,
        "service.version": "1.0.0"
    })

    # Создаем TracerProvider
    provider = TracerProvider(resource=resource)

    # Настраиваем Jaeger exporter
    jaeger_exporter = JaegerExporter(
        agent_host_name="jaeger",
        agent_port=6831,
    )

    # Добавляем BatchSpanProcessor
    span_processor = BatchSpanProcessor(jaeger_exporter)
    provider.add_span_processor(span_processor)

    # Устанавливаем глобальный provider
    trace.set_tracer_provider(provider)

    return trace.get_tracer(__name__)
```

### Инструментация aiohttp приложения

```python
# experiment-service/main.py
from opentelemetry.instrumentation.aiohttp import AioHttpClientInstrumentor
from opentelemetry.instrumentation.aiohttp_client import create_trace_config
from shared.tracing import setup_tracing
from aiohttp import web
import aiohttp

# Настройка tracing
tracer = setup_tracing("experiment-service")

# Инструментация aiohttp клиента
AioHttpClientInstrumentor().instrument()

async def init_tracing(app: web.Application):
    """Инициализация tracing при старте."""
    # Tracing уже настроен в setup_tracing
    app['tracer'] = tracer

async def cleanup_tracing(app: web.Application):
    """Очистка при остановке."""
    # Flush всех spans
    pass

# Middleware для автоматического создания spans
@web.middleware
async def tracing_middleware(request, handler):
    """Middleware для создания trace span."""
    tracer = request.app['tracer']

    # Извлекаем trace context из заголовков
    trace_context = tracer.start_as_current_span(
        name=f"{request.method} {request.path}"
    )

    with trace_context:
        # Добавляем атрибуты
        trace_context.set_attribute("http.method", request.method)
        trace_context.set_attribute("http.url", str(request.url))
        trace_context.set_attribute("http.route", request.path)

        try:
            response = await handler(request)
            trace_context.set_attribute("http.status_code", response.status)
            return response

        except Exception as e:
            trace_context.set_attribute("error", True)
            trace_context.set_attribute("error.message", str(e))
            raise

app = web.Application(middlewares=[tracing_middleware])

app.on_startup.append(init_tracing)
app.on_cleanup.append(cleanup_tracing)
```

### Ручное создание Spans

```python
# experiment-service/handlers/experiments.py
from opentelemetry import trace
from aiohttp import web

tracer = trace.get_tracer(__name__)

async def create_experiment_handler(request: web.Request):
    """Создание эксперимента с tracing."""
    with tracer.start_as_current_span("create_experiment") as span:
        # Получаем данные
        data = await request.json()
        span.set_attribute("experiment.name", data.get("name", ""))

        # Создаем эксперимент (вложенный span)
        with tracer.start_as_current_span("db.insert_experiment") as db_span:
            experiment = await experiment_service.create(data)
            db_span.set_attribute("experiment.id", experiment['id'])

        # Публикуем событие (вложенный span)
        with tracer.start_as_current_span("event.publish") as event_span:
            await event_bus.publish("experiment.created", {
                "experiment_id": experiment['id']
            })
            event_span.set_attribute("event.type", "experiment.created")

        span.set_attribute("experiment.id", experiment['id'])
        return web.json_response(experiment, status=201)
```

### Передача Trace Context между сервисами

```python
# experiment-service/client_wrapper.py
from opentelemetry import trace
from opentelemetry.propagate import inject, extract
from opentelemetry.trace.propagation.tracecontext import TraceContextTextMapPropagator
import httpx

tracer = trace.get_tracer(__name__)

async def call_metrics_service(experiment_id: int):
    """Вызов metrics-service с передачей trace context."""
    with tracer.start_as_current_span("call_metrics_service") as span:
        span.set_attribute("experiment.id", experiment_id)

        # Создаем заголовки с trace context
        headers = {}
        inject(headers)  # Инжектируем текущий trace context

        async with httpx.AsyncClient() as client:
            response = await client.get(
                f"http://metrics-service:8002/metrics/{experiment_id}",
                headers=headers  # Передаем trace context
            )
            response.raise_for_status()
            return response.json()
```

### Получение Trace Context на принимающем сервисе

```python
# metrics-service/main.py
from opentelemetry.propagate import extract
from opentelemetry.trace import get_current_span

@web.middleware
async def extract_trace_context(request, handler):
    """Извлечение trace context из заголовков."""
    # Извлекаем trace context из заголовков запроса
    context = extract(dict(request.headers))

    with trace.use_span(context):
        return await handler(request)
```

## Centralized Logging

### Проблема с логами в микросервисах

**Без централизации:**
- Логи в разных файлах/серверах
- Сложно найти все логи по запросу
- Нет контекста между сервисами

**С централизацией:**
- Все логи в одном месте
- Поиск по request_id
- Корреляция логов между сервисами

### ELK Stack

**ELK Stack:**
- **Elasticsearch** - хранение и поиск логов
- **Logstash** - обработка и трансформация логов
- **Kibana** - визуализация логов

**Альтернативы:**
- **Fluentd** вместо Logstash
- **Loki** (Grafana) - легковесная альтернатива
- **Elastic Cloud** - managed решение

### Docker Compose для ELK

```yaml
# docker-compose.yml
version: '3.8'

services:
  elasticsearch:
    image: docker.elastic.co/elasticsearch/elasticsearch:8.8.0
    environment:
      - discovery.type=single-node
      - xpack.security.enabled=false
      - "ES_JAVA_OPTS=-Xms512m -Xmx512m"
    ports:
      - "9200:9200"
    volumes:
      - elasticsearch_data:/usr/share/elasticsearch/data

  logstash:
    image: docker.elastic.co/logstash/logstash:8.8.0
    volumes:
      - ./logstash/pipeline:/usr/share/logstash/pipeline
      - ./logstash/config:/usr/share/logstash/config
    ports:
      - "5044:5044"
      - "9600:9600"
    depends_on:
      - elasticsearch

  kibana:
    image: docker.elastic.co/kibana/kibana:8.8.0
    environment:
      - ELASTICSEARCH_HOSTS=http://elasticsearch:9200
    ports:
      - "5601:5601"
    depends_on:
      - elasticsearch

volumes:
  elasticsearch_data:
```

**Kibana UI:** http://localhost:5601

### Structured Logging с trace context

```python
# shared/logging_config.py
import structlog
from opentelemetry import trace

def configure_logging():
    """Настройка structured logging с trace context."""
    structlog.configure(
        processors=[
            structlog.contextvars.merge_contextvars,  # Добавляет context variables
            structlog.processors.add_log_level,
            structlog.processors.TimeStamper(fmt="iso"),
            structlog.processors.JSONRenderer()
        ],
        wrapper_class=structlog.make_filtering_bound_logger(logging.INFO),
        context_class=dict,
        logger_factory=structlog.PrintLoggerFactory(),
        cache_logger_on_first_use=True,
    )

def get_logger(name: str):
    """Получение logger с автоматическим добавлением trace context."""
    logger = structlog.get_logger(name)

    # Получаем текущий span
    span = trace.get_current_span()
    if span:
        span_context = span.get_span_context()
        # Добавляем trace context в логи
        logger = logger.bind(
            trace_id=format(span_context.trace_id, '032x'),
            span_id=format(span_context.span_id, '016x')
        )

    return logger
```

### Использование в сервисах

```python
# experiment-service/handlers/experiments.py
from shared.logging_config import get_logger
from opentelemetry import trace

logger = get_logger(__name__)

async def create_experiment_handler(request: web.Request):
    """Создание эксперимента с логированием."""
    span = trace.get_current_span()

    # Логирование с trace context
    logger.info(
        "creating_experiment",
        method=request.method,
        path=request.path,
        user_id=request.get('user_id')
    )

    try:
        data = await request.json()

        experiment = await experiment_service.create(data)

        logger.info(
            "experiment_created",
            experiment_id=experiment['id'],
            name=experiment['name']
        )

        return web.json_response(experiment, status=201)

    except Exception as e:
        logger.error(
            "experiment_creation_failed",
            error=str(e),
            error_type=type(e).__name__
        )
        raise
```

### Отправка логов в ELK

**Вариант 1: Filebeat (рекомендуется)**

```yaml
# docker-compose.yml
services:
  filebeat:
    image: docker.elastic.co/beats/filebeat:8.8.0
    volumes:
      - ./filebeat/filebeat.yml:/usr/share/filebeat/filebeat.yml:ro
      - ./logs:/var/log/app:ro
    depends_on:
      - elasticsearch
```

```yaml
# filebeat/filebeat.yml
filebeat.inputs:
  - type: log
    enabled: true
    paths:
      - /var/log/app/*.log
    json.keys_under_root: true
    json.add_error_key: true

output.elasticsearch:
  hosts: ["elasticsearch:9200"]
```

**Вариант 2: Прямая отправка в Elasticsearch**

```python
# shared/logging_elasticsearch.py
from elasticsearch import AsyncElasticsearch
import structlog

es_client = AsyncElasticsearch(['http://elasticsearch:9200'])

class ElasticsearchProcessor:
    """Processor для отправки логов в Elasticsearch."""

    def __call__(self, logger, method_name, event_dict):
        """Отправка лога в Elasticsearch."""
        asyncio.create_task(self._send_to_elasticsearch(event_dict))
        return event_dict

    async def _send_to_elasticsearch(self, log_data):
        """Асинхронная отправка в Elasticsearch."""
        try:
            await es_client.index(
                index=f"logs-{datetime.now().strftime('%Y.%m.%d')}",
                body=log_data
            )
        except Exception as e:
            print(f"Failed to send log to Elasticsearch: {e}")

# Настройка
structlog.configure(
    processors=[
        structlog.contextvars.merge_contextvars,
        structlog.processors.add_log_level,
        structlog.processors.TimeStamper(fmt="iso"),
        ElasticsearchProcessor(),  # Отправка в Elasticsearch
        structlog.dev.ConsoleRenderer()  # И в консоль
    ]
)
```

## Корреляция Logs и Traces

### Связывание логов и traces

```python
# shared/logging_config.py
from opentelemetry import trace

def get_logger_with_trace(name: str):
    """Logger с автоматическим добавлением trace context."""
    logger = structlog.get_logger(name)

    span = trace.get_current_span()
    if span:
        span_context = span.get_span_context()
        # Добавляем trace_id и span_id для корреляции
        logger = logger.bind(
            trace_id=format(span_context.trace_id, '032x'),
            span_id=format(span_context.span_id, '016x')
        )

    return logger

# Использование
logger = get_logger_with_trace(__name__)

logger.info("processing_request", data=request_data)
# В логе будет trace_id, который можно найти в Jaeger!
```

### Поиск логов по trace_id в Kibana

```json
// Запрос в Kibana
{
  "query": {
    "match": {
      "trace_id": "abc123def456..."
    }
  }
}
```

## Метрики (кратко)

### Prometheus метрики

```python
# shared/metrics.py
from prometheus_client import Counter, Histogram, Gauge

# Метрики
request_count = Counter(
    'http_requests_total',
    'Total HTTP requests',
    ['method', 'endpoint', 'status']
)

request_duration = Histogram(
    'http_request_duration_seconds',
    'HTTP request duration',
    ['method', 'endpoint']
)

active_requests = Gauge(
    'http_active_requests',
    'Active HTTP requests',
    ['method', 'endpoint']
)

# Middleware для автоматического сбора метрик
@web.middleware
async def metrics_middleware(request, handler):
    """Middleware для сбора метрик."""
    method = request.method
    endpoint = request.path

    active_requests.labels(method=method, endpoint=endpoint).inc()

    with request_duration.labels(method=method, endpoint=endpoint).time():
        try:
            response = await handler(request)
            request_count.labels(
                method=method,
                endpoint=endpoint,
                status=response.status
            ).inc()
            return response
        finally:
            active_requests.labels(method=method, endpoint=endpoint).dec()
```

## Практический пример: Полная настройка

### Структура проекта

```
experiment-service/
├── main.py
├── shared/
│   ├── tracing.py
│   ├── logging_config.py
│   └── metrics.py
└── handlers/
    └── experiments.py
```

### main.py с полной observability

```python
# experiment-service/main.py
from aiohttp import web
from opentelemetry.instrumentation.aiohttp import AioHttpClientInstrumentor
from shared.tracing import setup_tracing
from shared.logging_config import configure_logging
from shared.metrics import metrics_middleware
import aiohttp

# Настройка logging
configure_logging()

# Настройка tracing
tracer = setup_tracing("experiment-service")
AioHttpClientInstrumentor().instrument()

# Middlewares
app = web.Application(middlewares=[
    tracing_middleware,
    metrics_middleware,
    logging_middleware
])

async def init_app(app: web.Application):
    """Инициализация приложения."""
    app['tracer'] = tracer
    # Инициализация других компонентов

async def cleanup_app(app: web.Application):
    """Очистка при остановке."""
    pass

app.on_startup.append(init_app)
app.on_cleanup.append(cleanup_app)
```

## Best Practices

### 1. Структурированное логирование

```python
# ✅ ХОРОШО
logger.info(
    "experiment_created",
    experiment_id=123,
    user_id=456,
    duration_ms=150
)

# ❌ ПЛОХО
logger.info(f"Experiment {123} created by user {456} in 150ms")
```

### 2. Добавляйте context

```python
# Добавляйте контекст к каждому логу
logger = logger.bind(
    request_id=request_id,
    user_id=user_id,
    service="experiment-service"
)
```

### 3. Уровни логирования

```python
logger.debug("detailed_info")  # Детальная информация
logger.info("important_event")  # Важные события
logger.warning("potential_issue")  # Потенциальные проблемы
logger.error("error_occurred")  # Ошибки
logger.critical("critical_failure")  # Критичные ошибки
```

### 4. Не логируйте чувствительные данные

```python
# ❌ ПЛОХО
logger.info("user_login", password=password, token=token)

# ✅ ХОРОШО
logger.info("user_login", user_id=user_id, username=username)
```

### 5. Семантическая конвенция для spans

```python
# Формат: <operation> <resource>
tracer.start_as_current_span("db.get_experiment")
tracer.start_as_current_span("http.call_metrics_service")
tracer.start_as_current_span("event.publish_experiment_created")
```

## Дополнительные материалы

### Полезные ссылки
- [OpenTelemetry Documentation](https://opentelemetry.io/docs/)
- [Jaeger Documentation](https://www.jaegertracing.io/docs/)
- [ELK Stack Guide](https://www.elastic.co/guide/)

### Библиотеки
- [opentelemetry-python](https://opentelemetry.io/docs/instrumentation/python/)
- [structlog](https://www.structlog.org/)
- [prometheus-client](https://github.com/prometheus/client_python)

### Статьи
- [Distributed Tracing Best Practices](https://opentelemetry.io/docs/specs/otel/trace/)
- [Logging Best Practices](https://www.structlog.org/en/stable/why.html)

## Вопросы для самопроверки

1. В чем разница между Logging, Metrics и Tracing?
2. Что такое Trace и Span в distributed tracing?
3. Как передается trace context между сервисами?
4. Зачем нужен centralized logging?
5. Как связать логи и traces для отладки?

## Следующая неделя

На [Неделе 26](../week-26/README.md) изучим Мониторинг и метрики: Prometheus, Grafana и alerting! 🚀

---

**Удачи с observability! 🔍**

