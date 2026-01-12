# Логирование с Grafana Loki

Этот проект использует Grafana Loki для централизованного сбора и просмотра логов всех сервисов через веб-интерфейс.

📖 **Подробное описание потока логов**: [docs/logging-flow.md](docs/logging-flow.md) - как логи попадают из сервисов в Grafana

## Быстрый старт

### Запуск стека логирования

Стек логирования находится в отдельном проекте `infrastructure/logging/`:

```bash
make logs-stack-up
```

Или вручную:
```bash
cd infrastructure/logging
docker-compose -f docker-compose.yml up -d
```

Grafana будет доступна на http://localhost:3001 после запуска.

### 2. Доступ к Grafana

- **URL**: http://localhost:3001
- **Логин**: `admin`
- **Пароль**: `admin` (или значение из `GRAFANA_ADMIN_PASSWORD` в `.env`)

### 3. Loki API

Loki API доступен на http://localhost:3100, но это **API сервер**, а не веб-интерфейс.

**Важно**: Для просмотра логов используйте **Grafana** (http://localhost:3001), а не Loki API напрямую.

Доступные эндпоинты Loki API:
- `http://localhost:3100/ready` - проверка готовности
- `http://localhost:3100/metrics` - метрики Prometheus
- `http://localhost:3100/loki/api/v1/labels` - список меток
- `http://localhost:3100/loki/api/v1/query` - запрос логов
- `http://localhost:3100/loki/api/v1/query_range` - запрос логов за период

Пример запроса через API:
```bash
curl "http://localhost:3100/loki/api/v1/labels"
```

### 4. Просмотр логов

1. Откройте Grafana: http://localhost:3001
2. Перейдите в **Explore** (иконка компаса в левом меню)
3. Выберите datasource **Loki**
4. Используйте LogQL запросы для фильтрации логов

## Примеры LogQL запросов

### Все логи конкретного сервиса
```
{service="experiment-service"}
```

### Все логи по имени контейнера
```
{container=~"experiment-.*"}
```

### Только ошибки (используя label level)
```
{level="ERROR"}
```

Или старый способ (поиск по содержимому):
```
{service=~".+"} |= "error" |= "ERROR" |= "Error" |= "exception"
```

### Логи auth-proxy
```
{service="auth-proxy"}
```

### Логи PostgreSQL
```
{container="backend-postgres"}
```

### Фильтрация по URL path
```
{path="/auth/login"}
```

Или с регулярным выражением:
```
{path=~"/api/.*"}
```

### Комбинация фильтров
```
{service="experiment-service", level="ERROR"}
```

Или с path:
```
{service="experiment-service", path="/api/users", level="ERROR"}
```

### Фильтрация по trace_id и request_id
```
{trace_id="550e8400-e29b-41d4-a716-446655440000"}
```

```
{request_id="660e8400-e29b-41d4-a716-446655440001"}
```

### Фильтрация по HTTP методу
```
{method="GET"}
```

```
{method="POST"}
```

### Фильтрация по HTTP статус коду
```
{status_code="200"}
```

```
{status_code=~"4.."}  # Все 4xx ошибки
```

```
{status_code=~"5.."}  # Все 5xx ошибки
```

### Фильтрация по типу события (event)
```
{event="Incoming request"}
```

```
{event="Request completed"}
```

```
{event=~".*error.*"}  # Все события с "error" в названии
```

### Фильтрация по логгеру (модулю)
```
{logger="auth_service.main"}
```

```
{logger=~".*middleware.*"}  # Все логи из middleware
```

### Фильтрация по типу ошибки
```
{error_type="HTTPException"}
```

```
{error_type="ValueError"}
```

### Комплексные примеры фильтрации
```
# Все POST запросы с ошибками
{method="POST", status_code=~"4..|5.."}
```

```
# Все ошибки в auth-service
{service="auth-service", level="ERROR"}
```

```
# Все запросы к конкретному endpoint с определенным статусом
{path="/api/users", status_code="404"}
```

```
# Все логи определенного события в конкретном сервисе
{service="experiment-service", event="experiment_created"}
```

## Компоненты стека

### Loki
- Хранилище логов
- API: http://localhost:3100
- Конфигурация: `infrastructure/logging/loki-config.yml`

### Alloy
- Сборщик логов из Docker контейнеров
- Автоматически обнаруживает контейнеры через Docker socket
- Конфигурация: `infrastructure/logging/alloy.river`

### Grafana
- Веб-интерфейс для визуализации
- URL: http://localhost:3001
- Автоматически настроен datasource Loki

## Управление стеком

```bash
# Запуск
make logs-stack-up

# Остановка
make logs-stack-down

# Перезапуск
make logs-stack-restart

# Просмотр статуса
cd infrastructure/logging && docker-compose -f docker-compose.yml ps
```

## Настройка

### Переменные окружения (в `.env`)

```bash
# Порт Grafana
GRAFANA_PORT=3001

# Порт Loki
LOKI_PORT=3100

# Учетные данные Grafana
GRAFANA_ADMIN_USER=admin
GRAFANA_ADMIN_PASSWORD=admin

# Разрешить анонимный доступ (true/false)
GRAFANA_ANONYMOUS_ENABLED=false
```

### Фильтрация логов в Alloy

По умолчанию Alloy собирает логи только контейнеров проекта `product_dev_course`.

Чтобы собирать логи всех контейнеров, измените фильтр в `infrastructure/logging/alloy.river`:

```river
// Удалите или закомментируйте фильтр по проекту в discovery.docker.containers
```

## Хранение данных

Данные хранятся в Docker volumes:
- `experiment-loki-data` - логи Loki
- `experiment-grafana-data` - настройки и дашборды Grafana

Для очистки данных:
```bash
cd infrastructure/logging && docker-compose -f docker-compose.yml down -v
```

## Troubleshooting

### Логи не появляются в Grafana

1. Проверьте, что Alloy запущен:
   ```bash
   cd infrastructure/logging && docker-compose -f docker-compose.yml ps alloy
   ```

2. Проверьте логи Alloy:
   ```bash
   cd infrastructure/logging && docker-compose -f docker-compose.yml logs alloy
   ```

3. Проверьте, что Loki доступен:
   ```bash
   curl http://localhost:3100/ready
   ```

### Не могу подключиться к Grafana

1. Проверьте, что контейнер запущен:
   ```bash
   cd infrastructure/logging && docker-compose -f docker-compose.yml ps grafana
   ```

2. Проверьте логи:
   ```bash
   cd infrastructure/logging && docker-compose -f docker-compose.yml logs grafana
   ```

3. Убедитесь, что порт 3001 не занят:
   ```bash
   netstat -tuln | grep 3001
   ```

## Дополнительные ресурсы

- [Grafana Loki Documentation](https://grafana.com/docs/loki/latest/)
- [LogQL Query Language](https://grafana.com/docs/loki/latest/logql/)
- [Grafana Alloy Documentation](https://grafana.com/docs/alloy/latest/)
