# Неделя 26: Мониторинг и метрики в микросервисах

## Цели недели
- Понять важность мониторинга в микросервисах
- Изучить Prometheus и метрики
- Освоить Grafana для визуализации
- Научиться настраивать alerting
- Понять как собирать и анализировать метрики

## Теоретическая часть

### Зачем нужен мониторинг?

**Мониторинг** - непрерывное отслеживание состояния системы.

**Задачи мониторинга:**
- ✅ Обнаружение проблем до того, как они станут критичными
- ✅ Понимание производительности системы
- ✅ Анализ использования ресурсов
- ✅ Оптимизация на основе данных

**Без мониторинга:**
- ❌ Проблемы обнаруживаются пользователями
- ❌ Нет понимания узких мест
- ❌ Сложно планировать масштабирование
- ❌ Нет данных для оптимизации

## Prometheus

### Что такое Prometheus?

**Prometheus** - система мониторинга и alerting с временной БД.

**Особенности:**
- Pull-based модель (Prometheus сам собирает метрики)
- Многомерная модель данных (метки/labels)
- PromQL - язык запросов
- Интеграция с Grafana

**Архитектура:**
```
Prometheus Server
  ├── Time Series Database
  ├── Data Retrieval (scraping)
  ├── PromQL Engine
  └── Alertmanager Integration

Exporters / Instrumentation
  ├── Application Metrics
  ├── System Metrics
  └── Custom Metrics
```

### Типы метрик

#### 1. Counter

**Counter** - монотонно возрастающее значение.

**Использование:** Количество запросов, ошибок, событий.

```python
from prometheus_client import Counter

http_requests_total = Counter(
    'http_requests_total',
    'Total HTTP requests',
    ['method', 'endpoint', 'status']
)

# Инкремент
http_requests_total.labels(method='GET', endpoint='/api/experiments', status='200').inc()
```

#### 2. Gauge

**Gauge** - значение, которое может увеличиваться и уменьшаться.

**Использование:** Текущее количество активных запросов, использование памяти.

```python
from prometheus_client import Gauge

active_requests = Gauge(
    'http_active_requests',
    'Active HTTP requests',
    ['method', 'endpoint']
)

# Установка значения
active_requests.labels(method='GET', endpoint='/api/experiments').set(5)

# Инкремент/декремент
active_requests.labels(method='GET', endpoint='/api/experiments').inc()
active_requests.labels(method='GET', endpoint='/api/experiments').dec()
```

#### 3. Histogram

**Histogram** - распределение значений в buckets.

**Использование:** Длительность запросов, размер ответов.

```python
from prometheus_client import Histogram

request_duration = Histogram(
    'http_request_duration_seconds',
    'HTTP request duration',
    ['method', 'endpoint'],
    buckets=[0.1, 0.5, 1.0, 2.0, 5.0]  # Buckets в секундах
)

# Измерение длительности
with request_duration.labels(method='GET', endpoint='/api/experiments').time():
    # Ваш код
    await process_request()

# Или вручную
request_duration.labels(method='GET', endpoint='/api/experiments').observe(0.25)
```

#### 4. Summary

**Summary** - похож на Histogram, но вычисляет квантили на клиенте.

**Использование:** Когда нужны точные квантили без buckets.

```python
from prometheus_client import Summary

request_latency = Summary(
    'http_request_latency_seconds',
    'HTTP request latency',
    ['method', 'endpoint']
)

# Аналогично Histogram
with request_latency.labels(method='GET', endpoint='/api/experiments').time():
    await process_request()
```

## Интеграция Prometheus в aiohttp

### Базовая настройка

```python
# experiment-service/main.py
from prometheus_client import Counter, Histogram, Gauge, generate_latest
from prometheus_client import CONTENT_TYPE_LATEST
from aiohttp import web
import time

# Метрики
http_requests_total = Counter(
    'http_requests_total',
    'Total HTTP requests',
    ['method', 'endpoint', 'status']
)

http_request_duration = Histogram(
    'http_request_duration_seconds',
    'HTTP request duration',
    ['method', 'endpoint'],
    buckets=[0.1, 0.5, 1.0, 2.0, 5.0, 10.0]
)

http_active_requests = Gauge(
    'http_active_requests',
    'Active HTTP requests',
    ['method', 'endpoint']
)

# Middleware для автоматического сбора метрик
@web.middleware
async def metrics_middleware(request, handler):
    """Middleware для сбора метрик."""
    method = request.method
    endpoint = request.match_info.route_info.get_info().get('formatter', request.path)

    # Увеличиваем счетчик активных запросов
    http_active_requests.labels(method=method, endpoint=endpoint).inc()

    start_time = time.time()

    try:
        response = await handler(request)

        # Регистрируем запрос
        http_requests_total.labels(
            method=method,
            endpoint=endpoint,
            status=str(response.status)
        ).inc()

        return response

    except Exception as e:
        # Регистрируем ошибку
        http_requests_total.labels(
            method=method,
            endpoint=endpoint,
            status='500'
        ).inc()
        raise

    finally:
        # Измеряем длительность
        duration = time.time() - start_time
        http_request_duration.labels(
            method=method,
            endpoint=endpoint
        ).observe(duration)

        # Уменьшаем счетчик активных запросов
        http_active_requests.labels(method=method, endpoint=endpoint).dec()

# Endpoint для Prometheus
async def metrics_handler(request):
    """Endpoint для Prometheus scraping."""
    return web.Response(
        body=generate_latest(),
        content_type=CONTENT_TYPE_LATEST
    )

app = web.Application(middlewares=[metrics_middleware])
app.router.add_get('/metrics', metrics_handler)
```

### Дополнительные метрики

```python
# experiment-service/metrics.py
from prometheus_client import Counter, Histogram, Gauge

# Метрики базы данных
db_query_duration = Histogram(
    'db_query_duration_seconds',
    'Database query duration',
    ['operation', 'table']
)

db_connections_active = Gauge(
    'db_connections_active',
    'Active database connections'
)

# Метрики бизнес-логики
experiments_created_total = Counter(
    'experiments_created_total',
    'Total experiments created',
    ['project_id']
)

experiment_runs_total = Counter(
    'experiment_runs_total',
    'Total experiment runs',
    ['experiment_id', 'status']
)

# Метрики внешних вызовов
external_api_duration = Histogram(
    'external_api_duration_seconds',
    'External API call duration',
    ['service', 'endpoint']
)

external_api_errors_total = Counter(
    'external_api_errors_total',
    'Total external API errors',
    ['service', 'endpoint', 'status_code']
)
```

### Использование метрик в коде

```python
# experiment-service/handlers/experiments.py
from experiment_service.metrics import (
    experiments_created_total,
    db_query_duration,
    external_api_duration
)
import time

async def create_experiment_handler(request: web.Request):
    """Создание эксперимента с метриками."""
    # Измеряем длительность запроса к БД
    start = time.time()
    experiment = await experiment_service.create(await request.json())
    db_query_duration.labels(
        operation='create',
        table='experiments'
    ).observe(time.time() - start)

    # Инкремент счетчика создания
    experiments_created_total.labels(
        project_id=str(experiment['project_id'])
    ).inc()

    return web.json_response(experiment, status=201)

async def call_external_service(service_url: str, endpoint: str):
    """Вызов внешнего сервиса с метриками."""
    start = time.time()

    try:
        async with httpx.AsyncClient() as client:
            response = await client.get(f"{service_url}{endpoint}")
            response.raise_for_status()

            # Регистрируем успешный вызов
            external_api_duration.labels(
                service=service_url,
                endpoint=endpoint
            ).observe(time.time() - start)

            return response.json()

    except httpx.HTTPError as e:
        # Регистрируем ошибку
        external_api_errors_total.labels(
            service=service_url,
            endpoint=endpoint,
            status_code=str(getattr(e, 'status_code', 'unknown'))
        ).inc()
        raise
```

## Docker Compose для Prometheus

### Настройка Prometheus

```yaml
# docker-compose.yml
version: '3.8'

services:
  prometheus:
    image: prom/prometheus:latest
    ports:
      - "9090:9090"
    volumes:
      - ./prometheus/prometheus.yml:/etc/prometheus/prometheus.yml
      - prometheus_data:/prometheus
    command:
      - '--config.file=/etc/prometheus/prometheus.yml'
      - '--storage.tsdb.path=/prometheus'
      - '--web.console.libraries=/usr/share/prometheus/console_libraries'
      - '--web.console.templates=/usr/share/prometheus/consoles'
    healthcheck:
      test: ["CMD", "wget", "--quiet", "--tries=1", "--spider", "http://localhost:9090/-/healthy"]
      interval: 10s
      timeout: 5s
      retries: 5

volumes:
  prometheus_data:
```

### Конфигурация Prometheus

```yaml
# prometheus/prometheus.yml
global:
  scrape_interval: 15s  # Интервал сбора метрик
  evaluation_interval: 15s  # Интервал оценки правил alerting

scrape_configs:
  # Experiment Service
  - job_name: 'experiment-service'
    static_configs:
      - targets: ['experiment-service:8000']
        labels:
          service: 'experiment-service'
          environment: 'development'

  # Metrics Service
  - job_name: 'metrics-service'
    static_configs:
      - targets: ['metrics-service:8002']
        labels:
          service: 'metrics-service'
          environment: 'development'

  # Resource Service
  - job_name: 'resource-service'
    static_configs:
      - targets: ['resource-service:8001']
        labels:
          service: 'resource-service'
          environment: 'development'

  # Prometheus сам
  - job_name: 'prometheus'
    static_configs:
      - targets: ['localhost:9090']
```

**Prometheus UI:** http://localhost:9090

## Grafana

### Что такое Grafana?

**Grafana** - платформа для визуализации метрик и логов.

**Возможности:**
- Дашборды с графиками
- Алерты
- Интеграция с Prometheus, Elasticsearch, Jaeger

### Docker Compose для Grafana

```yaml
# docker-compose.yml
services:
  grafana:
    image: grafana/grafana:latest
    ports:
      - "3000:3000"
    environment:
      - GF_SECURITY_ADMIN_USER=admin
      - GF_SECURITY_ADMIN_PASSWORD=admin
      - GF_INSTALL_PLUGINS=grafana-piechart-panel
    volumes:
      - grafana_data:/var/lib/grafana
      - ./grafana/provisioning:/etc/grafana/provisioning
    depends_on:
      - prometheus
    healthcheck:
      test: ["CMD", "wget", "--quiet", "--tries=1", "--spider", "http://localhost:3000/api/health"]
      interval: 10s
      timeout: 5s
      retries: 5

volumes:
  grafana_data:
```

**Grafana UI:** http://localhost:3000 (admin/admin)

### Настройка Prometheus как источника данных

```yaml
# grafana/provisioning/datasources/prometheus.yml
apiVersion: 1

datasources:
  - name: Prometheus
    type: prometheus
    access: proxy
    url: http://prometheus:9090
    isDefault: true
    editable: true
```

### Создание дашборда

**Пример дашборда для HTTP метрик:**

```json
{
  "dashboard": {
    "title": "HTTP Metrics Dashboard",
    "panels": [
      {
        "title": "Request Rate",
        "targets": [
          {
            "expr": "rate(http_requests_total[5m])",
            "legendFormat": "{{method}} {{endpoint}}"
          }
        ]
      },
      {
        "title": "Request Duration (p95)",
        "targets": [
          {
            "expr": "histogram_quantile(0.95, rate(http_request_duration_seconds_bucket[5m]))",
            "legendFormat": "p95 {{method}} {{endpoint}}"
          }
        ]
      },
      {
        "title": "Error Rate",
        "targets": [
          {
            "expr": "rate(http_requests_total{status=~\"5..\"}[5m])",
            "legendFormat": "{{method}} {{endpoint}}"
          }
        ]
      },
      {
        "title": "Active Requests",
        "targets": [
          {
            "expr": "http_active_requests",
            "legendFormat": "{{method}} {{endpoint}}"
          }
        ]
      }
    ]
  }
}
```

## PromQL (Prometheus Query Language)

### Базовые запросы

```promql
# Простая метрика
http_requests_total

# Фильтр по labels
http_requests_total{method="GET", status="200"}

# Rate (производная за единицу времени)
rate(http_requests_total[5m])

# Increase (увеличение за период)
increase(http_requests_total[1h])

# Sum (сумма)
sum(http_requests_total) by (method)

# Average (среднее)
avg(http_request_duration_seconds) by (endpoint)

# Max/Min
max(http_request_duration_seconds) by (endpoint)
min(http_request_duration_seconds) by (endpoint)

# Quantile для Histogram
histogram_quantile(0.95, rate(http_request_duration_seconds_bucket[5m]))

# Percentage ошибок
sum(rate(http_requests_total{status=~"5.."}[5m])) / sum(rate(http_requests_total[5m])) * 100
```

### Полезные запросы

```promql
# Requests per second по endpoint
sum(rate(http_requests_total[5m])) by (endpoint)

# P95 latency по endpoint
histogram_quantile(0.95,
  sum(rate(http_request_duration_seconds_bucket[5m])) by (le, endpoint)
)

# Error rate в процентах
sum(rate(http_requests_total{status=~"5.."}[5m])) /
sum(rate(http_requests_total[5m])) * 100

# Active connections
db_connections_active

# Top 10 endpoints по requests
topk(10, sum(rate(http_requests_total[5m])) by (endpoint))
```

## Alerting

### Настройка Alertmanager

```yaml
# docker-compose.yml
services:
  alertmanager:
    image: prom/alertmanager:latest
    ports:
      - "9093:9093"
    volumes:
      - ./alertmanager/alertmanager.yml:/etc/alertmanager/alertmanager.yml
    command:
      - '--config.file=/etc/alertmanager/alertmanager.yml'
    depends_on:
      - prometheus
```

### Конфигурация Alertmanager

```yaml
# alertmanager/alertmanager.yml
global:
  resolve_timeout: 5m

route:
  group_by: ['alertname', 'service']
  group_wait: 10s
  group_interval: 10s
  repeat_interval: 12h
  receiver: 'default'

  routes:
    - match:
        severity: critical
      receiver: 'critical-alerts'

    - match:
        severity: warning
      receiver: 'warning-alerts'

receivers:
  - name: 'default'
    webhook_configs:
      - url: 'http://notification-service:8000/webhooks/alerts'

  - name: 'critical-alerts'
    webhook_configs:
      - url: 'http://notification-service:8000/webhooks/alerts/critical'

  - name: 'warning-alerts'
    webhook_configs:
      - url: 'http://notification-service:8000/webhooks/alerts/warning'
```

### Правила алертинга в Prometheus

```yaml
# prometheus/alerts.yml
groups:
  - name: http_alerts
    interval: 30s
    rules:
      - alert: HighErrorRate
        expr: |
          sum(rate(http_requests_total{status=~"5.."}[5m])) /
          sum(rate(http_requests_total[5m])) > 0.05
        for: 5m
        labels:
          severity: critical
        annotations:
          summary: "High error rate detected"
          description: "Error rate is {{ $value | humanizePercentage }}"

      - alert: HighLatency
        expr: |
          histogram_quantile(0.95,
            sum(rate(http_request_duration_seconds_bucket[5m])) by (le, endpoint)
          ) > 2
        for: 10m
        labels:
          severity: warning
        annotations:
          summary: "High latency detected"
          description: "P95 latency is {{ $value }}s for {{ $labels.endpoint }}"

      - alert: HighRequestRate
        expr: |
          sum(rate(http_requests_total[5m])) > 1000
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "High request rate"
          description: "Request rate is {{ $value }} req/s"

      - alert: ServiceDown
        expr: up{job=~".*-service"} == 0
        for: 1m
        labels:
          severity: critical
        annotations:
          summary: "Service is down"
          description: "{{ $labels.job }} is down"

  - name: database_alerts
    interval: 30s
    rules:
      - alert: TooManyDBConnections
        expr: db_connections_active > 80
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "Too many database connections"
          description: "{{ $value }} active connections"
```

### Подключение alerts к Prometheus

```yaml
# prometheus/prometheus.yml
global:
  scrape_interval: 15s
  evaluation_interval: 15s

# ... scrape_configs ...

# Alertmanager configuration
alerting:
  alertmanagers:
    - static_configs:
        - targets: ['alertmanager:9093']

# Load alerting rules
rule_files:
  - 'alerts.yml'
```

## Мониторинг системы

### Node Exporter

**Node Exporter** - метрики системы (CPU, память, диск).

```yaml
# docker-compose.yml
services:
  node-exporter:
    image: prom/node-exporter:latest
    ports:
      - "9100:9100"
    volumes:
      - /proc:/host/proc:ro
      - /sys:/host/sys:ro
      - /:/rootfs:ro
    command:
      - '--path.procfs=/host/proc'
      - '--path.sysfs=/host/sys'
      - '--collector.filesystem.mount-points-exclude=^/(sys|proc|dev|host|etc)($$|/)'
```

### Конфигурация в Prometheus

```yaml
# prometheus/prometheus.yml
scrape_configs:
  - job_name: 'node'
    static_configs:
      - targets: ['node-exporter:9100']
```

### Полезные метрики системы

```promql
# CPU usage
100 - (avg(irate(node_cpu_seconds_total{mode="idle"}[5m])) * 100)

# Memory usage
node_memory_MemTotal_bytes - node_memory_MemAvailable_bytes

# Disk usage
100 - ((node_filesystem_avail_bytes{mountpoint="/"} * 100) / node_filesystem_size_bytes{mountpoint="/"})

# Network traffic
rate(node_network_receive_bytes_total[5m])
rate(node_network_transmit_bytes_total[5m])
```

## Best Practices

### 1. Именование метрик

```python
# ✅ ХОРОШО - префикс, суффикс единиц
http_requests_total
http_request_duration_seconds
db_connections_active

# ❌ ПЛОХО
requests
duration
connections
```

### 2. Используйте labels правильно

```python
# ✅ ХОРОШО - ограниченное количество label значений
http_requests_total{method="GET", endpoint="/api/experiments", status="200"}

# ❌ ПЛОХО - слишком много уникальных значений
http_requests_total{user_id="123", request_id="abc"}
```

### 3. Не создавайте метрики с высоким кардиналом

**Кардинал** - количество уникальных комбинаций labels.

```python
# ❌ ПЛОХО - высокая кардинальность
requests_total{user_id=unique_id}  # Тысячи уникальных user_id

# ✅ ХОРОШО - низкая кардинальность
requests_total{status="200"}  # Всего несколько значений status
```

### 4. Используйте правильный тип метрики

```python
# Counter для событий
requests_total.inc()

# Gauge для текущего состояния
active_connections.set(10)

# Histogram для распределения
request_duration.observe(0.5)
```

### 5. Экспортируйте метрики на отдельном endpoint

```python
# Отдельный endpoint для метрик
app.router.add_get('/metrics', metrics_handler)
```

## Практический пример: Полная настройка

### Структура проекта

```
experiment-service/
├── main.py
├── metrics.py
├── handlers/
│   └── experiments.py
└── prometheus/
    └── prometheus.yml
```

### main.py с метриками

```python
# experiment-service/main.py
from aiohttp import web
from experiment_service.metrics import setup_metrics

# Настройка метрик
setup_metrics()

app = web.Application(middlewares=[metrics_middleware])
app.router.add_get('/metrics', metrics_handler)

# ... остальные routes ...
```

## Дополнительные материалы

### Полезные ссылки
- [Prometheus Documentation](https://prometheus.io/docs/)
- [Grafana Documentation](https://grafana.com/docs/)
- [PromQL Guide](https://prometheus.io/docs/prometheus/latest/querying/basics/)

### Инструменты
- [Prometheus UI](http://localhost:9090)
- [Grafana UI](http://localhost:3000)
- [PromQL Cheat Sheet](https://promlabs.com/promql-cheat-sheet/)

### Статьи
- [Prometheus Best Practices](https://prometheus.io/docs/practices/)
- [Monitoring Microservices](https://www.monitoring.dev/)

## Вопросы для самопроверки

1. В чем разница между Counter, Gauge и Histogram?
2. Что такое PromQL и для чего он нужен?
3. Как настроить alerting в Prometheus?
4. Что такое кардинальность метрик и почему это важно?
5. Как визуализировать метрики в Grafana?

## Следующая неделя

На [Неделе 27](../week-27/README.md) изучим Нагрузочное тестирование: Locust, k6 и оптимизация производительности! 🚀

---

**Удачи с мониторингом! 📊**

