# Неделя 12: OpenAPI/Swagger - API документация и спецификации

## Цели недели
- Понять важность API документации
- Изучить OpenAPI спецификацию
- Научиться автоматически генерировать документацию
- Освоить Swagger UI и ReDoc
- Создать полную документацию для API
- Изучить best practices документирования API

## Теория

### Зачем нужна API документация?

**Проблема без документации:**
```
Frontend разработчик → "Какой endpoint для получения пользователя?"
Backend разработчик → "Напиши мне, я скажу"
Frontend разработчик → "А какие поля в ответе?"
Backend разработчик → "Попробуй и посмотри"
Frontend разработчик → "А что если пользователь не найден?"
Backend разработчик → "Я не помню, посмотрю в коде..."
```

**С документацией:**
```
Frontend разработчик → Открывает Swagger UI → Видит все endpoints, схемы, примеры
→ Самостоятельно интегрируется → Задаёт вопросы только по бизнес-логике
```

### Преимущества хорошей документации:

1. **Ускорение разработки** - Frontend не ждёт ответов от Backend
2. **Меньше ошибок** - Видны все возможные ответы и ошибки
3. **Проще онбординг** - Новые разработчики быстро понимают API
4. **Автоматическая валидация** - OpenAPI spec может валидировать запросы/ответы
5. **Генерация клиентов** - Автоматическая генерация SDK
6. **Testing** - Документация как источник правды для тестов

### Что такое OpenAPI?

**OpenAPI (ранее Swagger)** - спецификация для описания REST API.

**Версии:**
- OpenAPI 2.0 (Swagger 2.0) - старый формат
- OpenAPI 3.0 - современный стандарт
- OpenAPI 3.1 - последняя версия

**Формат:**
- YAML или JSON
- Машиночитаемый (можно генерировать код)
- Человекочитаемый (Swagger UI визуализация)

### Структура OpenAPI спецификации

```yaml
openapi: 3.0.3
info:
  title: User API
  description: API для управления пользователями
  version: 1.0.0
  contact:
    name: API Support
    email: support@example.com

servers:
  - url: http://localhost:8080/api/v1
    description: Development server
  - url: https://api.example.com/v1
    description: Production server

paths:
  /users:
    get:
      summary: Получить список пользователей
      tags:
        - Users
      parameters:
        - name: page
          in: query
          schema:
            type: integer
            default: 1
      responses:
        '200':
          description: Список пользователей
          content:
            application/json:
              schema:
                type: object
                properties:
                  users:
                    type: array
                    items:
                      $ref: '#/components/schemas/User'

components:
  schemas:
    User:
      type: object
      properties:
        id:
          type: integer
          format: int64
        username:
          type: string
        email:
          type: string
          format: email
      required:
        - id
        - username
        - email

  securitySchemes:
    bearerAuth:
      type: http
      scheme: bearer
      bearerFormat: JWT
```

## Генерация документации в aiohttp

### Подход 1: Автоматическая генерация из кода

**aiohttp-swagger3** - автоматическая генерация OpenAPI из route handlers.

**Установка:**
```bash
pip install aiohttp-swagger3
```

**Настройка:**
```python
# src/app.py
from aiohttp import web
from aiohttp_swagger3 import SwaggerDocs, SwaggerUiSettings

app = web.Application()

# Создаем Swagger документацию
swagger = SwaggerDocs(
    app,
    title="User API",
    version="1.0.0",
    swagger_ui_settings=SwaggerUiSettings(path="/docs")
)

# Регистрируем routes с аннотациями типов
from typing import List
from pydantic import BaseModel

class User(BaseModel):
    id: int
    username: str
    email: str

class UserCreate(BaseModel):
    username: str
    email: str
    password: str

# Endpoint с автоматической документацией
@swagger.routes.get("/api/v1/users")
async def get_users(request: web.Request) -> web.Response:
    """
    Получить список пользователей

    ---
    tags:
      - Users
    summary: Получить список пользователей
    description: Возвращает список всех пользователей с пагинацией
    parameters:
      - name: page
        in: query
        schema:
          type: integer
          default: 1
      - name: page_size
        in: query
        schema:
          type: integer
          default: 20
    responses:
      '200':
        description: Список пользователей
        content:
          application/json:
            schema:
              type: object
              properties:
                users:
                  type: array
                  items:
                    $ref: '#/components/schemas/User'
                total:
                  type: integer
                page:
                  type: integer
    """
    page = int(request.query.get("page", 1))
    page_size = int(request.query.get("page_size", 20))

    users = await get_users_from_db(page, page_size)

    return web.json_response({
        "users": [{"id": u.id, "username": u.username, "email": u.email} for u in users],
        "total": await count_users(),
        "page": page
    })


@swagger.routes.post("/api/v1/users")
async def create_user(request: web.Request, body: UserCreate) -> web.Response:
    """
    Создать нового пользователя

    ---
    tags:
      - Users
    summary: Создать пользователя
    requestBody:
      required: true
      content:
        application/json:
          schema:
            $ref: '#/components/schemas/UserCreate'
    responses:
      '201':
        description: Пользователь создан
        content:
          application/json:
            schema:
              $ref: '#/components/schemas/User'
      '400':
        description: Ошибка валидации
        content:
          application/json:
            schema:
              $ref: '#/components/schemas/Error'
    """
    user = await create_user_in_db(body.dict())
    return web.json_response({
        "id": user.id,
        "username": user.username,
        "email": user.email
    }, status=201)


# Регистрируем схемы
swagger.add_schema("User", User.schema())
swagger.add_schema("UserCreate", UserCreate.schema())
```

### Подход 2: Ручная спецификация

**aiohttp-swagger** - использование готовой OpenAPI спецификации.

```python
# src/app.py
from aiohttp import web
from aiohttp_swagger import setup_swagger

app = web.Application()

# Load OpenAPI spec
import yaml
with open("openapi.yaml") as f:
    spec = yaml.safe_load(f)

# Настройка Swagger UI
setup_swagger(
    app,
    swagger_url="/api-docs",
    swagger_info=spec["info"],
    swagger_path=spec["paths"]
)
```

### Подход 3: aiohttp + apispec (рекомендуется)

**apispec** - библиотека для генерации OpenAPI спецификаций.

**Установка:**
```bash
pip install apispec aiohttp-apispec
```

**Использование:**
```python
# src/app.py
from aiohttp import web
from aiohttp_apispec import setup_aiohttp_apispec, validation_middleware, SwaggerFileHandler
from marshmallow import Schema, fields

app = web.Application(middlewares=[validation_middleware])

# Pydantic схемы
from pydantic import BaseModel

class UserSchema(BaseModel):
    id: int
    username: str
    email: str

    class Config:
        schema_extra = {
            "example": {
                "id": 1,
                "username": "john_doe",
                "email": "john@example.com"
            }
        }


# Настройка apispec
setup_aiohttp_apispec(
    app=app,
    title="User API",
    version="1.0.0",
    url="/api-docs/swagger.json",
    swagger_path="/docs",
    static_path="/static/swagger"
)

# Handlers
from aiohttp_apispec import docs, use_kwargs, marshal_with
from marshmallow import Schema, fields

class UserResponseSchema(Schema):
    id = fields.Int()
    username = fields.Str()
    email = fields.Str()


@docs(
    tags=["Users"],
    summary="Get users list",
    description="Get paginated list of users",
    parameters=[
        {
            "name": "page",
            "in": "query",
            "schema": {"type": "integer", "default": 1}
        }
    ],
    responses={
        200: {
            "description": "Success response",
            "schema": UserResponseSchema(many=True)
        }
    }
)
@use_kwargs({"page": fields.Int(missing=1)})
@marshal_with(UserResponseSchema(many=True))
async def get_users(request: web.Request, **kwargs):
    page = kwargs["page"]
    users = await get_users_from_db(page)
    return users
```

## Pydantic для валидации и документации

### Pydantic Models как схемы OpenAPI

Pydantic автоматически генерирует JSON Schema, которую можно использовать в OpenAPI.

```python
from pydantic import BaseModel, Field, EmailStr, HttpUrl
from typing import List, Optional
from datetime import datetime

class UserBase(BaseModel):
    """Базовая модель пользователя."""
    username: str = Field(..., min_length=3, max_length=50, description="Имя пользователя")
    email: EmailStr = Field(..., description="Email адрес")

class UserCreate(UserBase):
    """Модель для создания пользователя."""
    password: str = Field(..., min_length=8, description="Пароль (мин. 8 символов)")

class UserUpdate(BaseModel):
    """Модель для обновления пользователя."""
    username: Optional[str] = Field(None, min_length=3, max_length=50)
    email: Optional[EmailStr] = None

class User(UserBase):
    """Модель пользователя."""
    id: int = Field(..., description="ID пользователя")
    created_at: datetime = Field(..., description="Дата создания")
    is_active: bool = Field(True, description="Активен ли пользователь")

    class Config:
        schema_extra = {
            "example": {
                "id": 1,
                "username": "john_doe",
                "email": "john@example.com",
                "created_at": "2024-01-01T00:00:00Z",
                "is_active": True
            }
        }

class UserListResponse(BaseModel):
    """Ответ со списком пользователей."""
    users: List[User] = Field(..., description="Список пользователей")
    total: int = Field(..., description="Общее количество")
    page: int = Field(..., description="Номер страницы")
    page_size: int = Field(..., description="Размер страницы")

class ErrorResponse(BaseModel):
    """Модель ошибки."""
    error: str = Field(..., description="Тип ошибки")
    message: str = Field(..., description="Сообщение об ошибке")
    details: Optional[dict] = Field(None, description="Дополнительные детали")
```

### Использование в handlers

```python
from aiohttp import web
from aiohttp_swagger3 import SwaggerDocs

swagger = SwaggerDocs(app)

@swagger.routes.post("/api/v1/users", summary="Create user")
async def create_user(request: web.Request, body: UserCreate) -> web.Response:
    """Создать пользователя."""
    user = await create_user_in_db(body.dict())
    return web.json_response(User.from_orm(user).dict(), status=201)

@swagger.routes.get("/api/v1/users", summary="Get users")
async def get_users(
    request: web.Request,
    page: int = 1,
    page_size: int = 20
) -> UserListResponse:
    """Получить список пользователей."""
    users = await get_users_from_db(page, page_size)
    total = await count_users()

    return UserListResponse(
        users=[User.from_orm(u) for u in users],
        total=total,
        page=page,
        page_size=page_size
    )
```

## Swagger UI и ReDoc

### Swagger UI

**Swagger UI** - интерактивная документация с возможностью тестирования API.

**Особенности:**
- Визуализация всех endpoints
- Попробовать API прямо в браузере
- Автоматическая валидация запросов
- Примеры запросов/ответов

**Доступ:**
```
http://localhost:8080/docs
```

### ReDoc

**ReDoc** - альтернативная визуализация OpenAPI спецификации.

**Особенности:**
- Красивый трёхколоночный дизайн
- Лучше для чтения
- Меньше интерактивности

**Настройка:**
```python
from aiohttp_swagger3 import ReDocSettings

swagger = SwaggerDocs(
    app,
    redoc_ui_settings=ReDocSettings(path="/redoc")
)
```

## Best Practices

### 1. Описывайте все endpoints

```python
@swagger.routes.get("/api/v1/users/{user_id}")
async def get_user(request: web.Request, user_id: int) -> web.Response:
    """
    Получить пользователя по ID

    ---
    tags:
      - Users
    summary: Получить пользователя
    description: Возвращает информацию о конкретном пользователе
    parameters:
      - name: user_id
        in: path
        required: true
        schema:
          type: integer
        description: ID пользователя
    responses:
      '200':
        description: Пользователь найден
        content:
          application/json:
            schema:
              $ref: '#/components/schemas/User'
      '404':
        description: Пользователь не найден
        content:
          application/json:
            schema:
              $ref: '#/components/schemas/Error'
    """
    ...
```

### 2. Используйте примеры

```python
class User(BaseModel):
    id: int
    username: str
    email: str

    class Config:
        schema_extra = {
            "example": {
                "id": 1,
                "username": "john_doe",
                "email": "john@example.com"
            }
        }
```

### 3. Описывайте ошибки

```python
responses:
  '400':
    description: Ошибка валидации
    content:
      application/json:
        schema:
          $ref: '#/components/schemas/ValidationError'
        examples:
          invalid_email:
            value:
              error: "ValidationError"
              message: "Invalid email format"
              fields:
                email: "Invalid email address"
          missing_field:
            value:
              error: "ValidationError"
              message: "Required field missing"
              fields:
                username: "Field required"
```

### 4. Добавляйте теги для организации

```python
tags:
  - Users
  - Posts
  - Comments
  - Auth
```

### 5. Используйте компоненты для переиспользования

```yaml
components:
  schemas:
    User:
      type: object
      properties:
        id:
          type: integer
        username:
          type: string

  parameters:
    PageParam:
      name: page
      in: query
      schema:
        type: integer
        default: 1

  responses:
    NotFound:
      description: Resource not found
      content:
        application/json:
          schema:
            $ref: '#/components/schemas/Error'
```

### 6. Описывайте авторизацию

```yaml
components:
  securitySchemes:
    bearerAuth:
      type: http
      scheme: bearer
      bearerFormat: JWT
      description: JWT токен в формате "Bearer {token}"

paths:
  /users:
    get:
      security:
        - bearerAuth: []
```

### 7. Версионирование API

```yaml
servers:
  - url: https://api.example.com/v1
    description: API version 1
  - url: https://api.example.com/v2
    description: API version 2
```

## Валидация запросов

OpenAPI spec можно использовать для автоматической валидации.

```python
from aiohttp_swagger3 import SwaggerDocs, RequestValidationError

swagger = SwaggerDocs(
    app,
    validate=True,  # Включить валидацию
    validate_response=True  # Валидировать ответы
)

@swagger.routes.post("/api/v1/users")
async def create_user(request: web.Request, body: UserCreate) -> web.Response:
    """Создать пользователя."""
    # body уже валидирован согласно схеме!
    user = await create_user_in_db(body.dict())
    return web.json_response(User.from_orm(user).dict())

# Обработка ошибок валидации
async def validation_error_handler(request: web.Request, error: RequestValidationError):
    return web.json_response({
        "error": "ValidationError",
        "message": "Invalid request data",
        "details": error.errors()
    }, status=400)

app.middlewares.append(validation_error_handler)
```

## Генерация клиентов

### OpenAPI Generator

**openapi-generator** - генерация клиентских библиотек из OpenAPI spec.

```bash
# Установка
npm install @openapi-generator-plus/cli -g

# Генерация TypeScript клиента
openapi-generator-plus \
  --input openapi.yaml \
  --output ./client \
  --generator typescript-axios

# Генерация Python клиента
openapi-generator-plus \
  --input openapi.yaml \
  --output ./client \
  --generator python
```

### Использование сгенерированного клиента

```typescript
// TypeScript
import { UsersApi } from './generated/api';

const api = new UsersApi({
  basePath: 'https://api.example.com/v1',
  accessToken: 'your-token'
});

const users = await api.getUsers({ page: 1 });
```

## Дополнительные материалы

### Документация
- [OpenAPI Specification](https://swagger.io/specification/)
- [aiohttp-swagger3](https://github.com/hh-h/aiohttp-swagger3)
- [Pydantic Documentation](https://docs.pydantic.dev/)
- [JSON Schema](https://json-schema.org/)

### Статьи
- [OpenAPI Best Practices](https://swagger.io/resources/articles/adopting-an-api-first-approach/)
- [API Documentation Guide](https://idratherbewriting.com/learnapidoc/)
- [OpenAPI 3.0 Tutorial](https://swagger.io/docs/specification/about/)

### Инструменты
- [Swagger Editor](https://editor.swagger.io/)
- [OpenAPI Generator](https://openapi-generator.tech/)
- [Stoplight Studio](https://stoplight.io/studio/)

### Видео
- [OpenAPI Tutorial](https://www.youtube.com/watch?v=6kwmU4kqPCE)
- [API Documentation Best Practices](https://www.youtube.com/watch?v=QUnX3x9gm2c)

## Следующая неделя

На [Неделе 13](../week-13/README.md) изучим API versioning, CORS, Rate limiting и интеграцию с внешними сервисами! 🚀

---

**Удачи с API документацией! 📚**

