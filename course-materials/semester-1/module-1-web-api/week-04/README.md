# Неделя 4: JWT аутентификация и защищенные endpoints

## Цели недели
- Понять принципы аутентификации и авторизации
- Освоить JWT (JSON Web Tokens) для stateless аутентификации
- Научиться безопасно хранить пароли (hashing + salt)
- Реализовать регистрацию и вход пользователей
- Создать защищенные endpoints с проверкой токенов
- Узнать про refresh tokens и best practices безопасности

## Теория

### Аутентификация vs Авторизация

**Аутентификация (Authentication)** - проверка личности пользователя
*"Кто ты?"*

**Авторизация (Authorization)** - проверка прав доступа
*"Что ты можешь делать?"*

```
┌─────────────┐
│   Client    │
└──────┬──────┘
       │ 1. Login (username + password)
       ▼
┌─────────────────────────────────┐
│         Server                  │
│  2. Verify credentials          │
│  3. Generate JWT token          │
└──────┬──────────────────────────┘
       │ 4. Return JWT token
       ▼
┌─────────────┐
│   Client    │ Store token
└──────┬──────┘
       │ 5. Request with token in header
       │    Authorization: Bearer <token>
       ▼
┌─────────────────────────────────┐
│         Server                  │
│  6. Verify token                │
│  7. Check permissions           │
│  8. Return protected resource   │
└─────────────────────────────────┘
```

### Что такое JWT?

**JWT (JSON Web Token)** - открытый стандарт (RFC 7519) для безопасной передачи информации между сторонами в виде JSON объекта.

**Структура JWT:**
```
header.payload.signature
```

**Пример:**
```
eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiIxMjM0NTY3ODkwIiwibmFtZSI6IkpvaG4gRG9lIiwiaWF0IjoxNTE2MjM5MDIyfQ.SflKxwRJSMeKKF2QT4fwpMeJf36POk6yJV_adQssw5c
```

**1. Header (алгоритм и тип токена):**
```json
{
  "alg": "HS256",
  "typ": "JWT"
}
```

**2. Payload (данные пользователя):**
```json
{
  "sub": "1234567890",
  "name": "John Doe",
  "iat": 1516239022,
  "exp": 1516242622
}
```

**3. Signature (подпись для проверки целостности):**
```
HMACSHA256(
  base64UrlEncode(header) + "." +
  base64UrlEncode(payload),
  secret
)
```

**Стандартные claims (поля):**
- `sub` (subject) - идентификатор пользователя
- `iat` (issued at) - время создания токена
- `exp` (expiration) - время истечения токена
- `iss` (issuer) - кто выдал токен
- `aud` (audience) - для кого токен

**Преимущества JWT:**
- ✅ Stateless - сервер не хранит сессии
- ✅ Масштабируемость - токен содержит все данные
- ✅ Кросс-доменность - работает между разными сервисами
- ✅ Мобильная совместимость

**Недостатки JWT:**
- ❌ Нельзя отозвать до истечения срока (нужен blacklist)
- ❌ Размер токена больше, чем session ID
- ❌ Нельзя обновить данные в токене (нужен refresh)

### Безопасное хранение паролей

**❌ НИКОГДА:**
```python
# ПЛОХО - хранение пароля в открытом виде
password = "mypassword123"
user.password = password  # ОПАСНО!
```

**✅ ПРАВИЛЬНО:**
```python
# Хеширование с солью
from passlib.context import CryptContext

pwd_context = CryptContext(schemes=["bcrypt"], deprecated="auto")

# Хеширование пароля
hashed_password = pwd_context.hash("mypassword123")
# $2b$12$EixZaYVK1fsbw1ZfbX3OXePaWxn96p36WQoeG6Lruj3vjPGga31lW

# Проверка пароля
is_valid = pwd_context.verify("mypassword123", hashed_password)
```

**bcrypt** - это алгоритм хеширования с:
- Автоматической солью (salt)
- Настраиваемой сложностью (work factor)
- Защитой от брутфорса (медленный алгоритм)

## Реализация на aiohttp

### 1. Установка зависимостей

```bash
pip install python-jose[cryptography] passlib[bcrypt] python-multipart
```

### 2. Конфигурация

```python
# src/config.py
from datetime import timedelta

class Settings:
    # JWT settings
    SECRET_KEY = "your-secret-key-keep-it-secret"  # В проде из env!
    ALGORITHM = "HS256"
    ACCESS_TOKEN_EXPIRE_MINUTES = 30
    REFRESH_TOKEN_EXPIRE_DAYS = 7

    # Password hashing
    PWD_SCHEMES = ["bcrypt"]
    PWD_DEPRECATED = "auto"


settings = Settings()
```

**ВАЖНО:** В продакшене `SECRET_KEY` должен быть:
```bash
# Генерация secure secret key
openssl rand -hex 32
# или
python -c "import secrets; print(secrets.token_hex(32))"
```

### 3. Утилиты для работы с JWT

```python
# src/auth/jwt.py
from datetime import datetime, timedelta
from typing import Optional
from jose import JWTError, jwt
from passlib.context import CryptContext

from src.config import settings


# Контекст для хеширования паролей
pwd_context = CryptContext(
    schemes=[settings.PWD_SCHEMES],
    deprecated=settings.PWD_DEPRECATED
)


def verify_password(plain_password: str, hashed_password: str) -> bool:
    """Проверка пароля."""
    return pwd_context.verify(plain_password, hashed_password)


def get_password_hash(password: str) -> str:
    """Хеширование пароля."""
    return pwd_context.hash(password)


def create_access_token(
    data: dict,
    expires_delta: Optional[timedelta] = None
) -> str:
    """Создание JWT access token."""
    to_encode = data.copy()

    if expires_delta:
        expire = datetime.utcnow() + expires_delta
    else:
        expire = datetime.utcnow() + timedelta(
            minutes=settings.ACCESS_TOKEN_EXPIRE_MINUTES
        )

    to_encode.update({
        "exp": expire,
        "iat": datetime.utcnow(),
        "type": "access"
    })

    encoded_jwt = jwt.encode(
        to_encode,
        settings.SECRET_KEY,
        algorithm=settings.ALGORITHM
    )
    return encoded_jwt


def create_refresh_token(data: dict) -> str:
    """Создание JWT refresh token."""
    to_encode = data.copy()
    expire = datetime.utcnow() + timedelta(
        days=settings.REFRESH_TOKEN_EXPIRE_DAYS
    )

    to_encode.update({
        "exp": expire,
        "iat": datetime.utcnow(),
        "type": "refresh"
    })

    encoded_jwt = jwt.encode(
        to_encode,
        settings.SECRET_KEY,
        algorithm=settings.ALGORITHM
    )
    return encoded_jwt


def decode_token(token: str) -> dict:
    """Декодирование и валидация JWT токена."""
    try:
        payload = jwt.decode(
            token,
            settings.SECRET_KEY,
            algorithms=[settings.ALGORITHM]
        )
        return payload
    except JWTError as e:
        raise ValueError(f"Invalid token: {e}")
```

### 4. SQL схема и queries для пользователей

**SQL схема (создание таблицы):**
```python
# src/database.py (добавить в create_tables)
async def create_tables():
    """Создать таблицы."""
    pool = get_db_pool()

    async with pool.acquire() as conn:
        # Создание таблицы users
        await conn.execute("""
            CREATE TABLE IF NOT EXISTS users (
                id SERIAL PRIMARY KEY,
                username VARCHAR(50) UNIQUE NOT NULL,
                email VARCHAR(100) UNIQUE NOT NULL,
                hashed_password VARCHAR(255) NOT NULL,
                is_active BOOLEAN DEFAULT TRUE NOT NULL,
                is_superuser BOOLEAN DEFAULT FALSE NOT NULL,
                created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP NOT NULL,
                updated_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP NOT NULL
            );
        """)

        # Индексы
        await conn.execute("CREATE INDEX IF NOT EXISTS idx_users_username ON users(username);")
        await conn.execute("CREATE INDEX IF NOT EXISTS idx_users_email ON users(email);")
        await conn.execute("CREATE INDEX IF NOT EXISTS idx_users_is_active ON users(is_active);")
```

**queries/users.py:**
```python
# src/queries/users.py
"""SQL запросы для работы с пользователями."""
from typing import Optional, Dict, Any
import asyncpg


async def get_user_by_id(
    conn: asyncpg.Connection,
    user_id: int
) -> Optional[Dict[str, Any]]:
    """Получить пользователя по ID."""
    row = await conn.fetchrow("""
        SELECT id, username, email, hashed_password, is_active, is_superuser,
               created_at, updated_at
        FROM users
        WHERE id = $1
    """, user_id)

    return dict(row) if row else None


async def get_user_by_username(
    conn: asyncpg.Connection,
    username: str
) -> Optional[Dict[str, Any]]:
    """Получить пользователя по username."""
    row = await conn.fetchrow("""
        SELECT id, username, email, hashed_password, is_active, is_superuser,
               created_at, updated_at
        FROM users
        WHERE username = $1
    """, username)

    return dict(row) if row else None


async def get_user_by_email(
    conn: asyncpg.Connection,
    email: str
) -> Optional[Dict[str, Any]]:
    """Получить пользователя по email."""
    row = await conn.fetchrow("""
        SELECT id, username, email, hashed_password, is_active, is_superuser,
               created_at, updated_at
        FROM users
        WHERE email = $1
    """, email)

    return dict(row) if row else None


async def create_user(
    conn: asyncpg.Connection,
    username: str,
    email: str,
    hashed_password: str
) -> Dict[str, Any]:
    """Создать нового пользователя."""
    row = await conn.fetchrow("""
        INSERT INTO users (username, email, hashed_password)
        VALUES ($1, $2, $3)
        RETURNING id, username, email, hashed_password, is_active, is_superuser,
                  created_at, updated_at
    """, username, email, hashed_password)

    return dict(row)
```

### 5. Pydantic схемы

```python
# src/schemas/auth.py
from pydantic import BaseModel, EmailStr, Field, validator
from typing import Optional
from datetime import datetime


class UserRegister(BaseModel):
    """Схема для регистрации пользователя."""
    username: str = Field(..., min_length=3, max_length=50)
    email: EmailStr
    password: str = Field(..., min_length=8, max_length=100)

    @validator('password')
    def password_strength(cls, v):
        """Проверка сложности пароля."""
        if not any(char.isdigit() for char in v):
            raise ValueError('Password must contain at least one digit')
        if not any(char.isupper() for char in v):
            raise ValueError('Password must contain at least one uppercase letter')
        if not any(char.islower() for char in v):
            raise ValueError('Password must contain at least one lowercase letter')
        return v

    @validator('username')
    def username_alphanumeric(cls, v):
        """Проверка username."""
        if not v.replace('_', '').replace('-', '').isalnum():
            raise ValueError('Username must be alphanumeric')
        return v


class UserLogin(BaseModel):
    """Схема для входа пользователя."""
    username: str
    password: str


class Token(BaseModel):
    """Схема токена."""
    access_token: str
    refresh_token: str
    token_type: str = "bearer"


class TokenPayload(BaseModel):
    """Данные из JWT токена."""
    sub: int  # user_id
    exp: datetime
    iat: datetime
    type: str


class UserResponse(BaseModel):
    """Схема ответа с данными пользователя."""
    id: int
    username: str
    email: str
    is_active: bool
    created_at: datetime

    class Config:
        from_attributes = True
```

### 6. Middleware для аутентификации

```python
# src/auth/middleware.py
from aiohttp import web
from jose import JWTError
from src.auth.jwt import decode_token
from src.database import get_db_pool
from src.queries.users import get_user_by_id


async def get_current_user(request: web.Request) -> dict:
    """Получение текущего пользователя из токена."""
    auth_header = request.headers.get('Authorization')

    if not auth_header:
        raise web.HTTPUnauthorized(
            reason="Missing Authorization header"
        )

    try:
        scheme, token = auth_header.split()
        if scheme.lower() != 'bearer':
            raise web.HTTPUnauthorized(
                reason="Invalid authentication scheme"
            )
    except ValueError:
        raise web.HTTPUnauthorized(
            reason="Invalid Authorization header format"
        )

    try:
        payload = decode_token(token)
    except ValueError as e:
        raise web.HTTPUnauthorized(reason=str(e))

    # Проверяем тип токена
    if payload.get("type") != "access":
        raise web.HTTPUnauthorized(
            reason="Invalid token type"
        )

    user_id = payload.get("sub")
    if user_id is None:
        raise web.HTTPUnauthorized(
            reason="Invalid token payload"
        )

    # Получаем пользователя из БД
    pool = get_db_pool()
    async with pool.acquire() as conn:
        user = await get_user_by_id(conn, user_id)

    if user is None:
        raise web.HTTPUnauthorized(
            reason="User not found"
        )

    if not user['is_active']:
        raise web.HTTPUnauthorized(
            reason="Inactive user"
        )

    return user


def require_auth(handler):
    """Декоратор для защиты endpoint'ов."""
    async def middleware(request: web.Request):
        user = await get_current_user(request)
        request['user'] = user
        return await handler(request)

    return middleware
```

### 7. Handlers для аутентификации

```python
# src/handlers/auth.py
from aiohttp import web
import asyncpg

from src.auth.jwt import (
    get_password_hash,
    verify_password,
    create_access_token,
    create_refresh_token,
    decode_token
)
from src.database import get_db_pool
from src.queries.users import (
    create_user,
    get_user_by_username,
    get_user_by_email
)
from src.schemas.auth import UserRegister, UserLogin, Token, UserResponse


async def register(request: web.Request) -> web.Response:
    """Регистрация нового пользователя."""
    try:
        data = await request.json()
        user_data = UserRegister(**data)
    except Exception as e:
        raise web.HTTPBadRequest(reason=f"Invalid data: {e}")

    # Хешируем пароль
    hashed_password = get_password_hash(user_data.password)

    # Создаем пользователя
    pool = get_db_pool()
    try:
        async with pool.acquire() as conn:
            async with conn.transaction():
                # Проверяем уникальность username и email
                existing_username = await get_user_by_username(conn, user_data.username)
                if existing_username:
                    raise web.HTTPConflict(
                        reason="Username already exists"
                    )

                existing_email = await get_user_by_email(conn, user_data.email)
                if existing_email:
                    raise web.HTTPConflict(
                        reason="Email already exists"
                    )

                new_user = await create_user(
                    conn,
                    username=user_data.username,
                    email=user_data.email,
                    hashed_password=hashed_password
                )
    except asyncpg.UniqueViolationError:
        raise web.HTTPConflict(
            reason="Username or email already exists"
        )

    # Создаем токены
    access_token = create_access_token({"sub": new_user['id']})
    refresh_token = create_refresh_token({"sub": new_user['id']})

    return web.json_response(
        {
            "user": UserResponse(**new_user).dict(),
            "tokens": Token(
                access_token=access_token,
                refresh_token=refresh_token
            ).dict()
        },
        status=201
    )


async def login(request: web.Request) -> web.Response:
    """Вход пользователя."""
    try:
        data = await request.json()
        credentials = UserLogin(**data)
    except Exception as e:
        raise web.HTTPBadRequest(reason=f"Invalid data: {e}")

    # Ищем пользователя
    pool = get_db_pool()
    async with pool.acquire() as conn:
        user = await get_user_by_username(conn, credentials.username)

    if not user:
        raise web.HTTPUnauthorized(
            reason="Incorrect username or password"
        )

    # Проверяем пароль
    if not verify_password(credentials.password, user['hashed_password']):
        raise web.HTTPUnauthorized(
            reason="Incorrect username or password"
        )

    if not user['is_active']:
        raise web.HTTPUnauthorized(reason="Inactive user")

    # Создаем токены
    access_token = create_access_token({"sub": user['id']})
    refresh_token = create_refresh_token({"sub": user['id']})

    return web.json_response(
        Token(
            access_token=access_token,
            refresh_token=refresh_token
        ).dict()
    )


async def refresh(request: web.Request) -> web.Response:
    """Обновление access token через refresh token."""
    try:
        data = await request.json()
        refresh_token = data.get("refresh_token")

        if not refresh_token:
            raise web.HTTPBadRequest(reason="Missing refresh_token")

        payload = decode_token(refresh_token)

        # Проверяем тип токена
        if payload.get("type") != "refresh":
            raise web.HTTPBadRequest(reason="Invalid token type")

        user_id = payload.get("sub")

        # Проверяем существование пользователя
        pool = get_db_pool()
        async with pool.acquire() as conn:
            user = await get_user_by_id(conn, user_id)

        if not user or not user['is_active']:
            raise web.HTTPUnauthorized(reason="Invalid user")

        # Создаем новые токены
        new_access_token = create_access_token({"sub": user_id})
        new_refresh_token = create_refresh_token({"sub": user_id})

        return web.json_response(
            Token(
                access_token=new_access_token,
                refresh_token=new_refresh_token
            ).dict()
        )

    except ValueError as e:
        raise web.HTTPUnauthorized(reason=str(e))


async def me(request: web.Request) -> web.Response:
    """Получение информации о текущем пользователе."""
    user = request['user']  # Добавлен middleware
    # Убираем hashed_password из ответа
    user_response = {k: v for k, v in user.items() if k != 'hashed_password'}
    return web.json_response(
        UserResponse(**user_response).dict()
    )


async def logout(request: web.Request) -> web.Response:
    """Выход пользователя (stateless, просто возвращаем 200)."""
    # В stateless архитектуре клиент просто удаляет токен
    # Для полноценного logout нужен token blacklist
    return web.json_response({"message": "Logged out successfully"})
```

### 8. Роутинг

```python
# src/routes.py
from aiohttp import web
from src.handlers import auth
from src.auth.middleware import require_auth


def setup_routes(app: web.Application):
    # Public routes
    app.router.add_post('/api/auth/register', auth.register)
    app.router.add_post('/api/auth/login', auth.login)
    app.router.add_post('/api/auth/refresh', auth.refresh)

    # Protected routes
    app.router.add_get('/api/auth/me', require_auth(auth.me))
    app.router.add_post('/api/auth/logout', require_auth(auth.logout))
```

## Защищенные endpoints

### Пример защищенного handler'а

```python
# src/handlers/todos.py
from aiohttp import web
from src.auth.middleware import require_auth


@require_auth
async def get_my_todos(request: web.Request) -> web.Response:
    """Получение TODO текущего пользователя."""
    user = request['user']

    # Теперь мы знаем, кто делает запрос
    async with async_session() as session:
        result = await session.execute(
            select(Todo).where(Todo.user_id == user.id)
        )
        todos = result.scalars().all()

    return web.json_response([
        {"id": todo.id, "title": todo.title, "completed": todo.completed}
        for todo in todos
    ])


@require_auth
async def create_todo(request: web.Request) -> web.Response:
    """Создание TODO для текущего пользователя."""
    user = request['user']
    data = await request.json()

    async with async_session() as session:
        todo = Todo(
            title=data['title'],
            user_id=user.id  # Автоматически привязываем к пользователю
        )
        session.add(todo)
        await session.commit()
        await session.refresh(todo)

    return web.json_response(
        {"id": todo.id, "title": todo.title},
        status=201
    )
```

## Тестирование аутентификации

```python
# tests/test_auth.py
import pytest
from src.auth.jwt import create_access_token


@pytest.mark.asyncio
async def test_register_success(client):
    """Тест успешной регистрации."""
    response = await client.post('/api/auth/register', json={
        "username": "testuser",
        "email": "test@example.com",
        "password": "SecurePass123"
    })

    assert response.status == 201
    data = await response.json()
    assert 'user' in data
    assert 'tokens' in data
    assert data['user']['username'] == 'testuser'
    assert 'access_token' in data['tokens']
    assert 'refresh_token' in data['tokens']


@pytest.mark.asyncio
async def test_register_duplicate_username(client, test_user):
    """Тест регистрации с существующим username."""
    response = await client.post('/api/auth/register', json={
        "username": test_user.username,
        "email": "another@example.com",
        "password": "SecurePass123"
    })

    assert response.status == 409  # Conflict


@pytest.mark.asyncio
async def test_login_success(client, test_user):
    """Тест успешного входа."""
    response = await client.post('/api/auth/login', json={
        "username": test_user.username,
        "password": "testpassword"
    })

    assert response.status == 200
    data = await response.json()
    assert 'access_token' in data
    assert 'refresh_token' in data
    assert data['token_type'] == 'bearer'


@pytest.mark.asyncio
async def test_login_wrong_password(client, test_user):
    """Тест входа с неверным паролем."""
    response = await client.post('/api/auth/login', json={
        "username": test_user.username,
        "password": "wrongpassword"
    })

    assert response.status == 401


@pytest.mark.asyncio
async def test_get_me_with_token(client, test_user):
    """Тест получения информации о текущем пользователе."""
    # Создаем токен
    token = create_access_token({"sub": test_user.id})

    # Делаем запрос с токеном
    response = await client.get(
        '/api/auth/me',
        headers={'Authorization': f'Bearer {token}'}
    )

    assert response.status == 200
    data = await response.json()
    assert data['id'] == test_user.id
    assert data['username'] == test_user.username


@pytest.mark.asyncio
async def test_get_me_without_token(client):
    """Тест доступа к защищенному endpoint без токена."""
    response = await client.get('/api/auth/me')
    assert response.status == 401


@pytest.mark.asyncio
async def test_get_me_invalid_token(client):
    """Тест доступа с невалидным токеном."""
    response = await client.get(
        '/api/auth/me',
        headers={'Authorization': 'Bearer invalid_token'}
    )
    assert response.status == 401


@pytest.mark.asyncio
async def test_refresh_token(client, test_user):
    """Тест обновления access token."""
    from src.auth.jwt import create_refresh_token

    refresh_token = create_refresh_token({"sub": test_user.id})

    response = await client.post('/api/auth/refresh', json={
        "refresh_token": refresh_token
    })

    assert response.status == 200
    data = await response.json()
    assert 'access_token' in data
    assert 'refresh_token' in data
```

## Security Best Practices

### 1. Хранение токенов на клиенте

**❌ ПЛОХО:**
```javascript
// НЕ храните токены в localStorage - уязвимо к XSS!
localStorage.setItem('token', token);
```

**✅ ХОРОШО:**
```javascript
// HttpOnly cookies - защита от XSS
// Или в памяти приложения
let accessToken = null;
```

### 2. HTTPS обязателен

```python
# В продакшене ВСЕГДА используйте HTTPS
if not request.secure and settings.ENVIRONMENT == "production":
    raise web.HTTPForbidden(reason="HTTPS required")
```

### 3. Rate limiting для login

```python
# Защита от брутфорса
from aiohttp_ratelimiter import RateLimiter

limiter = RateLimiter(
    storage_uri="redis://localhost:6379",
    max_requests=5,  # 5 попыток
    time_window=60   # за минуту
)

@limiter.limit()
async def login(request):
    ...
```

### 4. Token Blacklist

```python
# Для logout или отзыва токенов
async def logout(request: web.Request):
    user = request['user']
    token = extract_token(request)

    # Добавляем токен в blacklist в Redis
    await redis.setex(
        f"blacklist:{token}",
        settings.ACCESS_TOKEN_EXPIRE_MINUTES * 60,
        "1"
    )

    return web.json_response({"message": "Logged out"})


async def get_current_user(request):
    token = extract_token(request)

    # Проверяем blacklist
    if await redis.exists(f"blacklist:{token}"):
        raise web.HTTPUnauthorized(reason="Token revoked")

    ...
```

### 5. Валидация входных данных

```python
class UserRegister(BaseModel):
    username: str = Field(..., min_length=3, max_length=50)
    email: EmailStr  # Автоматическая валидация email
    password: str = Field(..., min_length=8)

    @validator('password')
    def validate_password(cls, v):
        if not re.search(r'[A-Z]', v):
            raise ValueError('Password must contain uppercase')
        if not re.search(r'[a-z]', v):
            raise ValueError('Password must contain lowercase')
        if not re.search(r'\d', v):
            raise ValueError('Password must contain digit')
        return v
```

## Дополнительные материалы

### Статьи
- [JWT Introduction](https://jwt.io/introduction)
- [OWASP Authentication Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/Authentication_Cheat_Sheet.html)
- [Password Storage Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/Password_Storage_Cheat_Sheet.html)

### Инструменты
- [jwt.io](https://jwt.io/) - декодирование JWT токенов
- [Python-JOSE](https://python-jose.readthedocs.io/) - документация
- [Passlib](https://passlib.readthedocs.io/) - документация

### Видео
- [JWT Authentication Tutorial](https://www.youtube.com/watch?v=7Q17ubqLfaM)
- [Secure Password Storage](https://www.youtube.com/watch?v=8ZtInClXe1Q)

## Следующая неделя

На [Неделе 5](../../module-2-testing/week-05/README.md) изучим Unit тестирование с pytest! 🚀

---

**Удачи с аутентификацией! 🔐**

