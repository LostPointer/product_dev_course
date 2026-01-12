# Как логи попадают из сервисов в Grafana

## Общая схема потока логов

```
Сервисы (Python/Node.js)
    ↓ (stdout/stderr)
Docker контейнеры
    ↓ (логи в /var/lib/docker/containers/)
Grafana Alloy
    ↓ (сбор, обработка, добавление меток)
Loki (хранилище логов)
    ↓ (HTTP API)
Grafana (веб-интерфейс)
```

## Детальное описание процесса

### 1. Генерация логов в сервисах

#### Backend сервисы (Python)
- Используют библиотеку `structlog` для структурированного логирования
- Логи выводятся в формате **TSKV** (Tagged Key-Value): `key=value key2=value2`
- Формат: `timestamp='2024-01-01T12:00:00Z' level='info' logger='service' event='Request completed' method='GET' path='/api/v1' status_code=200 trace_id='...' request_id='...'`
- Логи пишутся в **stdout** через `logging.StreamHandler`

**Пример конфигурации:**
```python
# projects/backend/common/src/backend_common/logging_config.py
structlog.configure(
    processors=[
        structlog.contextvars.merge_contextvars,  # Добавляет trace_id, request_id
        structlog.stdlib.add_log_level,
        structlog.processors.TimeStamper(fmt="iso"),
        structlog.processors.KeyValueRenderer(),  # Формат key=value
    ]
)
```

#### Frontend сервисы (Node.js)
- Используют стандартные логи Node.js (pino, winston и т.д.)
- Логи также выводятся в **stdout/stderr**

### 2. Docker собирает логи

- Docker автоматически перехватывает stdout/stderr всех контейнеров
- Логи сохраняются в файлы: `/var/lib/docker/containers/<container-id>/<container-id>-json.log`
- Формат: JSON обертка Docker с полями `log`, `stream`, `time`
- Все контейнеры проекта имеют метку: `com.docker.compose.project=product_dev_course`

### 3. Grafana Alloy собирает логи

#### Discovery (обнаружение контейнеров)
```river
discovery.docker "containers" {
  host = "unix:///var/run/docker.sock"
  refresh_interval = "5s"

  filter {
    name   = "label"
    values = ["com.docker.compose.project=product_dev_course"]
  }
}
```
- Alloy подключается к Docker socket (`/var/run/docker.sock`)
- Каждые 5 секунд обновляет список контейнеров
- Фильтрует только контейнеры проекта `product_dev_course`

#### Сбор логов
```river
loki.source.docker "docker_logs" {
  host       = "unix:///var/run/docker.sock"
  targets    = discovery.docker.containers.targets
  forward_to = [loki.relabel.docker_labels.receiver]
}
```
- Читает логи из файлов Docker контейнеров
- Использует метаданные контейнеров для добавления меток

#### Relabeling (добавление меток)
```river
loki.relabel "docker_labels" {
  rule {
    source_labels = ["__meta_docker_container_label_com_docker_compose_service"]
    target_label  = "service"
  }
  rule {
    source_labels = ["__meta_docker_container_name"]
    regex         = "/(.*)"
    target_label  = "container"
    replacement   = "${1}"
  }
  // ... другие правила
}
```
- Извлекает метки из Docker метаданных:
  - `service` - имя сервиса из docker-compose
  - `container` - имя контейнера
  - `project` - имя проекта
  - `job` - всегда "docker"

#### Обработка логов (парсинг TSKV)
```river
loki.process "docker_logs" {
  // Парсит JSON обертку Docker
  stage.json {
    expressions = {
      docker_log = "log",
      stream     = "stream",
    }
  }

  // Извлекает поля из TSKV формата
  stage.regex {
    expression = ` level='(?P<level>[^']+)'`
    source     = "docker_log"
  }
  stage.labels {
    values = {
      level = "level",
    }
  }
  // ... аналогично для path, trace_id, request_id, logger, event, method, status_code
}
```
- Парсит JSON обертку Docker, извлекает поле `log`
- Использует регулярные выражения для извлечения полей из TSKV формата
- Добавляет извлеченные поля как **labels** в Loki:
  - `level` - уровень логирования (info, warning, error)
  - `path` - URL path запроса
  - `trace_id` - ID для трейсинга
  - `request_id` - ID запроса
  - `logger` - имя логгера
  - `event` - тип события
  - `method` - HTTP метод
  - `status_code` - HTTP статус код

### 4. Отправка в Loki

```river
loki.write "loki" {
  endpoint {
    url = "http://loki:3100/loki/api/v1/push"
  }
}
```
- Alloy отправляет обработанные логи в Loki через HTTP API
- Каждая запись содержит:
  - **Labels** (метки) - для фильтрации и группировки
  - **Timestamp** - временная метка
  - **Log line** - сам текст лога

### 5. Хранение в Loki

- Loki индексирует логи по labels
- Хранит логи в chunks на диске
- Retention: 7 дней (168 часов) по умолчанию
- Каждая уникальная комбинация labels создает отдельный stream

**Пример stream:**
```
{
  "job": "docker",
  "service": "experiment-service",
  "container": "experiment-service",
  "level": "info",
  "path": "/api/v1/health"
}
```

### 6. Grafana получает логи из Loki

#### Настройка datasource
```yaml
# infrastructure/logging/grafana/provisioning/datasources/loki.yml
datasources:
  - name: Loki
    type: loki
    url: http://loki:3100
    isDefault: true
```
- Grafana автоматически подключается к Loki
- Использует LogQL для запросов

#### Запросы в Grafana Explore
- Пользователь вводит LogQL запрос: `{service="experiment-service"}`
- Grafana отправляет запрос в Loki через HTTP API
- Loki возвращает логи, соответствующие запросу
- Grafana отображает логи в интерфейсе

**Примеры запросов:**
```logql
# Все логи сервиса
{service="experiment-service"}

# Логи с ошибками
{service="experiment-service", level="error"}

# Логи конкретного trace
{trace_id="550e8400-e29b-41d4-a716-446655440000"}

# Логи по path
{path="/api/v1/users"}
```

## Важные особенности

### 1. Labels vs Fields
- **Labels** - используются для фильтрации и индексации (быстрый поиск)
- **Fields** - содержимое лога (медленный поиск по тексту)
- Все важные поля (level, path, trace_id) извлекаются как labels

### 2. Производительность
- Фильтрация по labels очень быстрая
- Поиск по содержимому логов медленнее
- Рекомендуется использовать labels для фильтрации

### 3. Сеть
- Все компоненты в одной Docker сети: `experiment-network`
- Alloy читает логи через Docker socket
- Alloy → Loki: HTTP внутри сети
- Grafana → Loki: HTTP внутри сети

### 4. Масштабирование
- Alloy автоматически обнаруживает новые контейнеры
- Не требует перезапуска при добавлении сервисов
- Loki масштабируется горизонтально (в production)

## Диагностика

### Проверка потока логов

1. **Проверка генерации логов:**
   ```bash
   docker logs experiment-service --tail 10
   ```

2. **Проверка Alloy:**
   ```bash
   docker logs alloy --tail 20
   curl http://localhost:12345/metrics | grep loki_source_docker
   ```

3. **Проверка Loki:**
   ```bash
   curl http://localhost:3100/ready
   curl http://localhost:3100/loki/api/v1/labels
   ```

4. **Проверка Grafana:**
   - Откройте http://localhost:3001
   - Explore → Loki
   - Запрос: `{job="docker"}`

## Схема компонентов

```
┌─────────────────┐
│  Backend/Frontend│
│     Сервисы      │
│  (Python/Node.js)│
└────────┬─────────┘
         │ stdout/stderr
         ↓
┌─────────────────┐
│ Docker Container │
│  (experiment-    │
│   service, etc.) │
└────────┬─────────┘
         │ /var/lib/docker/containers/
         ↓
┌─────────────────┐
│  Grafana Alloy  │
│  - Discovery    │
│  - Collection   │
│  - Relabeling   │
│  - Processing   │
└────────┬─────────┘
         │ HTTP POST /loki/api/v1/push
         ↓
┌─────────────────┐
│      Loki       │
│  (хранилище)    │
└────────┬─────────┘
         │ HTTP API /loki/api/v1/query
         ↓
┌─────────────────┐
│     Grafana     │
│  (веб-интерфейс)│
└─────────────────┘
```

## Конфигурационные файлы

- **Alloy**: `infrastructure/logging/alloy.river`
- **Loki**: `infrastructure/logging/loki-config.yml`
- **Grafana datasource**: `infrastructure/logging/grafana/provisioning/datasources/loki.yml`
- **Docker Compose**: `docker-compose.yml`, `docker-compose.override.yml`
