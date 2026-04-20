# RFC-0001: config-service

**Статус:** Draft

**Автор(ы):** Ivan Khokhlov

**Дата:** 17-04-2026

**Тип:** New Service

---

## Резюме

Сервис для управления runtime-конфигурацией системы: поднять таймауты походов в другие сервисы или БД, включить/выключить feature flag, перевести функциональность в режим деградации при инциденте. Сейчас такие изменения требуют правки конфигов и перезапуска сервисов, что приводит к простою и не даёт оперативно реагировать на проблемы в production.

Основной сценарий — **incident response**: дежурный через админский UI меняет конфигурацию, сервисы-потребители подхватывают изменения за несколько секунд без рестарта.

---

## Мотивация

Почему нужен новый сервис? Какие проблемы в текущей архитектуре он решает?

- **Проблема 1: Отсутствие runtime конфигурации** - Сейчас для изменения параметров поведения системы (таймауты, feature flags, пороги деградации) требуется изменить конфигурационные файлы и перезапустить сервис, что приводит к простою и невозможности оперативного реагирования на проблемы в production.

- **Проблема 2: Декентрализованное управление конфигурацией** - Конфигурация разбросана по разным сервисам в переменных окружения, ConfigMap'ах Kubernetes или локальных файлах, что усложняет аудит, версионирование и обеспечение консистентности.

- **Ограничения текущего подхода** - Нельзя оперативно реагировать на инциденты (например, увеличить таймаут к проблемному внешнему API или отключить проблемную функциональность без кодовых изменений и деплоя).

---

## Дизайн сервиса

### Основные компоненты

| Компонент | Описание | Технология |
|-----------|---------|-----------|
| API | REST эндпоинты (JSON over HTTP) | Python aiohttp |
| Хранилище конфигурации | Таблицы `configs` и `config_history` | PostgreSQL |
| Валидатор | Проверка `value` против JSON-Schema на каждый запись | jsonschema (Python) |

### API

Основные эндпоинты для управления конфигурацией:

```
POST   /api/v1/config                 # Создание конфигурации (поддерживает Idempotency-Key и ?dry_run=true)
GET    /api/v1/config?service=&project=&config_type=&is_active=&limit=50&cursor=  # Список с фильтрами и cursor-pagination
GET    /api/v1/config/{config_id}     # Получение конкретной конфигурации по ID
PATCH  /api/v1/config/{config_id}     # Частичное обновление (change_reason обязателен; поддерживает ?dry_run=true)
DELETE /api/v1/config/{config_id}     # Soft delete (выставляет deleted_at; не возвращается в list/bulk)
POST   /api/v1/config/{config_id}/activate   # Активация конфигурации
POST   /api/v1/config/{config_id}/deactivate # Деактивация конфигурации
POST   /api/v1/config/{config_id}/rollback   # Откат к указанной версии из config_history
GET    /api/v1/config/{config_id}/history    # История изменений конфигурации

GET    /api/v1/schemas                       # Список активных схем по всем config_type
GET    /api/v1/schemas/{config_type}         # Текущая активная схема для типа
GET    /api/v1/schemas/{config_type}/history # История версий схемы
PUT    /api/v1/schemas/{config_type}         # Загрузка новой версии схемы (с compat-check)
```

#### Семантика API

**Пагинация и фильтры (`GET /api/v1/config`):**
- Фильтры: `service`, `project`, `config_type`, `is_active` — все optional, AND-композиция.
- Cursor-based pagination: параметр `limit` (по умолчанию 50, максимум 500) и `cursor` (opaque-строка из предыдущего ответа).
- Ответ: `{ "items": [...], "next_cursor": "..." | null }`.
- Offset-pagination не используется — cursor лучше ведёт себя на больших наборах и при параллельных изменениях.

**Семантика `PATCH`:**
- Top-level поля (`description`, `metadata`, `is_active`) мержатся: поле, не переданное в запросе, остаётся прежним.
- Поле `value` **всегда заменяется целиком** — JSONB merge-patch по вложенным объектам не делается, чтобы исключить сюрпризы с частично применённой конфигурацией.
- Поле `change_reason` **обязательно** для `PATCH`, `DELETE`, `activate`, `deactivate` и `rollback`. Без него — `400 Bad Request`. Требование аудита: всегда известно, почему изменили.
- Для `POST` (создание) `change_reason` опционален: если не передан — записывается `'Initial creation'` в `config_history` для version=1.

**Rollback (`POST /api/v1/config/{id}/rollback`):**

```json
// POST /api/v1/config/{id}/rollback
{
  "target_version": 2,
  "change_reason": "INC-1234 — откат к последнему рабочему состоянию"
}

// Response 200 — возвращается обновлённая конфигурация с новым version
```

Сервис читает `config_history` по `(config_id, version=target_version)`, берёт из неё `value`, `metadata`, `is_active` и применяет как обычный `PATCH` с инкрементом `version`. Старые версии остаются в истории — повторный rollback всегда возможен.

**Dry-run (`?dry_run=true`):**
- Поддерживается на `POST /api/v1/config` и `PATCH /api/v1/config/{id}`.
- Запрос проходит все проверки (JSON-Schema, RBAC, бизнес-правила), сервис возвращает предварительный результат, но транзакция откатывается.
- Ответ: `200 OK` с `{"preview": {...}, "dry_run": true}`.
- Используется админским UI для кнопки «Проверить» перед «Создать/Сохранить».

**Оптимистическая блокировка (If-Match):**

Все write-эндпоинты (`PATCH`, `DELETE`, `activate`, `deactivate`, `rollback`) **требуют** заголовок `If-Match: "<version>"`, где `<version>` — последняя известная клиенту версия конфигурации. Сервер:

- При любом `GET` возвращает заголовок `ETag: "<version>"` и поле `version` в теле.
- На write-запрос выполняет `UPDATE ... WHERE id = ? AND version = ? RETURNING *` и атомарно инкрементирует `version`. Если затронуто 0 строк → `412 Precondition Failed` (кто-то обновил запись раньше — клиент обязан перечитать и повторить).
- Отсутствие `If-Match` → `428 Precondition Required`.

Это защищает от потерянных обновлений при параллельной работе нескольких админов и автоматизаций.

**Идемпотентность `POST`:**
- Клиент может передать заголовок `Idempotency-Key: <uuid>` на `POST /api/v1/config`.
- Сервис хранит `(idempotency_key, user_id, request_hash, response_body, response_status)` в таблице `idempotency_keys` с TTL 24 часа.
- При повторном `POST` с тем же ключом от того же пользователя в окне TTL:
  - Если `request_hash` совпадает — возвращается сохранённый ответ без повторного создания.
  - Если не совпадает — `409 Conflict` («ключ уже использован с другим телом»).
- Соответствует паттерну, принятому в experiment-service.

#### Внутренний эндпоинт для сервисов-потребителей

Сервисы платформы используют внутренний эндпоинт для получения набора конфигураций, актуальных для конкретного сервиса и проекта, с поддержкой кэширования через ETag/Last-Modified:

```
GET    /api/v1/configs/bulk?service={service_name}&project={project_id}  # Получение всех конфигураций для сервиса и проекта
```

**Параметры запроса:**
- `service` (обязательно): идентификатор сервиса (например, `experiment-service`, `telemetry-ingest`)
- `project` (опционально): идентификатор проекта, если конфигурации scoped по проектам

**Ответ:**
- JSON-объект вида `{ "configs": { "<key>": <value>, ... } }` где ключ — это имя конфигурации (`configs.key` из БД, например `write_path_enabled` или `http_qos`), а значение — поле `value` соответствующей конфигурации целиком (объект).
- Заголовок `ETag` — хеш всего набора конфигураций.
- Заголовок `Last-Modified` — timestamp самого свежего обновления среди возвращаемых конфигураций.

**Условные запросы:**
Клиент может передавать заголовок `If-None-Match` со значением предыдущего ETag. Если набор конфигураций не изменился, сервис возвращает **304 Not Modified** и пустое тело.

**Пример запроса/ответа:**

```http
GET /api/v1/configs/bulk?service=experiment-service&project=project-a
Accept: application/json
If-None-Match: "W/\"abc123\""
```

*Если конфигурации изменились:*
```http
HTTP/1.1 200 OK
Content-Type: application/json
ETag: "W/\"def456\""
Last-Modified: Thu, 17 Apr 2026 10:30:00 GMT

{
  "configs": {
    "write_path_enabled": { "enabled": true },
    "http_qos": {
      "__default__": { "timeout_ms": 150, "retries": 2 },
      "/v1/verify":  { "timeout_ms": 10000, "retries": 5 }
    }
  }
}
```

*Если конфигурации не изменились:*
```http
HTTP/1.1 304 Not Modified
ETag: "W/\"abc123\""
Last-Modified: Thu, 17 Apr 2026 10:15:00 GMT
```

**Типы конфигурации (поле `config_type`):**
- `feature_flag` — булевые переключатели функциональности; используются в том числе для экстренного отключения и graceful degradation при инцидентах.
- `qos` — параметры качества обслуживания для исходящих вызовов: таймауты и ретраи, сгруппированные по endpoint-ам, с обязательным ключом `__default__` для всех остальных путей. Группировка в одном `value` гарантирует **атомарность** связанных параметров (timeout + retries меняются одной операцией), без отдельного bulk-patch API.

> **Scope:** `rate_limit`, `circuit_breaker` и продуктовые A/B-эксперименты (rollout, сегментация по юзерам/geo) вынесены за рамки этого RFC и будут описаны в отдельных RFC по мере необходимости.

#### Валидация `value` по JSON-Schema

Для каждого `config_type` существует активная JSON-Schema (Draft 2020-12), против которой валидируется поле `value`:

- **На запись** (`POST`/`PATCH`/`rollback`) — strict: нарушение → `422 Unprocessable Entity` с описанием ошибок, запись отклоняется.
- **На чтение** (внутри сервиса — перед отдачей в bulk или single-get) — paranoid: каждая запись валидируется против текущей активной схемы, отдаётся как обычно, но при нарушении инкрементируется метрика `config_read_schema_violations_total{config_type, config_id}`. Это страховка от багов compat-checker'а: расхождения должны быть нулевыми в норме, любая ненулевая метрика — сигнал разбираться.

**Хранение схем:**

Схемы — first-class данные сервиса, лежат в таблице `config_schemas` (см. раздел «Миграции БД»). На каждый `config_type` активна ровно одна версия схемы; история версий сохраняется в той же таблице для аудита и отката.

Начальные схемы для MVP-типов (`feature_flag`, `qos`) засеиваются SQL-миграцией при первом деплое.

Пример схемы для `feature_flag`:

```json
{
  "$schema": "https://json-schema.org/draft/2020-12/schema",
  "type": "object",
  "required": ["enabled"],
  "properties": {
    "enabled": { "type": "boolean" }
  },
  "additionalProperties": false
}
```

Пример схемы для `qos` (per-endpoint + обязательный `__default__`):

```json
{
  "$schema": "https://json-schema.org/draft/2020-12/schema",
  "type": "object",
  "required": ["__default__"],
  "additionalProperties": { "$ref": "#/$defs/qosSettings" },
  "properties": {
    "__default__": { "$ref": "#/$defs/qosSettings" }
  },
  "$defs": {
    "qosSettings": {
      "type": "object",
      "required": ["timeout_ms", "retries"],
      "properties": {
        "timeout_ms": { "type": "integer", "minimum": 1, "maximum": 600000 },
        "retries": { "type": "integer", "minimum": 0, "maximum": 10 }
      },
      "additionalProperties": false
    }
  }
}
```

Пример валидного значения:
```json
{
  "__default__":  { "timeout_ms": 150, "retries": 2 },
  "/v1/verify":   { "timeout_ms": 10000, "retries": 5 },
  "/v1/refresh":  { "timeout_ms": 300, "retries": 1 }
}
```

**Эволюция схем и обратная совместимость:**

Обновление схемы идёт через админский API (`PUT /api/v1/schemas/{config_type}`). Config-service сам решает, применять ли изменение — MVP принципиально поддерживает **только обратно-совместимые** обновления:

1. Сервис сравнивает новую схему с текущей активной (diff JSON-Schema).
2. Если все изменения — additive, создаётся новая версия, становится активной, старая остаётся в истории.
3. Если найдено хотя бы одно breaking-изменение — `422 Unprocessable Entity` с перечнем нарушений, схема НЕ обновляется.

Ожидаемая частота обновлений — раз в несколько месяцев. Ломающие изменения (с миграцией значений и отдельным тулингом) — вне scope MVP и будут описаны отдельным RFC при первой реальной потребности.

**Разрешённые (additive) изменения:**
- Добавление optional-поля.
- Расширение числовых границ (`maximum` выше, `minimum` ниже).
- Расширение `enum` (новые значения).
- Снятие `required` с поля.

**Запрещённые (breaking) изменения:**
- Добавление `required`-поля.
- Ужесточение границ (`maximum` ниже, `minimum` выше).
- Сужение `enum` (удаление значений).
- Удаление или переименование поля.
- Смена `type`.

**Дополнительная защита (sanity check):**

После прохождения compat-check и до применения новой схемы сервис в той же транзакции прогоняет новую схему по всем существующим записям `configs` этого `config_type`. При любом нарушении — транзакция откатывается, схема остаётся в старой версии, возвращается `500` с детализацией. Такой случай считается багом compat-checker'а, а не штатной ситуацией — он означает, что правила compat-проверки пропустили ломающее изменение.

**Новый `config_type`:** первая версия схемы загружается тем же эндпоинтом `PUT /api/v1/schemas/{config_type}` (compat-check не требуется, так как нет существующих записей).

> **Отложено:** автоматический экспорт изменений конфигураций (и схем) в git-репозиторий как YAML-файлов (git-mirror) — для бэкапа, истории и возможности code-review. Вынесено в отдельный RFC.

**Пример 1 — feature flag как kill-switch:**

```json
// POST /api/v1/config
{
  "service_name": "telemetry-ingest",
  "project_id": null,
  "key": "write_path_enabled",
  "config_type": "feature_flag",
  "description": "Kill-switch на приём телеметрии (использовать при инциденте)",
  "value": { "enabled": true },
  "metadata": { "owner": "sre", "ticket": "INC-1234" }
}

// Response 201
{
  "id": "a1b2c3d4-e5f6-7890-g1h2-i3j4k5l6m7n8",
  "service_name": "telemetry-ingest",
  "project_id": null,
  "key": "write_path_enabled",
  "config_type": "feature_flag",
  "description": "Kill-switch на приём телеметрии (использовать при инциденте)",
  "value": { "enabled": true },
  "metadata": { "owner": "sre", "ticket": "INC-1234" },
  "is_active": true,
  "version": 1,
  "created_at": "2026-04-17T10:00:00Z",
  "updated_at": "2026-04-17T10:00:00Z",
  "created_by": "ivan.khokhlov@company.com"
}
```

**Пример 2 — изменение QoS во время инцидента:**

```http
PATCH /api/v1/config/{id}
If-Match: "1"
Content-Type: application/json

{
  "value": {
    "__default__": { "timeout_ms": 150, "retries": 2 },
    "/v1/verify":  { "timeout_ms": 10000, "retries": 5 }
  },
  "change_reason": "Auth API тормозит на /v1/verify, INC-1234 — атомарно поднимаем timeout+retries для этой ручки"
}
```

```http
HTTP/1.1 200 OK
ETag: "2"
Content-Type: application/json

{ ... обновлённая конфигурация с "version": 2 ... }
```

Поскольку `value` заменяется целиком и содержит все endpoint'ы вместе с `__default__`, timeout и retries для конкретной ручки меняются одной атомарной операцией — промежуточное состояние типа «новый timeout, старые retries» клиенту никогда не видно.

### Интеграция с другими сервисами

**Какие сервисы вызывают новый сервис:**
- **auth-service** - проверяет доступ пользователей к изменению конфигурации (RBAC)
- **Все сервисы-platform** (experiment-service, telemetry-ingest, и т.д.) - периодически опрашивают внутренний эндпоинт `/api/v1/configs/bulk` (см. раздел «Распространение изменений» ниже)

**На какие внешние сервисы зависит:**
- **PostgreSQL** - перманентное хранилище всех конфигураций и их истории
- **auth-service** - проверка прав доступа при изменении конфигурации (через HTTP)

### Распространение изменений

Изменения применяются у сервисов-потребителей через **клиентский polling** эндпоинта `/api/v1/configs/bulk`:

- Клиент (SDK) хранит in-process кеш `{ configs, etag }`.
- Раз в `poll_interval` (по умолчанию 1 сек) делает `GET /api/v1/configs/bulk?service=<name>` с заголовком `If-None-Match: <last_etag>`.
- `304 Not Modified` → кеш остался валидным, клиент ничего не делает.
- `200 OK` → клиент атомарно заменяет кеш новым набором конфигураций и публикует событие для подписчиков внутри процесса (колбэки/observable).

**Рекомендации для SDK:**
- Jitter ±25% от `poll_interval`, чтобы несколько инстансов одного сервиса не били в config-service синхронно.
- Exponential backoff при ошибках, но продолжаем отдавать last-known-good из кеша — приложение не должно падать, если config-service недоступен.
- После каждого успешного `200 OK` SDK сохраняет ответ на локальный диск (`/var/cache/config-service/<service>.json`) как fallback-файл с атомарной записью (tmp + rename).

**Cold start и недоступность config-service (fail-open + fallback-файл):**

Приоритет источников конфигурации при старте сервиса-потребителя:

1. **ENV-override** (см. ниже) — высший приоритет, перебивает всё.
2. **config-service bulk-эндпоинт** — первичный источник.
3. **Локальный fallback-файл** (`/var/cache/config-service/<service>.json`) — используется, если config-service недоступен при cold start.
4. **Hardcoded defaults в коде** — последний рубеж; логируется `warning` о том, что работаем на дефолтах.

SDK **не блокирует старт сервиса** при недоступности config-service: применяется лучший доступный источник (fallback-файл → defaults), параллельно продолжается polling. Как только config-service ответит — конфигурация горячо обновляется, событие публикуется подписчикам. Логика: доступность сервиса важнее точности конфигов в момент старта.

**ENV-override (emergency kill-switch для config-service):**

Для каждой записи конфигурации доступна конвенция:

```
CONFIG_OVERRIDE__<SERVICE_NAME>__<KEY>=<json-value>
```

Например:
```
CONFIG_OVERRIDE__AUTH_SERVICE__WRITE_PATH_ENABLED='{"enabled":false}'
CONFIG_OVERRIDE__AUTH_SERVICE__HTTP_QOS='{"__default__":{"timeout_ms":200,"retries":0}}'
```

Если ENV-переменная выставлена, SDK **игнорирует** значение из config-service и использует ENV. Нужна для экстренного отката при каскадном инциденте (config-service перегружен / отдаёт кривой конфиг — админ выставляет ENV на нужных подах через k8s/docker/terraform и делает restart, без участия самого config-service). SDK читает ENV только при старте — перезапуск пода обязателен.

**SLO:**
- Propagation lag p95 < 5 сек (от `updated_at` в БД до применения на последнем инстансе).
- Availability config-service 99.9%; недоступность НЕ должна каскадно ронять потребителей благодаря локальному кешу.

**Ограничения голого polling:**
- Задержка применения ≈ `poll_interval` + время запроса.
- Нагрузка: `N_services × N_instances / poll_interval` RPS на config-service. При 10 сервисах × 3 инстанса × 1 сек = 30 RPS — приемлемо.
- Для мгновенного применения (SLO < 1 сек) в будущем можно добавить push (PostgreSQL `LISTEN/NOTIFY` + SSE-стрим) — но это отдельный RFC.

---

## Реализация

### Структура проекта

```
projects/backend/services/config-service/
├── src/config_service/
│   ├── api/           # REST routes (v1)
│   │   ├── v1/
│   │   │   ├── routes/
│   │   │   │   ├── config.py
│   │   │   │   └── health.py
│   │   │   └── dependencies.py
│   │   └── __init__.py
│   ├── services/      # Бизнес-логика
│   │   ├── config_service.py
│   │   ├── schema_service.py       # управление схемами, compat-check
│   │   └── validation_service.py   # валидация value против активной схемы
│   ├── repositories/  # Слой доступа к данным
│   │   ├── config_repository.py
│   │   ├── history_repository.py
│   │   └── schema_repository.py
│   ├── models/        # Pydantic модели и SQLAlchemy модели
│   │   ├── config.py
│   │   ├── history.py
│   │   ├── schema.py
│   │   └── enums.py
│   └── main.py        # Entry point
├── tests/
│   ├── unit/
│   │   ├── test_config_service.py
│   │   ├── test_cache_service.py
│   │   └── test_validation.py
│   ├── integration/
│   │   └── test_config_api.py
│   └── conftest.py
├── migrations/        # SQL миграции (Alembic)
│   ├── versions/
│   └── env.py
├── openapi/           # OpenAPI 3.1 спецификация
│   └── config-service.yaml
├── pyproject.toml
├── Dockerfile
└── README.md
```

### Порт и переменные окружения

- **Порт:** 8004 (следовать номерации сервисов: auth-8001, experiment-8002, telemetry-ingest-8003)
- **Переменные:**
  - `DATABASE_URL`: PostgreSQL connection string (обязательно)
  - `AUTH_SERVICE_URL`: auth-service endpoint для проверки прав (обязательно)
  - `LOG_LEVEL`: уровень логирования (INFO/DEBUG/WARNING/ERROR)

### Миграции БД

Начальная схема для хранения конфигураций, scoped по сервису и проекту:

```sql
-- Основная таблица конфигураций
CREATE TABLE configs (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  service_name VARCHAR(100) NOT NULL,   -- например, 'experiment-service', 'telemetry-ingest'
  project_id VARCHAR(100),              -- nullable, если конфигурация не привязана к конкретному проекту
  key VARCHAR(255) NOT NULL,            -- имя конфигурации внутри сервиса/проекта
  config_type VARCHAR(50) NOT NULL,     -- feature_flag, qos
  description TEXT,
  value JSONB NOT NULL,                 -- Значение конфигурации (валидируется по JSON-Schema для config_type)
  metadata JSONB,                       -- Свободные метаданные (owner, ticket, комментарии)
  is_active BOOLEAN DEFAULT FALSE,
  version INTEGER DEFAULT 1,
  created_at TIMESTAMPTZ DEFAULT CURRENT_TIMESTAMP,
  updated_at TIMESTAMPTZ DEFAULT CURRENT_TIMESTAMP,
  created_by VARCHAR(255),              -- email или ID пользователя
  updated_by VARCHAR(255),
  deleted_at TIMESTAMPTZ,               -- soft delete: NULL = активная запись
  deleted_by VARCHAR(255),
  is_critical BOOLEAN DEFAULT FALSE,    -- конфиг с большим блэст-радиусом (kill-switch'и и т.п.); UI требует подтверждения
  is_sensitive BOOLEAN DEFAULT FALSE,   -- value содержит чувствительные данные; redacted в audit-лог и для не-админов
  UNIQUE NULLS NOT DISTINCT (service_name, project_id, key)  -- PG 15+: NULL в project_id трактуется как равный NULL, что блокирует дубликаты для глобальных конфигов
);

-- Хранилище JSON-Schema по config_type (runtime source of truth для валидатора)
CREATE TABLE config_schemas (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  config_type VARCHAR(50) NOT NULL,      -- feature_flag, qos
  version INTEGER NOT NULL,              -- инкрементируется при каждом обновлении
  schema JSONB NOT NULL,                 -- JSON Schema Draft 2020-12
  is_active BOOLEAN DEFAULT FALSE,       -- ровно одна активная версия на config_type
  created_at TIMESTAMPTZ DEFAULT CURRENT_TIMESTAMP,
  created_by VARCHAR(255),
  UNIQUE (config_type, version)
);

-- Гарантирует ровно одну активную версию на каждый config_type
CREATE UNIQUE INDEX idx_config_schemas_active_unique
  ON config_schemas(config_type) WHERE is_active = TRUE;

-- Начальный seed схем MVP-типов (feature_flag, qos) — заполняется миграцией.

-- История изменений конфигураций (для аудита и отката)
CREATE TABLE config_history (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  config_id UUID NOT NULL REFERENCES configs(id) ON DELETE NO ACTION,  -- защищаем историю от потери при случайном hard delete
  service_name VARCHAR(100) NOT NULL,
  project_id VARCHAR(100),
  key VARCHAR(255) NOT NULL,
  config_type VARCHAR(50) NOT NULL,
  description TEXT,
  value JSONB NOT NULL,
  metadata JSONB,
  is_active BOOLEAN,
  version INTEGER,
  deleted_at TIMESTAMPTZ,            -- snapshot значения на момент изменения
  is_critical BOOLEAN,
  is_sensitive BOOLEAN,
  changed_at TIMESTAMPTZ DEFAULT CURRENT_TIMESTAMP,
  changed_by VARCHAR(255),
  source_ip VARCHAR(45),             -- IP клиента (IPv6 max 45 символов)
  user_agent TEXT,                   -- источник: UI / CLI / автоматика
  correlation_id VARCHAR(64),        -- trace-id из OpenTelemetry для связи с логами
  change_reason TEXT                 -- Почему было сделано изменение
);

-- Индексы для быстрого поиска
CREATE INDEX idx_configs_service ON configs(service_name);
CREATE INDEX idx_configs_project ON configs(project_id);
CREATE INDEX idx_configs_key ON configs(key);
CREATE INDEX idx_configs_type ON configs(config_type);
CREATE INDEX idx_configs_active ON configs(is_active) WHERE is_active = TRUE AND deleted_at IS NULL;
CREATE INDEX idx_configs_not_deleted ON configs(deleted_at) WHERE deleted_at IS NULL;
CREATE INDEX idx_config_history_config_id ON config_history(config_id);
CREATE INDEX idx_config_history_changed_at ON config_history(changed_at);

-- Таблица для идемпотентности POST-запросов
CREATE TABLE idempotency_keys (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  idempotency_key VARCHAR(255) NOT NULL,
  user_id VARCHAR(255) NOT NULL,
  request_hash VARCHAR(64) NOT NULL,     -- sha256 тела запроса (для проверки совпадения)
  response_body JSONB NOT NULL,
  response_status INTEGER NOT NULL,
  created_at TIMESTAMPTZ DEFAULT CURRENT_TIMESTAMP,
  expires_at TIMESTAMPTZ NOT NULL,       -- created_at + 24h
  UNIQUE (idempotency_key, user_id)
);

CREATE INDEX idx_idempotency_keys_expires ON idempotency_keys(expires_at);
-- Очистка истёкших ключей: периодический cron-job либо pg_cron (`DELETE FROM idempotency_keys WHERE expires_at < NOW()`).

-- Триггер для автоматического обновления updated_at
CREATE OR REPLACE FUNCTION update_updated_at_column()
RETURNS TRIGGER AS $$
BEGIN
  NEW.updated_at = CURRENT_TIMESTAMP;
  RETURN NEW;
END;
$$ language 'plpgsql';

CREATE TRIGGER update_configs_updated_at
BEFORE UPDATE ON configs
FOR EACH ROW EXECUTE PROCEDURE update_updated_at_column();

-- История изменений (config_history) заполняется из Python-репозитория (config_repository)
-- в той же транзакции, что и INSERT/UPDATE на configs. Триггер не используется —
-- change_reason передаётся параметром метода репозитория, что избегает session-level
-- GUC-переменных и делает поток данных явным в коде.
--
-- Репозиторий пишет в config_history на каждую запись, включая INSERT (version=1):
-- начальная запись нужна для полноты аудита и чтобы rollback мог возвращаться к version=1.
-- change_reason для POST опционален (default 'Initial creation');
-- для PATCH, DELETE (soft), activate, deactivate, rollback — обязателен (API возвращает 400, если не передан).
```

---

## Безопасность

### Защита bulk-эндпоинта

Bulk-эндпоинт `GET /api/v1/configs/bulk` используется всеми сервисами-потребителями и содержит полный набор актуальных конфигов. В MVP защита — **сетевая изоляция**: эндпоинт биндится только на internal network (Yandex Cloud VPC / внутренняя сеть docker-compose), не выставляется через ingress / API gateway.

Подделка запроса возможна только при проникновении во внутреннюю сеть — модель угроз уровня ниже, чем у публичных эндпоинтов.

> **Отложено в отдельный RFC:** service-to-service аутентификация (у каждого сервиса-потребителя собственный токен; параметр `?service=<name>` проверяется против токена — несовпадение → `403`). Нужно будет реализовать перед выводом за пределы одной команды или при переходе на multi-tenant.

### RBAC

Используется существующая RBAC-инфраструктура auth-service. Матрица ролей config-service:

| Роль | Права |
|------|-------|
| `config_admin` | Полный доступ: CRUD по всем `service_name`, управление схемами (`PUT /api/v1/schemas/*`), чтение sensitive-значений |
| `config_editor:{service_name}` | CRUD, activate/deactivate/rollback только для конфигов указанного `service_name` |
| `config_viewer` | Только чтение через UI (`GET /api/v1/config*`). Bulk-эндпоинт защищён сетевой изоляцией, отдельная роль не требуется |

На каждый write-запрос config-service вызывает `POST /authorize` у auth-service с `(user_id, action, resource={service_name, config_id})` и ждёт `200 OK`. Несовпадение → `403 Forbidden`.

Обновление схем (`PUT /api/v1/schemas/{config_type}`) доступно только `config_admin`.

### Audit log

Кроме полей `changed_by` / `change_reason`, `config_history` содержит:
- `source_ip` — IP клиента (поддерживается IPv6).
- `user_agent` — источник запроса (UI / CLI / автоматика).
- `correlation_id` — trace-id из OpenTelemetry.

Параллельно каждое изменение пишется в Loki (через Alloy) как structured-JSON с полями `service`, `config_type`, `config_id`, `actor`, `action`, `change_reason`, `is_critical`, `correlation_id`, `source_ip`. Это даёт:
- фильтрацию и дашборды по актору / сервису в Grafana,
- алерты на подозрительные паттерны (например, >10 изменений критичных конфигов за минуту),
- быстрый grep во время инцидентов.

### Критичные конфигурации

Поле `is_critical BOOLEAN DEFAULT FALSE` в `configs`. Используется для kill-switch'ей и переключателей с большим блэст-радиусом.

- Помечается админом вручную при создании или отдельным PATCH.
- При изменении `is_critical=TRUE`-конфига админский UI требует явного подтверждения: checkbox «Я понимаю последствия» + повтор `key` или расширенный `change_reason` (≥20 символов).
- Флаг возвращается в API-ответах, чтобы UI знал, когда показывать предупреждение.

> **Отложено:** 4-eyes approval (согласование от второго админа для изменения критичного конфига) и rate limit на write-операции — оба требуют отдельной инфраструктуры, уходят в backlog.

### Sensitive values

Поле `is_sensitive BOOLEAN DEFAULT FALSE` в `configs`. Указывает, что `value` содержит чувствительные данные.

- В structured-audit-log (Loki) `value` для sensitive-конфигов заменяется на `"***"` перед записью.
- В API-ответах для ролей ниже `config_admin` поле `value` также заменяется на `"***"`.
- В `config_history` `value` хранится как есть (нужно для отката), доступ к нему — только для `config_admin` через явный параметр `?include_sensitive=true`.

> **Отложено в отдельный RFC:** полноценный secrets management. Два варианта на будущее — (а) отдельный **secrets-service** поверх Yandex Lockbox / Vault, либо (б) расширение config-service reference'ами на внешние хранилища (`vault://path/...`), резолвимыми на стороне bulk-эндпоинта. Выбор — когда появится реальная потребность в runtime-секретах.

---

## Тестирование

### Unit tests

- Тестирование бизнес-логики сервисов (config_service, cache_service, validation_service)
- Тестирование репозиториев (запросы к PostgreSQL с использованием моков или in-memory SQLite)
- Мокирование внешних зависимостей (auth-service для проверки прав, Redis)
- Тестирование Pydantic моделей и валидации входных данных
- Тестирование валидации `value` против JSON-Schema для каждого `config_type`

### Integration tests

- E2E сценарии через API: создание конфигурации → чтение → обновление → деактивация → удаление
- Проверка интеграции с auth-service: эндпоинты защищены и требуют соответствующих ролей
- Тестирование обработки одновременных обновлений конфигурации (гонки условий)

### Load tests (опционально, если критично для высоконагруженных систем)

Нагрузка на сервис будет небольшая, несколько десятков RPS. По этому пока не вижу смысла в нагрузочном тестировании

---

## Развёртывание

### Docker Compose (dev)

```yaml
config-service:
  image: config-service:latest
  ports:
    - "8004:8004"
  environment:
    DATABASE_URL: postgresql://user:pass@postgres:5432/config_service_db
    AUTH_SERVICE_URL: http://auth-service:8001
    LOG_LEVEL: INFO
  depends_on:
    - postgres
```

### Миграции в CI/CD

```bash
# При деплое на stage/prod
make config-service-migrate
```

### Мониторинг

- OpenTelemetry экспорт (OTLP) для трассировки запросов и операций
- Логирование в Loki (через Alloy) с уровнем INFO в продакшене
- Метрики в Prometheus: количество запросов, задержки, ошибки валидации, propagation lag
- Графики/алерты в Grafama:
  - Задержки API эндпоинтов (p95 < 100ms)
  - Количество ошибок валидации и авторизации

---

## Альтернативы

Почему выбран этот подход вместо других?

- **Альтернатива 1: Использование только переменных окружения и ConfigMaps** - Описание: Хранить всю конфигурацию в переменных окружения или Kubernetes ConfigMaps. Плюсы: простота реализации, нет внешних зависимостей. Минусы: требует перезапуска сервисов при изменении, сложно управлять условиями и версионированием, нет истории изменений.

- **Альтернатива 2: Сторонние решения (Consul, etcd, Apache Zookeeper)** - Описание: Использовать специализированные системы для distributed конфигурации. Плюсы: готовые решения, поддержка наблюдения за изменениями, сильная консистентность. Минусы: дополнительная сложность в инфраструктуре, требуется изучение новой технологии, потенциальный vendor lock-in.

- **Выбранный подход:** собственный сервис на основе PostgreSQL лучше, потому что:
  1. Использует уже существующую в стеке технологию (PostgreSQL), без дополнительных зависимостей (Redis, RabbitMQ не требуются для MVP)
  2. Позволяет гибко расширять типы конфигурации через JSON-Schema в репозитории, проходя code review на каждый новый тип
  3. Обеспечивает полный контроль над производительностью и оптимизацией
  4. Легко интегрируется с существующими системами мониторинга и алертинга
  5. Обеспечивает историю изменений и аудит из коробки

---

## Известные ограничения / Tech Debt

- **Ограничение 3: Ограничение размера значения** - Поле `value` типа JSONB имеет практический лимит размера (~1GB), но extremely большие конфигурации могут влиять на производительность. Следует документировать рекомендуемый максимальный размер.

---

## План реализации

### Phase 1: MVP (неделя 1)

- [ ] Структура проекта + Dockerfile
- [ ] API базовые эндпоинты (CRUD для конфигураций, фильтры, cursor-pagination, soft delete)
- [ ] Rollback-эндпоинт (`POST /api/v1/config/{id}/rollback`)
- [ ] Dry-run (`?dry_run=true`) для POST/PATCH
- [ ] Idempotency-Key middleware + таблица `idempotency_keys`
- [ ] Audit-поля в `config_history` (`source_ip`, `user_agent`, `correlation_id`) + `is_critical` / `is_sensitive` в `configs`
- [ ] Structured audit-логирование изменений в Loki
- [ ] БД миграции (таблицы `configs`, `config_history`, `config_schemas`, `idempotency_keys`)
- [ ] Seed начальных схем MVP-типов (`feature_flag`, `qos`) через миграцию
- [ ] JSON-Schema валидация `value` на запись (strict) и на чтение (paranoid: метрика `config_read_schema_violations_total`)
- [ ] API управления схемами (`GET/PUT /api/v1/schemas`) с backward-compat checker'ом
- [ ] Unit tests (сервисы, репозитории, валидация, compat-check на additive/breaking, idempotency)

### Phase 2: Интеграция (неделя 2)

- [ ] Интеграция с auth-service: RBAC matrix (`config_admin`, `config_editor:{service}`, `config_viewer`)
- [ ] Redaction sensitive-значений в API-ответах для не-админов и в audit-логе
- [ ] Сетевая изоляция bulk-эндпоинта (конфигурация в Yandex Cloud VPC / docker-compose networks)
- [ ] Integration tests (полные сценарии через API, RBAC, redaction)
- [ ] OpenAPI 3.1 спецификация
- [ ] Health check эндпоинт

### Phase 3: Production (неделя 3)

- [ ] OTLP/Loki конфигурация (трассировка и логирование)
- [ ] Мониторинг/алерты (задержки, ошибки, hit rate кеша)
- [ ] Documentation (руководство по использованию для сервисов-потребителей)
- [ ] Security audit: проверка авторизации, JSON-Schema (отсутствие чувствительных `string`-полей в MVP-типах), корректность redaction в audit-логе

---

## Ссылки

- Related ADR: [docs/adr/XXXX-название.md](../adr/)
- Issue/Epic: [GitHub issue #XXX](https://github.com/)
- Slack thread: (если есть обсуждение)

---

## История изменений

| Дата | Автор | Изменение |
|------|-------|----------|
| 2026-04-17 | [Ivan Khokhlov] | Первая версия |
