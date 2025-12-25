# Семестр 1: Основы бэкенд-разработки

## Обзор семестра

**Длительность:** 14 учебных недель + 2 резервные недели

Первый семестр посвящен изучению фундаментальных концепций бэкенд-разработки на Python с использованием aiohttp.

### Цели семестра

К концу семестра студенты будут уметь:
- Создавать REST API на aiohttp
- Работать с PostgreSQL через asyncpg
- Писать unit и integration тесты
- Настраивать CI/CD с GitHub Actions
- Оптимизировать производительность с Redis
- Работать с асинхронным кодом
- Создавать API документацию

### Модули

#### Модуль 1: Веб-разработка и API (Недели 1-4)
**Темы:**
- Неделя 1: HTTP протокол и REST principles, aiohttp основы
- Неделя 2: CRUD операции, валидация данных с Pydantic
- Неделя 3: PostgreSQL и asyncpg, миграции
- Неделя 4: JWT аутентификация, защищенные endpoints

**Практика:**
- Создание TODO API
- User management API
- Аутентификация и авторизация

#### Модуль 2: Качество и тестирование (Недели 5-7)
**Темы:**
- Неделя 5: Unit тестирование с pytest, fixtures, mocking
- Неделя 6: Integration тесты с testsuite, test coverage
- Неделя 7: CI/CD с GitHub Actions, линтеры, structured logging

**Практика:**
- Покрытие тестами API (>70%)
- Настройка CI pipeline
- Логирование и мониторинг

#### Модуль 3: Кэширование и производительность (Недели 8-11)
**Темы:**
- Неделя 8: Redis и кэширование, стратегии кэширования
- Неделя 9: Async/await глубже, асинхронные паттерны
- Неделя 10: Background jobs с Celery/arq
- Неделя 11: Оптимизация БД, N+1 problem, индексы, профилирование

**Практика:**
- Кэширование запросов
- Оптимизация queries
- Background task processing

#### Модуль 4: API контракты и интеграция (Недели 12-14)
**Темы:**
- Неделя 12: OpenAPI/Swagger, API документация
- Неделя 13: API versioning, CORS, Rate limiting
- Неделя 14: Итоговая работа семестра, защита проектов

**Практика:**
- Генерация API документации
- Версионирование API
- Интеграция и deployment

#### Резервные недели (15-16)
- Неделя 15: Резерв (каникулы, догоняющие занятия)
- Неделя 16: Резерв (дополнительные консультации, пересдачи)

### Итоговая работа семестра

Создать полноценное REST API приложение с:
- Аутентификацией
- CRUD операциями
- Тестами (покрытие >70%)
- CI/CD
- Документацией

**Примеры тем:**
- Task management system
- Blog API
- Library management system
- Event booking system

### Оценивание

- Практические задания: 30 баллов
- Промежуточное тестирование: 10 баллов
- Итоговое задание: 10 баллов
- **Всего: 50 баллов**

### Полезные ресурсы

**Документация:**
- [aiohttp](https://docs.aiohttp.org/)
- [asyncpg](https://magicstack.github.io/asyncpg/)
- [Pydantic](https://docs.pydantic.dev/)
- [pytest](https://docs.pytest.org/)

**Книги:**
- "Python Concurrency with asyncio" - Matthew Fowler
- "Test-Driven Development with Python" - Harry Percival

**Видео:**
- [AsyncIO Tutorial](https://www.youtube.com/watch?v=t5Bo1Je9EmE)
- [asyncpg Tutorial](https://www.youtube.com/watch?v=DJ0P7F9y8uo)

### Следующий семестр

Во втором семестре мы перейдем к микросервисной архитектуре, Docker, и построению распределенных систем.

