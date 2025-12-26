# Техническое задание: Auth Service

## 1. Общее описание

**Auth Service** — микросервис аутентификации и авторизации для платформы отслеживания экспериментов (Experiment Tracking Platform). Сервис предоставляет REST API для управления пользователями, аутентификации через JWT токены и управления сессиями.

### 1.1. Назначение
- Регистрация и аутентификация пользователей
- Управление JWT токенами (access и refresh)
- Управление паролями пользователей
- Предоставление информации о текущем пользователе

### 1.2. Технологический стек
- **Язык:** Python 3.14+
- **Фреймворк:** aiohttp (асинхронный веб-фреймворк)
- **База данных:** PostgreSQL
- **ORM/Query Builder:** asyncpg (нативный async драйвер)
- **Валидация:** Pydantic
- **Хеширование паролей:** bcrypt
- **JWT:** PyJWT
- **Миграции:** SQL файлы с ручным управлением версиями

## 2. Функциональные требования

### 2.1. Регистрация пользователя

**Эндпоинт:** `POST /auth/register`

**Описание:** Создание нового пользователя в системе.

**Входные данные:**
```json
{
  "username": "string",  // 3-50 символов
  "email": "string",      // валидный email
  "password": "string"    // 8-100 символов
}
```

**Выходные данные (201 Created):**
```json
{
  "user": {
    "id": "uuid",
    "username": "string",
    "email": "string",
    "password_change_required": false
  },
  "access_token": "string",
  "refresh_token": "string"
}
```

**Ошибки:**
- `400 Bad Request` — невалидные входные данные
- `409 Conflict` — пользователь с таким username или email уже существует
- `500 Internal Server Error` — внутренняя ошибка сервера

**Бизнес-логика:**
- Пароль хешируется с использованием bcrypt (12 раундов)
- Email должен быть уникальным
- Username должен быть уникальным
- При создании `password_change_required` устанавливается в `false`

### 2.2. Вход пользователя

**Эндпоинт:** `POST /auth/login`

**Описание:** Аутентификация пользователя по username и password.

**Входные данные:**
```json
{
  "username": "string",
  "password": "string"
}
```

**Выходные данные (200 OK):**
```json
{
  "user": {
    "id": "uuid",
    "username": "string",
    "email": "string",
    "password_change_required": boolean
  },
  "access_token": "string",
  "refresh_token": "string"
}
```

**Ошибки:**
- `400 Bad Request` — невалидные входные данные
- `401 Unauthorized` — неверные учетные данные
- `500 Internal Server Error` — внутренняя ошибка сервера

**Бизнес-логика:**
- Проверка username и password
- Сравнение пароля с хешем в БД
- Генерация access и refresh токенов
- Access token TTL: 15 минут (900 секунд)
- Refresh token TTL: 14 дней (1209600 секунд)

### 2.3. Обновление токена

**Эндпоинт:** `POST /auth/refresh`

**Описание:** Обновление access токена с использованием refresh токена.

**Входные данные:**
```json
{
  "refresh_token": "string"
}
```

**Выходные данные (200 OK):**
```json
{
  "access_token": "string",
  "refresh_token": "string"
}
```

**Ошибки:**
- `400 Bad Request` — невалидные входные данные
- `401 Unauthorized` — невалидный или истекший refresh token
- `500 Internal Server Error` — внутренняя ошибка сервера

**Бизнес-логика:**
- Валидация refresh token
- Генерация новой пары токенов (access и refresh)
- Refresh token ротируется при каждом обновлении

### 2.4. Получение информации о текущем пользователе

**Эндпоинт:** `GET /auth/me`

**Описание:** Получение информации о текущем аутентифицированном пользователе.

**Заголовки:**
```
Authorization: Bearer <access_token>
```

**Выходные данные (200 OK):**
```json
{
  "id": "uuid",
  "username": "string",
  "email": "string",
  "password_change_required": boolean
}
```

**Ошибки:**
- `401 Unauthorized` — отсутствует или невалидный токен
- `500 Internal Server Error` — внутренняя ошибка сервера

**Бизнес-логика:**
- Извлечение токена из заголовка Authorization
- Валидация и декодирование токена
- Поиск пользователя в БД по данным из токена

### 2.5. Выход пользователя

**Эндпоинт:** `POST /auth/logout`

**Описание:** Выход пользователя из системы (placeholder).

**Заголовки:**
```
Authorization: Bearer <access_token>
```

**Выходные данные (200 OK):**
```json
{
  "ok": true
}
```

**Примечание:** В текущей реализации это placeholder. В production необходимо реализовать инвалидацию токенов через blacklist или удаление из БД.

### 2.6. Смена пароля

**Эндпоинт:** `POST /auth/change-password`

**Описание:** Смена пароля текущего пользователя.

**Заголовки:**
```
Authorization: Bearer <access_token>
```

**Входные данные:**
```json
{
  "old_password": "string",
  "new_password": "string"  // 8-100 символов
}
```

**Выходные данные (200 OK):**
```json
{
  "id": "uuid",
  "username": "string",
  "email": "string",
  "password_change_required": false
}
```

**Ошибки:**
- `400 Bad Request` — невалидные входные данные
- `401 Unauthorized` — отсутствует токен или неверный старый пароль
- `500 Internal Server Error` — внутренняя ошибка сервера

**Бизнес-логика:**
- Проверка старого пароля
- Хеширование нового пароля
- Обновление пароля в БД
- Установка `password_change_required` в `false` после успешной смены

### 2.7. Health Check

**Эндпоинт:** `GET /health`

**Описание:** Проверка работоспособности сервиса.

**Выходные данные (200 OK):**
```json
{
  "status": "ok",
  "service": "auth-service",
  "env": "development"
}
```

## 3. Нефункциональные требования

### 3.1. Производительность
- Время ответа на запросы аутентификации: < 200ms (p95)
- Поддержка до 1000 одновременных подключений
- Использование connection pooling для БД (размер пула: 20 соединений)

### 3.2. Безопасность
- Пароли хранятся в виде bcrypt хешей (12 раундов)
- JWT токены подписываются с использованием HS256
- CORS настроен для разрешенных origins
- Валидация всех входных данных через Pydantic
- Защита от SQL-инъекций через параметризованные запросы

### 3.3. Надежность
- Автоматическое применение миграций при старте сервиса
- Retry логика для подключения к БД (5 попыток с задержкой 2 секунды)
- Обработка ошибок с понятными сообщениями
- Логирование всех ошибок

### 3.4. Масштабируемость
- Stateless архитектура (токены в JWT, без сессий в БД)
- Горизонтальное масштабирование без изменений
- Поддержка нескольких инстансов сервиса

## 4. Архитектура

### 4.1. Структура проекта
```
auth-service/
├── src/
│   └── auth_service/
│       ├── api/
│       │   └── routes/
│       │       └── auth.py          # API роуты
│       ├── core/
│       │   └── exceptions.py        # Кастомные исключения
│       ├── db/
│       │   └── pool.py               # Connection pool
│       ├── domain/
│       │   ├── dto.py                # Data Transfer Objects
│       │   └── models.py             # Доменные модели
│       ├── repositories/
│       │   └── users.py              # Репозиторий пользователей
│       ├── services/
│       │   └── auth.py               # Бизнес-логика аутентификации
│       ├── settings.py               # Конфигурация
│       └── main.py                   # Точка входа
├── migrations/                        # SQL миграции
├── bin/                               # Утилиты
│   ├── migrate.py                     # Скрипт миграций
│   └── generate_password_hash.py     # Утилита для хеширования
├── tests/                             # Тесты
├── Dockerfile                         # Docker образ
├── pyproject.toml                     # Зависимости
└── env.example                        # Пример переменных окружения
```

### 4.2. Слои архитектуры
1. **API Layer** (`api/routes/`) — обработка HTTP запросов, валидация входных данных
2. **Service Layer** (`services/`) — бизнес-логика аутентификации
3. **Repository Layer** (`repositories/`) — работа с БД
4. **Domain Layer** (`domain/`) — модели и DTO

### 4.3. База данных

**Таблица: users**
```sql
CREATE TABLE users (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    username VARCHAR(50) UNIQUE NOT NULL,
    email VARCHAR(255) UNIQUE NOT NULL,
    hashed_password TEXT NOT NULL,
    password_change_required BOOLEAN NOT NULL DEFAULT FALSE,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);
```

**Таблица: schema_migrations**
```sql
CREATE TABLE schema_migrations (
    version TEXT PRIMARY KEY,
    checksum TEXT NOT NULL,
    applied_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);
```

## 5. Конфигурация

### 5.1. Переменные окружения

| Переменная | Описание | Значение по умолчанию |
|------------|----------|----------------------|
| `ENV` | Окружение (development/staging/production) | `development` |
| `APP_NAME` | Имя приложения | `auth-service` |
| `HOST` | Хост для прослушивания | `0.0.0.0` |
| `PORT` | Порт для прослушивания | `8001` |
| `DATABASE_URL` | URL подключения к PostgreSQL | `postgresql://postgres:postgres@localhost:5432/auth_db` |
| `DB_POOL_SIZE` | Размер пула соединений | `20` |
| `JWT_SECRET` | Секретный ключ для JWT | `dev-secret-key-change-in-production` |
| `JWT_ALGORITHM` | Алгоритм подписи JWT | `HS256` |
| `ACCESS_TOKEN_TTL_SEC` | TTL access токена (секунды) | `900` (15 минут) |
| `REFRESH_TOKEN_TTL_SEC` | TTL refresh токена (секунды) | `1209600` (14 дней) |
| `BCRYPT_ROUNDS` | Количество раундов bcrypt | `12` |
| `CORS_ALLOWED_ORIGINS` | Разрешенные CORS origins (через запятую) | `http://localhost:3000,http://localhost:8080` |

### 5.2. Пользователь по умолчанию

После применения миграций создается пользователь:
- **Username:** `admin`
- **Password:** `admin123`
- **Email:** `admin@example.com`
- **Password Change Required:** `true`

⚠️ **ВАЖНО:** В production обязательно смените пароль администратора!

## 6. API Контракты

### 6.1. Формат ошибок

Все ошибки возвращаются в следующем формате:
```json
{
  "error": "Описание ошибки"
}
```

### 6.2. Коды состояния HTTP

- `200 OK` — успешный запрос
- `201 Created` — ресурс создан
- `400 Bad Request` — невалидные входные данные
- `401 Unauthorized` — требуется аутентификация или неверные учетные данные
- `404 Not Found` — ресурс не найден
- `409 Conflict` — конфликт (например, пользователь уже существует)
- `500 Internal Server Error` — внутренняя ошибка сервера

### 6.3. JWT Токены

**Access Token Payload:**
```json
{
  "sub": "user_id",
  "username": "string",
  "email": "string",
  "exp": 1234567890,
  "iat": 1234567890,
  "type": "access"
}
```

**Refresh Token Payload:**
```json
{
  "sub": "user_id",
  "exp": 1234567890,
  "iat": 1234567890,
  "type": "refresh"
}
```

## 7. Миграции

### 7.1. Применение миграций

Миграции применяются автоматически при старте сервиса. Также можно применить вручную:

```bash
poetry run python bin/migrate.py --database-url "postgresql://postgres:postgres@localhost:5432/auth_db"
```

### 7.2. Версионирование миграций

Миграции хранятся в папке `migrations/` с именами вида `001_initial_schema.sql`, `002_add_indexes.sql` и т.д.

Версии отслеживаются в таблице `schema_migrations` с проверкой checksum для предотвращения изменений примененных миграций.

## 8. Тестирование

### 8.1. Типы тестов
- Unit тесты для бизнес-логики
- Integration тесты для API эндпоинтов
- Тесты безопасности (валидация, хеширование паролей)

### 8.2. Покрытие
- Минимальное покрытие: 80%
- Критичные пути (аутентификация, регистрация): 100%

## 9. Мониторинг и логирование

### 9.1. Логирование
- Все ошибки логируются с контекстом
- Уровни логирования: DEBUG, INFO, WARNING, ERROR
- Логирование запросов (без чувствительных данных)

### 9.2. Метрики
- Количество запросов по эндпоинтам
- Время ответа (latency)
- Количество ошибок
- Количество активных пользователей

## 10. Развертывание

### 10.1. Docker
```bash
docker build -t auth-service:dev .
docker run -p 8001:8001 --env-file .env auth-service:dev
```

### 10.2. Локальная разработка
```bash
cd projects/backend/services/auth-service
poetry install
cp env.example .env
poetry run python -m auth_service.main
```

## 11. Интеграция с другими сервисами

### 11.1. Auth Proxy (BFF)
Auth Service интегрируется с Auth Proxy (BFF паттерн), который:
- Управляет HttpOnly cookies для токенов
- Проксирует запросы к Auth Service
- Добавляет токены в заголовки для других сервисов

### 11.2. Experiment Service
Experiment Service использует токены из Auth Service для авторизации запросов через API Gateway или Auth Proxy.

## 12. Будущие улучшения

- [ ] Инвалидация токенов при logout (blacklist или БД)
- [ ] Rate limiting для защиты от brute force
- [ ] Двухфакторная аутентификация (2FA)
- [ ] OAuth2 интеграция (Google, GitHub)
- [ ] Роли и права доступа (RBAC)
- [ ] История смены паролей
- [ ] Уведомления о смене пароля
- [ ] Аудит логи (кто, когда, что)

