# Быстрый старт

## Самый простой способ (1 команда)

```bash
docker-compose up -d
```

Готово! Сервис доступен на http://localhost:8002

## Проверка работы

```bash
# Проверка health endpoint
curl http://localhost:8002/health

# Просмотр логов
docker-compose logs -f experiment-service

# Остановка
docker-compose down
```

## Локальная разработка

```bash
# 1. Запустить только БД
docker-compose up -d postgres

# 2. Установить зависимости
pip install -r requirements.txt

# 3. Запустить сервис
python main.py
```

## Что происходит при запуске

1. ✅ PostgreSQL запускается в контейнере
2. ✅ Создается база данных `experiment_db`
3. ✅ Experiment Service подключается к БД с автоматическими повторами
4. ✅ Автоматически создаются все таблицы (experiments, runs, experiment_tags)
5. ✅ Сервис готов принимать запросы на порту 8002

## Переменные окружения

Скопируйте `env.example` в `.env` и отредактируйте при необходимости:

```bash
cp env.example .env
```

Основные параметры:
- `DATABASE_URL` - строка подключения к PostgreSQL
- `PORT` - порт для сервиса (по умолчанию 8002)
- `LOG_LEVEL` - уровень логирования

