# Неделя 18: Docker Compose - Multi-container приложения

## Цели недели
- Понять Docker Compose и его назначение
- Научиться создавать multi-container приложения
- Освоить работу с volumes и networks
- Настроить docker-compose для полного стека приложения
- Изучить best practices для Docker Compose

## Теоретическая часть

### Что такое Docker Compose?

**Docker Compose** - инструмент для определения и запуска multi-container Docker приложений.

**Проблема без Compose:**
```bash
# Запуск одного контейнера за раз - неудобно!
docker run -d --name postgres -e POSTGRES_PASSWORD=pass postgres:15
docker run -d --name redis -p 6379:6379 redis:7
docker run -d --name app --link postgres --link redis -p 8000:8000 myapp
# Сложно управлять, нет автоматического перезапуска, сложная настройка сети...
```

**С Docker Compose:**
```bash
docker-compose up -d  # Один раз - всё запускается!
```

**Преимущества Docker Compose:**
- ✅ Однофайловая конфигурация всего стека
- ✅ Автоматическое создание сетей и volumes
- ✅ Простое управление жизненным циклом
- ✅ Переменные окружения в одном месте
- ✅ Версионирование конфигурации в Git

### Структура docker-compose.yml

```yaml
version: '3.8'  # Версия формата

services:       # Определение сервисов
  app:
    # Конфигурация приложения
  db:
    # Конфигурация БД
  redis:
    # Конфигурация Redis

volumes:        # Именованные volumes
  postgres_data:
    driver: local

networks:       # Пользовательские сети (опционально)
  app_network:
    driver: bridge
```

## Основные концепции

### 1. Services (Сервисы)

**Service** - это контейнер, который Compose создаёт и управляет.

```yaml
services:
  web:
    build: .              # Собрать из Dockerfile
    image: myapp:latest   # Или использовать готовый образ
    ports:
      - "8000:8000"       # Маппинг портов host:container
    environment:
      - DEBUG=true        # Переменные окружения
    depends_on:           # Зависимости
      - db
      - redis
```

### 2. Volumes (Тома)

**Volume** - способ сохранять данные между перезапусками контейнеров.

**Типы volumes:**
1. **Named volumes** - управляемые Docker
2. **Bind mounts** - привязка к папке хоста
3. **Anonymous volumes** - временные

```yaml
services:
  db:
    image: postgres:15-alpine
    volumes:
      # Named volume
      - postgres_data:/var/lib/postgresql/data

      # Bind mount (для разработки)
      - ./postgres-init:/docker-entrypoint-initdb.d

      # Anonymous volume
      - /tmp/cache

volumes:
  postgres_data:
    driver: local
```

### 3. Networks (Сети)

**Network** - изолированная сеть для коммуникации между контейнерами.

```yaml
services:
  app:
    networks:
      - app_network
  db:
    networks:
      - app_network

networks:
  app_network:
    driver: bridge
```

**Преимущества:**
- Контейнеры могут обращаться друг к другу по имени сервиса
- Изоляция от других приложений
- Автоматическое DNS разрешение имен

## Практическая часть

### Задание 1: Базовый docker-compose.yml

Создайте `docker-compose.yml` для вашего API с PostgreSQL:

```yaml
version: '3.8'

services:
  # API сервис
  api:
    build:
      context: .
      dockerfile: Dockerfile
    container_name: todo-api
    ports:
      - "8000:8000"
    environment:
      - DATABASE_URL=postgresql://todo_user:todo_pass@db:5432/tododb
      - REDIS_URL=redis://redis:6379/0
      - DEBUG=false
    depends_on:
      db:
        condition: service_healthy
      redis:
        condition: service_started
    restart: unless-stopped
    networks:
      - app_network

  # PostgreSQL
  db:
    image: postgres:15-alpine
    container_name: todo-postgres
    environment:
      POSTGRES_USER: todo_user
      POSTGRES_PASSWORD: todo_pass
      POSTGRES_DB: tododb
    volumes:
      - postgres_data:/var/lib/postgresql/data
      - ./alembic/versions:/docker-entrypoint-initdb.d
    ports:
      - "5432:5432"
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U todo_user -d tododb"]
      interval: 10s
      timeout: 5s
      retries: 5
      start_period: 10s
    restart: unless-stopped
    networks:
      - app_network

  # Redis
  redis:
    image: redis:7-alpine
    container_name: todo-redis
    ports:
      - "6379:6379"
    volumes:
      - redis_data:/data
    command: redis-server --appendonly yes
    healthcheck:
      test: ["CMD", "redis-cli", "ping"]
      interval: 10s
      timeout: 5s
      retries: 5
    restart: unless-stopped
    networks:
      - app_network

volumes:
  postgres_data:
    driver: local
  redis_data:
    driver: local

networks:
  app_network:
    driver: bridge
```

**Запуск:**
```bash
# Запустить все сервисы
docker-compose up -d

# Просмотр логов
docker-compose logs -f

# Логи конкретного сервиса
docker-compose logs -f api

# Остановить все
docker-compose stop

# Остановить и удалить контейнеры
docker-compose down

# Остановить и удалить volumes (⚠️ данные будут удалены!)
docker-compose down -v
```

### Задание 2: Разные конфигурации для dev/prod

Создайте разные конфигурации для разработки и продакшена:

**docker-compose.yml** (базовая):
```yaml
version: '3.8'

services:
  api:
    build: .
    depends_on:
      - db
      - redis

  db:
    image: postgres:15-alpine
    environment:
      POSTGRES_USER: ${POSTGRES_USER:-todo_user}
      POSTGRES_PASSWORD: ${POSTGRES_PASSWORD:-todo_pass}
      POSTGRES_DB: ${POSTGRES_DB:-tododb}
    volumes:
      - postgres_data:/var/lib/postgresql/data

  redis:
    image: redis:7-alpine
    volumes:
      - redis_data:/data

volumes:
  postgres_data:
  redis_data:
```

**docker-compose.dev.yml** (для разработки):
```yaml
version: '3.8'

services:
  api:
    build:
      context: .
      dockerfile: Dockerfile.dev
    volumes:
      # Hot reload - монтируем код
      - .:/app
      - /app/__pycache__  # Исключаем кэш
    ports:
      - "8000:8000"
    environment:
      - DEBUG=true
      - DATABASE_URL=postgresql://todo_user:todo_pass@db:5432/tododb
      - REDIS_URL=redis://redis:6379/0
    command: python -m aiohttp.web -H 0.0.0.0 -P 8000 main:app

  db:
    ports:
      - "5432:5432"  # Доступ к БД с хоста для отладки

  redis:
    ports:
      - "6379:6379"  # Доступ к Redis с хоста
```

**docker-compose.prod.yml** (для продакшена):
```yaml
version: '3.8'

services:
  api:
    restart: always
    environment:
      - DEBUG=false
      - DATABASE_URL=${DATABASE_URL}
      - REDIS_URL=${REDIS_URL}
      - SECRET_KEY=${SECRET_KEY}
    deploy:
      replicas: 2
      resources:
        limits:
          cpus: '1'
          memory: 512M
        reservations:
          cpus: '0.5'
          memory: 256M

  db:
    restart: always
    deploy:
      resources:
        limits:
          cpus: '1'
          memory: 1G
```

**Использование:**
```bash
# Для разработки
docker-compose -f docker-compose.yml -f docker-compose.dev.yml up

# Для продакшена
docker-compose -f docker-compose.yml -f docker-compose.prod.yml up -d
```

### Задание 3: Environment файлы

Используйте `.env` файлы для конфигурации:

**.env.example:**
```env
POSTGRES_USER=todo_user
POSTGRES_PASSWORD=change_me_in_production
POSTGRES_DB=tododb
DATABASE_URL=postgresql://todo_user:todo_pass@db:5432/tododb
REDIS_URL=redis://redis:6379/0
DEBUG=true
SECRET_KEY=change_me_in_production
```

**.env** (не коммитить в Git!):
```env
POSTGRES_USER=todo_user
POSTGRES_PASSWORD=my_secure_password
POSTGRES_DB=tododb
DATABASE_URL=postgresql://todo_user:my_secure_password@db:5432/tododb
REDIS_URL=redis://redis:6379/0
DEBUG=true
SECRET_KEY=super_secret_key_here
```

**docker-compose.yml:**
```yaml
version: '3.8'

services:
  api:
    build: .
    env_file:
      - .env
    environment:
      - DATABASE_URL=${DATABASE_URL}
      - DEBUG=${DEBUG:-false}

  db:
    image: postgres:15-alpine
    environment:
      POSTGRES_USER: ${POSTGRES_USER}
      POSTGRES_PASSWORD: ${POSTGRES_PASSWORD}
      POSTGRES_DB: ${POSTGRES_DB}
```

### Задание 4: Продвинутые возможности

#### Healthchecks

```yaml
services:
  api:
    build: .
    healthcheck:
      test: ["CMD", "curl", "-f", "http://localhost:8000/health"]
      interval: 30s
      timeout: 10s
      retries: 3
      start_period: 40s
```

#### Depends_on с условиями

```yaml
services:
  api:
    depends_on:
      db:
        condition: service_healthy
      redis:
        condition: service_started
```

#### Restart policies

```yaml
services:
  api:
    restart: unless-stopped  # Перезапуск всегда, кроме ручной остановки
    # restart: always          # Перезапуск всегда
    # restart: on-failure      # Только при ошибке
    # restart: no              # Не перезапускать (по умолчанию)
```

#### Resource limits

```yaml
services:
  api:
    deploy:
      resources:
        limits:
          cpus: '1.0'
          memory: 512M
        reservations:
          cpus: '0.5'
          memory: 256M
```

## Volumes - детальнее

### Named Volumes

```yaml
volumes:
  postgres_data:
    driver: local
    driver_opts:
      type: none
      o: bind
      device: ./data/postgres
```

**Управление:**
```bash
# Список volumes
docker volume ls

# Детали volume
docker volume inspect postgres_data

# Удаление
docker volume rm postgres_data
```

### Bind Mounts (для разработки)

```yaml
services:
  api:
    volumes:
      # Монтирование кода для hot reload
      - ./src:/app/src
      - ./config:/app/config:ro  # read-only
```

### tmpfs mounts (для временных данных)

```yaml
services:
  api:
    tmpfs:
      - /tmp:rw,noexec,nosuid,size=100m
```

## Networks - детальнее

### Пользовательские сети

```yaml
networks:
  frontend:
    driver: bridge
  backend:
    driver: bridge

services:
  api:
    networks:
      - frontend
      - backend

  db:
    networks:
      - backend  # Только backend сеть
```

### External networks

```yaml
networks:
  external_network:
    external: true
    name: my_existing_network
```

## Best Practices

### 1. Используйте .dockerignore

```.dockerignore
# .dockerignore
__pycache__
*.pyc
*.pyo
*.pyd
.Python
*.so
.git
.gitignore
.env
.venv
venv/
*.md
tests/
.pytest_cache
.coverage
```

### 2. Версионируйте compose файлы

```yaml
version: '3.8'  # Всегда указывайте версию
```

### 3. Используйте переменные окружения

```yaml
services:
  api:
    environment:
      - DATABASE_URL=${DATABASE_URL}
    # Или через env_file
    env_file:
      - .env
```

### 4. Healthchecks для всех сервисов

```yaml
services:
  api:
    healthcheck:
      test: ["CMD", "curl", "-f", "http://localhost:8000/health"]
      interval: 30s
      timeout: 10s
      retries: 3
```

### 5. Правильные зависимости

```yaml
depends_on:
  db:
    condition: service_healthy
```

### 6. Resource limits в продакшене

```yaml
deploy:
  resources:
    limits:
      cpus: '1'
      memory: 512M
```

### 7. Безопасность

```yaml
services:
  api:
    # Не запускать от root
    user: "1000:1000"

    # Read-only root filesystem где возможно
    read_only: true
    tmpfs:
      - /tmp
```

## Docker Compose команды

```bash
# Запуск
docker-compose up              # Запустить в foreground
docker-compose up -d           # Запустить в background
docker-compose up --build      # Пересобрать образы

# Остановка
docker-compose stop            # Остановить
docker-compose down            # Остановить и удалить
docker-compose down -v         # С volumes

# Логи
docker-compose logs            # Все логи
docker-compose logs -f api     # Следить за логами API
docker-compose logs --tail=100 # Последние 100 строк

# Выполнение команд
docker-compose exec api bash   # Зайти в контейнер api
docker-compose exec db psql -U user -d dbname

# Перезапуск
docker-compose restart api     # Перезапустить сервис
docker-compose up --force-recreate  # Пересоздать

# Статус
docker-compose ps              # Статус всех сервисов
docker-compose top             # Процессы в контейнерах

# Сборка
docker-compose build           # Собрать образы
docker-compose build --no-cache  # Без кэша

# Версионирование
docker-compose config          # Проверить конфигурацию
docker-compose config > output.yml  # Сохранить финальную конфигурацию
```

## Пример: Полный стек

**docker-compose.yml:**
```yaml
version: '3.8'

services:
  # API Gateway
  gateway:
    build:
      context: ./gateway
      dockerfile: Dockerfile
    ports:
      - "8000:8000"
    environment:
      - AUTH_SERVICE=http://auth:8001
      - EXPERIMENT_SERVICE=http://experiment:8002
      - METRICS_SERVICE=http://metrics:8003
    depends_on:
      - auth
      - experiment
      - metrics
    networks:
      - api_network
    restart: unless-stopped

  # Auth Service
  auth:
    build:
      context: ./auth-service
      dockerfile: Dockerfile
    environment:
      - DATABASE_URL=postgresql://auth_user:auth_pass@auth_db:5432/authdb
      - REDIS_URL=redis://redis:6379/1
      - JWT_SECRET=${JWT_SECRET}
    depends_on:
      auth_db:
        condition: service_healthy
      redis:
        condition: service_healthy
    networks:
      - api_network
      - db_network
    restart: unless-stopped

  auth_db:
    image: postgres:15-alpine
    environment:
      POSTGRES_USER: auth_user
      POSTGRES_PASSWORD: auth_pass
      POSTGRES_DB: authdb
    volumes:
      - auth_db_data:/var/lib/postgresql/data
    networks:
      - db_network
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U auth_user"]
      interval: 10s
      timeout: 5s
      retries: 5
    restart: unless-stopped

  # Experiment Service
  experiment:
    build:
      context: ./experiment-service
      dockerfile: Dockerfile
    environment:
      - DATABASE_URL=postgresql://exp_user:exp_pass@exp_db:5432/expdb
      - RABBITMQ_URL=amqp://rabbitmq:5672
    depends_on:
      exp_db:
        condition: service_healthy
      rabbitmq:
        condition: service_healthy
    networks:
      - api_network
      - db_network
      - rabbitmq_network
    restart: unless-stopped

  exp_db:
    image: postgres:15-alpine
    environment:
      POSTGRES_USER: exp_user
      POSTGRES_PASSWORD: exp_pass
      POSTGRES_DB: expdb
    volumes:
      - exp_db_data:/var/lib/postgresql/data
    networks:
      - db_network
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U exp_user"]
      interval: 10s
      timeout: 5s
      retries: 5
    restart: unless-stopped

  # Metrics Service
  metrics:
    build:
      context: ./metrics-service
      dockerfile: Dockerfile
    environment:
      - DATABASE_URL=postgresql://metrics_user:metrics_pass@metrics_db:5432/metricsdb
      - RABBITMQ_URL=amqp://rabbitmq:5672
    depends_on:
      metrics_db:
        condition: service_healthy
      rabbitmq:
        condition: service_healthy
    networks:
      - api_network
      - db_network
      - rabbitmq_network
    restart: unless-stopped

  metrics_db:
    image: postgres:15-alpine
    environment:
      POSTGRES_USER: metrics_user
      POSTGRES_PASSWORD: metrics_pass
      POSTGRES_DB: metricsdb
    volumes:
      - metrics_db_data:/var/lib/postgresql/data
    networks:
      - db_network
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U metrics_user"]
      interval: 10s
      timeout: 5s
      retries: 5
    restart: unless-stopped

  # Redis
  redis:
    image: redis:7-alpine
    volumes:
      - redis_data:/data
    command: redis-server --appendonly yes
    networks:
      - api_network
    healthcheck:
      test: ["CMD", "redis-cli", "ping"]
      interval: 10s
      timeout: 5s
      retries: 5
    restart: unless-stopped

  # RabbitMQ
  rabbitmq:
    image: rabbitmq:3-management-alpine
    ports:
      - "15672:15672"  # Management UI
    environment:
      RABBITMQ_DEFAULT_USER: admin
      RABBITMQ_DEFAULT_PASS: admin
    volumes:
      - rabbitmq_data:/var/lib/rabbitmq
    networks:
      - rabbitmq_network
    healthcheck:
      test: ["CMD", "rabbitmq-diagnostics", "ping"]
      interval: 10s
      timeout: 5s
      retries: 5
    restart: unless-stopped

volumes:
  auth_db_data:
    driver: local
  exp_db_data:
    driver: local
  metrics_db_data:
    driver: local
  redis_data:
    driver: local
  rabbitmq_data:
    driver: local

networks:
  api_network:
    driver: bridge
  db_network:
    driver: bridge
  rabbitmq_network:
    driver: bridge
```

## Troubleshooting

### Проблема: Контейнер не может подключиться к БД

**Решение:**
```yaml
# Убедитесь, что используете имя сервиса, а не localhost
DATABASE_URL=postgresql://user:pass@db:5432/dbname
# НЕ: postgresql://user:pass@localhost:5432/dbname
```

### Проблема: Volumes не работают

**Решение:**
```yaml
# Проверьте права доступа
volumes:
  - ./data:/data:rw  # Укажите права явно
```

### Проблема: Порты заняты

**Решение:**
```bash
# Проверьте какие порты заняты
docker-compose ps

# Или измените маппинг портов
ports:
  - "8001:8000"  # Внешний порт 8001, внутренний 8000
```

## Дополнительные материалы

### Полезные ссылки
- [Docker Compose Documentation](https://docs.docker.com/compose/)
- [Compose File Reference](https://docs.docker.com/compose/compose-file/)
- [Best Practices for Compose](https://docs.docker.com/compose/production/)

### Инструменты
- [Docker Compose](https://docs.docker.com/compose/install/)
- [Compose V2](https://docs.docker.com/compose/cli-command/) - новая версия
- [Portainer](https://www.portainer.io/) - GUI для Docker

### Статьи
- [Docker Compose Best Practices](https://www.freecodecamp.org/news/docker-compose-best-practices/)
- [Multi-container Applications](https://docs.docker.com/get-started/07_multi_container/)

## Вопросы для самопроверки

1. В чем разница между named volume и bind mount?
2. Когда использовать depends_on с condition?
3. Как правильно организовать сети для микросервисов?
4. Зачем нужны healthchecks в docker-compose?
5. Как обеспечить безопасность в Docker Compose?

## Следующая неделя

На [Неделе 19](../week-19/README.md) изучим CI/CD с Docker, deployment strategies и production setup! 🚀

---

**Удачи с Docker Compose! 🐳**

