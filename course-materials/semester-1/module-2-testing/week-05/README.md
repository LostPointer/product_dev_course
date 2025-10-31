# Неделя 5: Unit тестирование с pytest

## Цели недели
- Освоить pytest для написания тестов на Python
- Понять разницу между unit, integration и e2e тестами
- Научиться использовать fixtures для подготовки тестовых данных
- Овладеть mocking для изоляции тестируемого кода
- Применять parametrize для тестирования множества сценариев
- Написать полное покрытие тестами для handlers и бизнес-логики

## Теория

### Пирамида тестирования

```
           ┌─────────┐
          ╱  E2E (UI) ╲
         ╱   Tests     ╲  ← Медленные, хрупкие
        ├───────────────┤     (~10%)
       ╱  Integration   ╲
      ╱     Tests        ╲ ← Средние по скорости
     ├─────────────────────┤   (~30%)
    ╱                       ╲
   ╱     Unit Tests          ╲ ← Быстрые, надежные
  ╱___________________________╲  (~60%)
```

### Виды тестов

**Unit Tests (Юнит-тесты):**
- Тестируют отдельную функцию/метод
- Очень быстрые (миллисекунды)
- Полная изоляция с помощью моков
- Цель: проверить логику

**Integration Tests (Интеграционные):**
- Тестируют взаимодействие компонентов
- Средняя скорость (секунды)
- Реальные зависимости (БД, Redis)
- Цель: проверить интеграцию

**E2E Tests (End-to-End):**
- Тестируют весь flow от UI до БД
- Медленные (минуты)
- Все компоненты реальные
- Цель: проверить пользовательский сценарий

### Что такое pytest?

**pytest** - это мощный фреймворк для тестирования Python приложений.

**Преимущества pytest:**
- 🎯 Простой и понятный синтаксис
- 🔧 Богатая система fixtures
- 📊 Детальный вывод ошибок
- 🔌 Множество плагинов
- ⚡ Параллельное выполнение тестов
- 📝 Автоматическое обнаружение тестов

**Установка:**
```bash
pip install pytest pytest-asyncio pytest-cov pytest-mock
```

## Основы pytest

### 1. Структура тестов

```python
# test_example.py

def test_simple_assertion():
    """Простой тест с assert."""
    result = 2 + 2
    assert result == 4


def test_with_message():
    """Тест с сообщением при ошибке."""
    result = sum([1, 2, 3])
    assert result == 6, f"Expected 6, got {result}"


def test_exceptions():
    """Тест на исключения."""
    import pytest

    with pytest.raises(ZeroDivisionError):
        result = 1 / 0

    with pytest.raises(ValueError, match="invalid literal"):
        int("not a number")
```

### 2. Naming Conventions

```
tests/
├── conftest.py           # Общие fixtures
├── test_auth.py          # Тесты аутентификации
├── test_users.py         # Тесты пользователей
└── test_utils.py         # Тесты утилит
```

**Правила именования:**
- Файлы: `test_*.py` или `*_test.py`
- Функции: `test_*`
- Классы: `Test*`

### 3. Запуск тестов

```bash
# Запуск всех тестов
pytest

# Запуск конкретного файла
pytest tests/test_auth.py

# Запуск конкретного теста
pytest tests/test_auth.py::test_login_success

# Запуск с подробным выводом
pytest -v

# Запуск с показом print()
pytest -s

# Запуск только failed тестов
pytest --lf  # last failed

# Запуск до первой ошибки
pytest -x

# Параллельный запуск (требует pytest-xdist)
pytest -n 4  # 4 процесса
```

## Fixtures - подготовка данных

### Что такое Fixtures?

**Fixture** - это функция, которая подготавливает данные или состояние для тестов.

### Простой Fixture

```python
# conftest.py
import pytest


@pytest.fixture
def sample_user():
    """Создание тестового пользователя."""
    return {
        "id": 1,
        "username": "testuser",
        "email": "test@example.com"
    }


# test_users.py
def test_user_data(sample_user):
    """Тест использует fixture."""
    assert sample_user["username"] == "testuser"
    assert sample_user["email"] == "test@example.com"
```

### Fixture Scopes

```python
import pytest


@pytest.fixture(scope="function")  # По умолчанию - каждый тест
def function_fixture():
    """Создается для каждого теста."""
    print("Setup function")
    yield "function data"
    print("Teardown function")


@pytest.fixture(scope="class")  # Один раз на класс
def class_fixture():
    """Создается один раз для класса."""
    return "class data"


@pytest.fixture(scope="module")  # Один раз на модуль
def module_fixture():
    """Создается один раз для модуля."""
    return "module data"


@pytest.fixture(scope="session")  # Один раз на всю сессию
def session_fixture():
    """Создается один раз для всей сессии тестов."""
    return "session data"
```

### Setup и Teardown

```python
import pytest


@pytest.fixture
def database_connection():
    """Fixture с setup и teardown."""
    # Setup: подготовка
    conn = create_db_connection()
    print("Database connected")

    # Возвращаем данные для теста
    yield conn

    # Teardown: очистка (выполнится после теста)
    conn.close()
    print("Database disconnected")


def test_query(database_connection):
    """Тест использует connection."""
    result = database_connection.execute("SELECT 1")
    assert result == 1
    # После теста автоматически вызовется conn.close()
```

### Fixture Dependencies

```python
import pytest


@pytest.fixture
def database():
    """База данных."""
    return Database()


@pytest.fixture
def user_repository(database):
    """Repository зависит от database."""
    return UserRepository(database)


@pytest.fixture
def test_user(user_repository):
    """Пользователь зависит от repository."""
    user = user_repository.create(
        username="testuser",
        email="test@example.com"
    )
    return user


def test_user_exists(test_user, user_repository):
    """Тест использует зависимые fixtures."""
    found = user_repository.get_by_id(test_user.id)
    assert found is not None
    assert found.username == "testuser"
```

## Mocking - изоляция тестов

### Зачем нужен Mocking?

**Mock** - это объект-заглушка, который имитирует поведение реального объекта.

**Применение:**
- 🚫 Изолировать тесты от внешних зависимостей
- ⚡ Ускорить выполнение тестов
- 🎯 Контролировать поведение зависимостей
- 📊 Проверить, что методы были вызваны

### unittest.mock - базовый мок

```python
from unittest.mock import Mock, MagicMock, patch


def test_mock_basic():
    """Базовое использование Mock."""
    # Создаем мок
    mock = Mock()

    # Настраиваем return value
    mock.get_user.return_value = {"id": 1, "name": "John"}

    # Вызываем метод
    result = mock.get_user(user_id=1)

    # Проверяем результат
    assert result["name"] == "John"

    # Проверяем, что метод был вызван
    mock.get_user.assert_called_once()
    mock.get_user.assert_called_with(user_id=1)


def test_mock_side_effect():
    """Mock с side_effect для исключений."""
    mock = Mock()
    mock.divide.side_effect = ZeroDivisionError("Division by zero")

    import pytest
    with pytest.raises(ZeroDivisionError):
        mock.divide(10, 0)
```

### Patching - замена реальных объектов

```python
from unittest.mock import patch


# src/services/email.py
def send_email(to: str, subject: str, body: str):
    """Реальная отправка email."""
    # Вызов SMTP сервера
    smtp.send(to, subject, body)


# src/handlers/auth.py
from src.services.email import send_email

async def register_user(data):
    user = create_user(data)
    send_email(
        user.email,
        "Welcome!",
        f"Hello {user.username}"
    )
    return user


# tests/test_auth.py
@patch('src.handlers.auth.send_email')
def test_register_sends_email(mock_send_email):
    """Тест, что при регистрации отправляется email."""
    # Arrange
    user_data = {
        "username": "newuser",
        "email": "new@example.com",
        "password": "pass123"
    }

    # Act
    user = register_user(user_data)

    # Assert
    assert user.username == "newuser"

    # Проверяем, что send_email был вызван
    mock_send_email.assert_called_once()
    call_args = mock_send_email.call_args
    assert call_args[0][0] == "new@example.com"  # to
    assert "Welcome" in call_args[0][1]  # subject
```

### pytest-mock plugin

```python
import pytest


def test_with_mocker(mocker):
    """pytest-mock предоставляет удобный mocker."""
    # Патчим функцию
    mock_send = mocker.patch('src.services.email.send_email')
    mock_send.return_value = True

    # Вызываем код
    result = register_user({"email": "test@example.com"})

    # Проверяем
    assert mock_send.called
    assert mock_send.call_count == 1


def test_mock_database(mocker):
    """Мокируем database query."""
    # Мокируем метод БД
    mock_query = mocker.patch('src.db.session.execute')
    mock_query.return_value.scalar_one_or_none.return_value = {
        "id": 1,
        "username": "testuser"
    }

    # Тестируем handler
    user = get_user_by_id(1)
    assert user["username"] == "testuser"
```

## Async Tests

### Тестирование async функций

```python
import pytest


@pytest.mark.asyncio
async def test_async_function():
    """Тест асинхронной функции."""
    result = await fetch_data()
    assert result is not None


@pytest.mark.asyncio
async def test_async_handler(aiohttp_client):
    """Тест aiohttp handler."""
    app = create_app()
    client = await aiohttp_client(app)

    response = await client.get('/api/users')
    assert response.status == 200
    data = await response.json()
    assert isinstance(data, list)
```

### Мокирование async функций

```python
import pytest
from unittest.mock import AsyncMock


@pytest.mark.asyncio
async def test_mock_async(mocker):
    """Мокирование async функции."""
    # Создаем async mock
    mock_fetch = mocker.patch(
        'src.services.external_api.fetch_user',
        new=AsyncMock(return_value={"id": 1, "name": "John"})
    )

    # Вызываем код, который использует fetch_user
    result = await get_user_from_external_api(1)

    # Проверяем
    assert result["name"] == "John"
    mock_fetch.assert_awaited_once_with(1)
```

## Parametrize - множественные тесты

### Базовый parametrize

```python
import pytest


@pytest.mark.parametrize("input,expected", [
    (2, 4),
    (3, 9),
    (4, 16),
    (5, 25),
])
def test_square(input, expected):
    """Тест функции возведения в квадрат."""
    assert input ** 2 == expected


@pytest.mark.parametrize("email", [
    "test@example.com",
    "user.name@domain.co.uk",
    "user+tag@example.com",
])
def test_valid_emails(email):
    """Тест валидных email адресов."""
    assert is_valid_email(email)


@pytest.mark.parametrize("invalid_email", [
    "",
    "not-an-email",
    "@example.com",
    "user@",
    "user @example.com",
    "user@.com",
])
def test_invalid_emails(invalid_email):
    """Тест невалидных email адресов."""
    assert not is_valid_email(invalid_email)
```

### Комбинации параметров

```python
import pytest


@pytest.mark.parametrize("username,password,expected_status", [
    ("validuser", "ValidPass123", 200),
    ("", "ValidPass123", 400),  # Empty username
    ("validuser", "", 400),  # Empty password
    ("validuser", "short", 400),  # Too short password
    ("a" * 100, "ValidPass123", 400),  # Too long username
])
def test_login_validation(username, password, expected_status):
    """Тест валидации login."""
    response = login(username, password)
    assert response.status == expected_status
```

### pytest.param для меток

```python
import pytest


@pytest.mark.parametrize("value,expected", [
    (5, 25),
    (10, 100),
    pytest.param(
        100, 10000,
        marks=pytest.mark.slow,
        id="large_number"
    ),
])
def test_calculations(value, expected):
    assert calculate(value) == expected
```

## Примеры тестов для aiohttp

### Тестирование handlers

```python
# src/handlers/users.py
from aiohttp import web
from database import get_db_pool
from queries.users import get_user_by_id as get_user_by_id_query, list_users


async def get_users(request: web.Request) -> web.Response:
    """Получение списка пользователей."""
    pool = get_db_pool()
    async with pool.acquire() as conn:
        users = await list_users(conn)

    return web.json_response([
        {"id": u['id'], "username": u['username'], "email": u['email']}
        for u in users
    ])


async def get_user_by_id(request: web.Request) -> web.Response:
    """Получение пользователя по ID."""
    user_id = int(request.match_info['id'])

    pool = get_db_pool()
    async with pool.acquire() as conn:
        user = await get_user_by_id_query(conn, user_id)

    if not user:
        raise web.HTTPNotFound(reason="User not found")

    return web.json_response({
        "id": user['id'],
        "username": user['username'],
        "email": user['email']
    })


# tests/test_users_handlers.py
import pytest
from unittest.mock import AsyncMock, MagicMock


@pytest.mark.asyncio
async def test_get_users_empty(mocker):
    """Тест получения пустого списка пользователей."""
    # Мокируем connection pool и connection
    mock_conn = AsyncMock()
    mock_conn.__aenter__ = AsyncMock(return_value=mock_conn)
    mock_conn.__aexit__ = AsyncMock(return_value=None)
    mock_conn.fetch = AsyncMock(return_value=[])

    mock_pool = AsyncMock()
    mock_pool.acquire.return_value = mock_conn

    mocker.patch(
        'src.handlers.users.get_db_pool',
        return_value=mock_pool
    )
    mocker.patch(
        'src.handlers.users.list_users',
        return_value=[]
    )

    # Создаем fake request
    request = MagicMock()

    # Вызываем handler
    response = await get_users(request)

    # Проверяем
    assert response.status == 200
    data = await response.json()
    assert data == []


@pytest.mark.asyncio
async def test_get_users_with_data(mocker):
    """Тест получения списка пользователей."""
    # Создаем mock пользователей
    mock_user1 = MagicMock()
    mock_user1.id = 1
    mock_user1.username = "user1"
    mock_user1.email = "user1@example.com"

    mock_user2 = MagicMock()
    mock_user2.id = 2
    mock_user2.username = "user2"
    mock_user2.email = "user2@example.com"

    # Мокируем session
    mock_session = AsyncMock()
    mock_result = MagicMock()
    mock_result.scalars.return_value.all.return_value = [mock_user1, mock_user2]
    mock_session.execute.return_value = mock_result

    mocker.patch(
        'src.handlers.users.async_session',
        return_value=mock_session
    )

    request = MagicMock()
    response = await get_users(request)

    assert response.status == 200
    # Парсим JSON ответ
    import json
    data = json.loads(response.body)
    assert len(data) == 2
    assert data[0]["username"] == "user1"
    assert data[1]["username"] == "user2"


@pytest.mark.asyncio
async def test_get_user_by_id_not_found(mocker):
    """Тест получения несуществующего пользователя."""
    # Мокируем БД - пользователь не найден
    mock_session = AsyncMock()
    mock_result = MagicMock()
    mock_result.scalar_one_or_none.return_value = None
    mock_session.execute.return_value = mock_result

    mocker.patch(
        'src.handlers.users.async_session',
        return_value=mock_session
    )

    # Создаем request с ID
    request = MagicMock()
    request.match_info = {'id': '999'}

    # Проверяем, что выбрасывается 404
    with pytest.raises(web.HTTPNotFound):
        await get_user_by_id(request)
```

### Тестирование бизнес-логики

```python
# src/services/user_service.py
from typing import Optional
from src.models.user import User
from src.auth.jwt import get_password_hash, verify_password


class UserService:
    """Сервис для работы с пользователями."""

    def __init__(self, db_session):
        self.db = db_session

    async def create_user(
        self,
        username: str,
        email: str,
        password: str
    ) -> User:
        """Создание пользователя."""
        # Проверяем уникальность
        existing = await self.get_by_username(username)
        if existing:
            raise ValueError("Username already exists")

        # Хешируем пароль
        hashed_password = get_password_hash(password)

        # Создаем пользователя
        user = User(
            username=username,
            email=email,
            hashed_password=hashed_password
        )
        self.db.add(user)
        await self.db.commit()

        return user

    async def get_by_username(self, username: str) -> Optional[User]:
        """Поиск пользователя по username."""
        result = await self.db.execute(
            select(User).where(User.username == username)
        )
        return result.scalar_one_or_none()

    async def authenticate(
        self,
        username: str,
        password: str
    ) -> Optional[User]:
        """Аутентификация пользователя."""
        user = await self.get_by_username(username)
        if not user:
            return None

        if not verify_password(password, user.hashed_password):
            return None

        return user


# tests/test_user_service.py
import pytest
from unittest.mock import AsyncMock, MagicMock


@pytest.fixture
def mock_db_session():
    """Mock database session."""
    return AsyncMock()


@pytest.fixture
def user_service(mock_db_session):
    """UserService с mock БД."""
    return UserService(mock_db_session)


@pytest.mark.asyncio
async def test_create_user_success(user_service, mock_db_session, mocker):
    """Тест успешного создания пользователя."""
    # Мокируем get_by_username - пользователь не существует
    mocker.patch.object(
        user_service,
        'get_by_username',
        return_value=None
    )

    # Мокируем хеширование пароля
    mocker.patch(
        'src.services.user_service.get_password_hash',
        return_value='hashed_password'
    )

    # Создаем пользователя
    user = await user_service.create_user(
        username="newuser",
        email="new@example.com",
        password="SecurePass123"
    )

    # Проверяем
    assert user.username == "newuser"
    assert user.email == "new@example.com"
    assert user.hashed_password == "hashed_password"

    # Проверяем, что БД методы были вызваны
    mock_db_session.add.assert_called_once()
    mock_db_session.commit.assert_awaited_once()


@pytest.mark.asyncio
async def test_create_user_duplicate_username(user_service, mocker):
    """Тест создания пользователя с существующим username."""
    # Мокируем get_by_username - пользователь существует
    existing_user = MagicMock()
    existing_user.username = "existinguser"

    mocker.patch.object(
        user_service,
        'get_by_username',
        return_value=existing_user
    )

    # Проверяем, что выбрасывается ошибка
    with pytest.raises(ValueError, match="Username already exists"):
        await user_service.create_user(
            username="existinguser",
            email="new@example.com",
            password="pass123"
        )


@pytest.mark.asyncio
async def test_authenticate_success(user_service, mocker):
    """Тест успешной аутентификации."""
    # Создаем mock пользователя
    mock_user = MagicMock()
    mock_user.username = "testuser"
    mock_user.hashed_password = "hashed_pass"

    # Мокируем get_by_username
    mocker.patch.object(
        user_service,
        'get_by_username',
        return_value=mock_user
    )

    # Мокируем verify_password
    mocker.patch(
        'src.services.user_service.verify_password',
        return_value=True
    )

    # Аутентифицируем
    user = await user_service.authenticate("testuser", "password123")

    # Проверяем
    assert user is not None
    assert user.username == "testuser"


@pytest.mark.asyncio
async def test_authenticate_wrong_password(user_service, mocker):
    """Тест аутентификации с неверным паролем."""
    mock_user = MagicMock()
    mock_user.username = "testuser"
    mock_user.hashed_password = "hashed_pass"

    mocker.patch.object(
        user_service,
        'get_by_username',
        return_value=mock_user
    )

    # Мокируем verify_password - неверный пароль
    mocker.patch(
        'src.services.user_service.verify_password',
        return_value=False
    )

    # Аутентифицируем
    user = await user_service.authenticate("testuser", "wrongpassword")

    # Проверяем
    assert user is None


@pytest.mark.asyncio
async def test_authenticate_user_not_found(user_service, mocker):
    """Тест аутентификации несуществующего пользователя."""
    mocker.patch.object(
        user_service,
        'get_by_username',
        return_value=None
    )

    user = await user_service.authenticate("nonexistent", "password")
    assert user is None
```

## Best Practices

### 1. Arrange-Act-Assert (AAA) Pattern

```python
def test_user_creation():
    # Arrange - подготовка данных
    username = "testuser"
    email = "test@example.com"

    # Act - выполнение действия
    user = create_user(username, email)

    # Assert - проверка результата
    assert user.username == username
    assert user.email == email
```

### 2. Один assert - одна концепция

```python
# ❌ Плохо - слишком много проверок
def test_user_data():
    user = get_user(1)
    assert user.id == 1
    assert user.username == "test"
    assert user.email == "test@example.com"
    assert user.is_active == True
    assert user.created_at is not None


# ✅ Хорошо - разделить на несколько тестов
def test_user_has_correct_id():
    user = get_user(1)
    assert user.id == 1

def test_user_has_correct_username():
    user = get_user(1)
    assert user.username == "test"

def test_user_is_active():
    user = get_user(1)
    assert user.is_active
```

### 3. Понятные имена тестов

```python
# ❌ Плохо
def test_1():
    ...

def test_user():
    ...


# ✅ Хорошо
def test_user_creation_with_valid_data_succeeds():
    ...

def test_user_creation_with_duplicate_email_raises_error():
    ...

def test_inactive_user_cannot_login():
    ...
```

### 4. Не тестируйте implementation details

```python
# ❌ Плохо - тестирует внутреннюю реализацию
def test_user_service_calls_database_execute():
    service.create_user("test", "test@example.com")
    mock_db.execute.assert_called()  # Хрупкий тест


# ✅ Хорошо - тестирует поведение
def test_user_service_creates_user_successfully():
    user = service.create_user("test", "test@example.com")
    assert user.username == "test"  # Проверяем результат
```

## Дополнительные материалы

### Документация
- [pytest documentation](https://docs.pytest.org/)
- [pytest-asyncio](https://pytest-asyncio.readthedocs.io/)
- [pytest-mock](https://pytest-mock.readthedocs.io/)
- [unittest.mock](https://docs.python.org/3/library/unittest.mock.html)

### Книги
- "Python Testing with pytest" - Brian Okken
- "Test-Driven Development with Python" - Harry Percival

### Статьи
- [Effective Python Testing With Pytest](https://realpython.com/pytest-python-testing/)
- [Mocking in Python](https://realpython.com/python-mock-library/)

### Видео
- [pytest Tutorial](https://www.youtube.com/watch?v=bbp_849-RZ4)
- [Python Mocking 101](https://www.youtube.com/watch?v=ww1UsGZV8fQ)

## Следующая неделя

На [Неделе 6](../week-06/README.md) изучим Integration тесты с testsuite и реальными зависимостями! 🧪

---

**Удачи с unit тестированием! 🧪**

