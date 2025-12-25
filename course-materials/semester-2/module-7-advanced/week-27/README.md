# Неделя 27: Нагрузочное тестирование микросервисов

## Цели недели
- Понять типы нагрузочного тестирования
- Изучить инструменты для нагрузочного тестирования
- Освоить Locust и k6
- Научиться анализировать результаты тестов
- Понять как оптимизировать производительность

## Теоретическая часть

### Зачем нужно нагрузочное тестирование?

**Нагрузочное тестирование** - проверка работы системы под нагрузкой.

**Цели:**
- ✅ Определить максимальную производительность
- ✅ Найти узкие места (bottlenecks)
- ✅ Проверить стабильность под нагрузкой
- ✅ Спланировать масштабирование
- ✅ Убедиться в готовности к production

**Без нагрузочного тестирования:**
- ❌ Неизвестна максимальная нагрузка
- ❌ Непонятно, где узкие места
- ❌ Неожиданные падения в production
- ❌ Неправильное планирование ресурсов

### Типы нагрузочного тестирования

#### 1. Load Testing (Нагрузочное тестирование)

**Цель:** Проверить работу системы при ожидаемой нагрузке.

**Характеристики:**
- Реалистичная нагрузка
- Продолжительное время
- Ожидаемая производительность

```
Load: [=====>     ] (постепенное увеличение до ожидаемого уровня)
Time: |---------->
```

#### 2. Stress Testing (Стресс-тестирование)

**Цель:** Найти точку отказа системы.

**Характеристики:**
- Нагрузка выше ожидаемой
- Постепенное увеличение до предела
- Поиск точки отказа

```
Load: [===========>] (до предела)
Time: |---------->
```

#### 3. Spike Testing (Пиковое тестирование)

**Цель:** Проверить реакцию на резкий всплеск нагрузки.

**Характеристики:**
- Резкое увеличение нагрузки
- Короткая продолжительность
- Проверка восстановления

```
Load: [     /|\    ] (резкий всплеск)
Time: |---------->
```

#### 4. Endurance Testing (Тестирование на выносливость)

**Цель:** Проверить стабильность при длительной нагрузке.

**Характеристики:**
- Умеренная нагрузка
- Длительное время (часы/дни)
- Проверка утечек памяти

```
Load: [=====>     ] (стабильная нагрузка)
Time: |------------------------>
```

#### 5. Volume Testing (Объемное тестирование)

**Цель:** Проверить работу с большими объемами данных.

**Характеристики:**
- Большие объемы данных
- Проверка обработки
- Производительность с данными

## Метрики нагрузочного тестирования

### Что измеряем?

1. **Throughput (Пропускная способность)**
   - Количество запросов в секунду (RPS)
   - Количество транзакций в секунду (TPS)

2. **Latency (Задержка)**
   - Response time (время ответа)
   - P50, P95, P99 перцентили

3. **Error Rate (Частота ошибок)**
   - Процент ошибок
   - Типы ошибок (5xx, 4xx, timeout)

4. **Resource Usage (Использование ресурсов)**
   - CPU usage
   - Memory usage
   - Network I/O
   - Database connections

5. **Concurrency (Параллелизм)**
   - Количество одновременных пользователей
   - Количество активных соединений

## Locust

### Что такое Locust?

**Locust** - инструмент для нагрузочного тестирования на Python.

**Преимущества:**
- ✅ Код на Python (гибкость)
- ✅ Распределенное выполнение
- ✅ Веб-интерфейс для мониторинга
- ✅ Поддержка различных протоколов

### Установка

```bash
pip install locust
```

### Базовый пример

```python
# locustfile.py
from locust import HttpUser, task, between

class ExperimentAPIUser(HttpUser):
    """Пользователь для тестирования Experiment API."""
    wait_time = between(1, 3)  # Пауза между запросами (1-3 сек)

    def on_start(self):
        """Выполняется один раз при старте пользователя."""
        # Логин или другая инициализация
        response = self.client.post(
            "/api/v1/auth/login",
            json={"username": "test_user", "password": "password"}
        )
        self.token = response.json()["access_token"]
        self.client.headers = {"Authorization": f"Bearer {self.token}"}

    @task(3)  # Вес задачи (частота выполнения)
    def get_experiments(self):
        """Получение списка экспериментов."""
        self.client.get("/api/v1/experiments")

    @task(2)
    def get_experiment_by_id(self):
        """Получение эксперимента по ID."""
        experiment_id = 1
        self.client.get(f"/api/v1/experiments/{experiment_id}")

    @task(1)
    def create_experiment(self):
        """Создание эксперимента."""
        self.client.post(
            "/api/v1/experiments",
            json={
                "name": "Test Experiment",
                "project_id": 1,
                "config": {}
            }
        )

    def on_stop(self):
        """Выполняется при остановке пользователя."""
        # Cleanup если нужно
        pass
```

### Запуск Locust

```bash
# Запуск с веб-интерфейсом
locust -f locustfile.py --host=http://localhost:8000

# Веб-интерфейс: http://localhost:8089
```

**Параметры запуска:**
```bash
# Запуск без веб-интерфейса (headless)
locust -f locustfile.py \
  --host=http://localhost:8000 \
  --users=100 \
  --spawn-rate=10 \
  --run-time=5m \
  --headless

# Распределенный запуск
# Master
locust -f locustfile.py --master

# Workers (на других машинах)
locust -f locustfile.py --worker --master-host=<master-ip>
```

### Продвинутые возможности

#### Группировка задач

```python
from locust import HttpUser, task, TaskSet

class ExperimentTasks(TaskSet):
    """Группа задач для экспериментов."""

    @task(3)
    def list_experiments(self):
        self.client.get("/api/v1/experiments")

    @task(1)
    def create_experiment(self):
        self.client.post("/api/v1/experiments", json={...})

class UserTasks(TaskSet):
    """Группа задач для пользователей."""

    @task(2)
    def get_profile(self):
        self.client.get("/api/v1/users/me")

    tasks = {ExperimentTasks: 3, UserTasks: 1}  # Веса групп

class ExperimentAPIUser(HttpUser):
    tasks = [UserTasks]
    wait_time = between(1, 3)
```

#### Custom клиенты

```python
from locust import User, task, events
import asyncio
import httpx

class AsyncHttpUser(User):
    """Асинхронный HTTP клиент для Locust."""
    abstract = True

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.client = httpx.AsyncClient(base_url=self.host)

    @task
    async def my_task(self):
        response = await self.client.get("/api/v1/experiments")
        # Locust автоматически учитывает время выполнения
```

#### Проверка ответов

```python
from locust import HttpUser, task
from locust.exception import StopUser

class ExperimentAPIUser(HttpUser):

    @task
    def get_experiments(self):
        with self.client.get(
            "/api/v1/experiments",
            catch_response=True
        ) as response:
            if response.status_code == 200:
                data = response.json()
                if len(data) > 0:
                    response.success()
                else:
                    response.failure("Empty response")
            elif response.status_code == 401:
                # Перелогин при необходимости
                self.login()
                response.failure("Unauthorized")
            else:
                response.failure(f"Status {response.status_code}")
```

#### Параметризация данных

```python
import random
from locust import HttpUser, task

class ExperimentAPIUser(HttpUser):

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        # Загружаем данные для тестирования
        self.experiment_ids = [1, 2, 3, 4, 5]
        self.project_ids = [1, 2, 3]

    @task
    def get_experiment(self):
        experiment_id = random.choice(self.experiment_ids)
        self.client.get(f"/api/v1/experiments/{experiment_id}")

    @task
    def create_experiment(self):
        project_id = random.choice(self.project_ids)
        self.client.post(
            "/api/v1/experiments",
            json={
                "name": f"Test Experiment {random.randint(1, 1000)}",
                "project_id": project_id
            }
        )
```

#### События и метрики

```python
from locust import events
from locust.runners import MasterRunner

@events.test_start.add_listener
def on_test_start(environment, **kwargs):
    """Выполняется при старте теста."""
    print("Test started!")

@events.test_stop.add_listener
def on_test_stop(environment, **kwargs):
    """Выполняется при остановке теста."""
    print("Test stopped!")
    # Сохранение результатов, отправка отчетов и т.д.

@events.request.add_listener
def on_request(request_type, name, response_time, response_length, exception, **kwargs):
    """Обработка каждого запроса."""
    if exception:
        print(f"Request failed: {name}, Exception: {exception}")

@events.user_error.add_listener
def on_user_error(user_instance, exception, **kwargs):
    """Обработка ошибок пользователя."""
    print(f"User error: {exception}")
```

## k6

### Что такое k6?

**k6** - инструмент для нагрузочного тестирования от Grafana Labs.

**Преимущества:**
- ✅ Скрипты на JavaScript
- ✅ Высокая производительность
- ✅ Встроенные метрики
- ✅ Интеграция с Grafana/InfluxDB

### Установка

```bash
# macOS
brew install k6

# Linux
curl https://github.com/grafana/k6/releases/download/v0.47.0/k6-v0.47.0-linux-amd64.tar.gz -L | tar xvz
sudo mv k6-v0.47.0-linux-amd64/k6 /usr/local/bin/

# Docker
docker pull grafana/k6
```

### Базовый пример

```javascript
// load_test.js
import http from 'k6/http';
import { check, sleep } from 'k6';

export const options = {
  stages: [
    { duration: '30s', target: 50 },   // Ramp-up до 50 пользователей
    { duration: '1m', target: 50 },     // Держим 50 пользователей
    { duration: '30s', target: 100 },    // Увеличиваем до 100
    { duration: '1m', target: 100 },     // Держим 100
    { duration: '30s', target: 0 },     // Ramp-down до 0
  ],
  thresholds: {
    http_req_duration: ['p(95)<500'],  // 95% запросов < 500ms
    http_req_failed: ['rate<0.01'],    // <1% ошибок
  },
};

const BASE_URL = 'http://localhost:8000';

export function setup() {
  // Выполняется один раз перед тестом
  const loginRes = http.post(`${BASE_URL}/api/v1/auth/login`, JSON.stringify({
    username: 'test_user',
    password: 'password',
  }), {
    headers: { 'Content-Type': 'application/json' },
  });

  const token = loginRes.json('access_token');
  return { token };
}

export default function (data) {
  const params = {
    headers: {
      'Authorization': `Bearer ${data.token}`,
    },
  };

  // Получение списка экспериментов
  const experimentsRes = http.get(`${BASE_URL}/api/v1/experiments`, params);
  check(experimentsRes, {
    'status is 200': (r) => r.status === 200,
    'response has experiments': (r) => r.json().length > 0,
  });

  sleep(1);

  // Создание эксперимента
  const createRes = http.post(
    `${BASE_URL}/api/v1/experiments`,
    JSON.stringify({
      name: `Test Experiment ${__VU}`,  // __VU - уникальный ID виртуального пользователя
      project_id: 1,
    }),
    {
      ...params,
      headers: {
        ...params.headers,
        'Content-Type': 'application/json',
      },
    }
  );

  check(createRes, {
    'create status is 201': (r) => r.status === 201,
  });

  sleep(1);
}

export function teardown(data) {
  // Выполняется после теста
  console.log('Test completed');
}
```

### Запуск k6

```bash
# Запуск теста
k6 run load_test.js

# Запуск с выводом в InfluxDB
k6 run --out influxdb=http://localhost:8086/k6 load_test.js

# Запуск с облаком Grafana
k6 cloud load_test.js
```

### Продвинутые возможности k6

#### Кастомные метрики

```javascript
import { Trend, Counter, Rate } from 'k6/metrics';

const experimentCreationTime = new Trend('experiment_creation_time');
const experimentsCreated = new Counter('experiments_created_total');
const experimentCreationRate = new Rate('experiment_creation_success');

export default function (data) {
  const startTime = Date.now();

  const res = http.post(
    `${BASE_URL}/api/v1/experiments`,
    JSON.stringify({ name: 'Test', project_id: 1 }),
    { headers: { ... } }
  );

  const duration = Date.now() - startTime;
  experimentCreationTime.add(duration);

  if (res.status === 201) {
    experimentsCreated.add(1);
    experimentCreationRate.add(1);
  } else {
    experimentCreationRate.add(0);
  }
}
```

#### Параметризация

```javascript
import { SharedArray } from 'k6/data';

const experiments = new SharedArray('experiments', function () {
  return JSON.parse(open('./experiments.json'));
});

export default function (data) {
  const experiment = experiments[__VU % experiments.length];
  http.get(`${BASE_URL}/api/v1/experiments/${experiment.id}`, params);
}
```

#### Проверки и thresholds

```javascript
export const options = {
  thresholds: {
    // HTTP метрики
    http_req_duration: ['p(95)<500', 'p(99)<1000'],
    http_req_failed: ['rate<0.01'],
    http_req_waiting: ['p(95)<300'],

    // Кастомные метрики
    experiment_creation_time: ['p(95)<200'],
    experiment_creation_success: ['rate>0.95'],

    // Группировка по тегам
    'http_req_duration{endpoint:/api/v1/experiments}': ['p(95)<300'],
    'http_req_duration{endpoint:/api/v1/runs}': ['p(95)<500'],
  },
};

export default function (data) {
  const res = http.get(
    `${BASE_URL}/api/v1/experiments`,
    { tags: { endpoint: '/api/v1/experiments' } }
  );

  check(res, {
    'status is 200': (r) => r.status === 200,
    'response time < 500ms': (r) => r.timings.duration < 500,
  });
}
```

## Анализ результатов

### Что смотреть в результатах?

1. **Throughput (RPS/TPS)**
   - Достаточно ли запросов обрабатывается?
   - Есть ли деградация при росте нагрузки?

2. **Latency (Response Time)**
   - P50, P95, P99 перцентили
   - Есть ли выбросы?
   - Деградация при росте нагрузки?

3. **Error Rate**
   - Какой процент ошибок?
   - Какие типы ошибок?
   - Когда появляются ошибки?

4. **Resource Usage**
   - CPU, Memory, I/O
   - Узкие места в ресурсах?

### Интерпретация результатов Locust

```
Name                                                           # reqs      # fails  |     Avg     Min     Max  |  Median   req/s
----------------------------------------------------------------------------------------------------------------------------------------
GET /api/v1/experiments                                        5000        0(0.00%)  |     45     20     200  |     42    50.00
POST /api/v1/experiments                                       1000        5(0.50%)  |    120     50     500  |    110    10.00
GET /api/v1/experiments/1                                      2000        0(0.00%)  |     30     15     100  |     28    20.00
----------------------------------------------------------------------------------------------------------------------------------------
Total                                                          8000        5(0.06%)                                80.00 req/s
```

**Анализ:**
- GET /experiments: хорошо (низкая latency, нет ошибок)
- POST /experiments: есть ошибки (0.5%), высокая latency
- GET /experiments/:id: хорошо

### Интерпретация результатов k6

```
     ✓ status is 200
     ✓ response has experiments
     ✓ create status is 201

     checks.........................: 100.00% ✓ 8000      ✗ 0
     data_received..................: 2.5 MB  41 kB/s
     data_sent......................: 1.2 MB  20 kB/s
     http_req_duration..............: avg=45ms    min=20ms   med=42ms   max=200ms   p(90)=80ms   p(95)=100ms
     http_req_failed................: 0.06%   ✓ 5         ✗ 7995
     http_reqs......................: 8000   133.33/s
     iteration_duration.............: avg=1.5s   min=1.0s   med=1.4s   max=3.0s
     iterations....................: 8000   133.33/s
     vus............................: 100     min=1      max=100
     vus_max........................: 100     min=100    max=100
```

## Оптимизация производительности

### Найденные проблемы и решения

#### 1. Высокая latency

**Проблема:** P95 latency > 1s

**Возможные причины:**
- Медленные запросы к БД
- N+1 проблема
- Отсутствие кэширования
- Медленные внешние вызовы

**Решения:**
- Оптимизация запросов к БД
- Добавление индексов
- Кэширование
- Параллельные запросы
- Connection pooling

#### 2. Высокий error rate

**Проблема:** >1% ошибок

**Возможные причины:**
- Недостаточно ресурсов
- Таймауты
- Проблемы с БД
- Ограничения rate limiting

**Решения:**
- Увеличение ресурсов
- Настройка таймаутов
- Оптимизация БД
- Увеличение лимитов

#### 3. Низкий throughput

**Проблема:** RPS меньше ожидаемого

**Возможные причины:**
- Узкое место в коде
- Блокирующие операции
- Недостаточно параллелизма

**Решения:**
- Асинхронная обработка
- Параллелизм
- Оптимизация алгоритмов

### Пример оптимизации

**До оптимизации:**
```
POST /api/v1/experiments: avg=500ms, p95=800ms, errors=2%
```

**Проблемы:**
- Медленные запросы к БД
- Последовательные вызовы внешних сервисов
- Нет кэширования

**Оптимизации:**
```python
# Было (последовательно)
experiment = await create_experiment(data)
metrics = await init_metrics(experiment['id'])
resources = await allocate_resources(experiment['id'])

# Стало (параллельно)
experiment = await create_experiment(data)
metrics, resources = await asyncio.gather(
    init_metrics(experiment['id']),
    allocate_resources(experiment['id'])
)
```

**После оптимизации:**
```
POST /api/v1/experiments: avg=150ms, p95=250ms, errors=0%
```

## Best Practices

### 1. Начинайте с малой нагрузки

```python
# Постепенное увеличение
stages = [
    {'duration': '1m', 'target': 10},   # Разминка
    {'duration': '2m', 'target': 50},     # Постепенный рост
    {'duration': '5m', 'target': 100},    # Целевая нагрузка
]
```

### 2. Тестируйте реалистичные сценарии

```python
# ✅ ХОРОШО - реалистичные пропорции
@task(10)  # Читаем чаще
def get_experiment(self):
    ...

@task(2)   # Создаем реже
def create_experiment(self):
    ...
```

### 3. Используйте реальные данные

```python
# Загружайте данные из файлов
with open('experiment_ids.json') as f:
    self.experiment_ids = json.load(f)
```

### 4. Мониторьте во время теста

```python
# Используйте Prometheus метрики во время теста
# Смотрите CPU, Memory, Database connections
```

### 5. Тестируйте разные сценарии

```python
# Разные типы тестов
# - Нормальная нагрузка
# - Пиковая нагрузка
# - Стресс-тест
```

## Практический пример: Полный тест

### Locust тест для Experiment Service

```python
# locustfile.py
from locust import HttpUser, task, between, events
import random
import json

class ExperimentAPIUser(HttpUser):
    wait_time = between(1, 3)

    def on_start(self):
        """Инициализация пользователя."""
        # Логин
        response = self.client.post(
            "/api/v1/auth/login",
            json={"username": "test_user", "password": "password"}
        )
        if response.status_code == 200:
            self.token = response.json()["access_token"]
            self.client.headers = {"Authorization": f"Bearer {self.token}"}

            # Загружаем ID экспериментов
            exp_response = self.client.get("/api/v1/experiments")
            if exp_response.status_code == 200:
                experiments = exp_response.json()
                self.experiment_ids = [e['id'] for e in experiments[:10]]
            else:
                self.experiment_ids = [1, 2, 3, 4, 5]
        else:
            # Fallback для тестирования без авторизации
            self.experiment_ids = [1, 2, 3, 4, 5]

    @task(5)
    def list_experiments(self):
        """Список экспериментов (часто читаем)."""
        with self.client.get(
            "/api/v1/experiments",
            catch_response=True,
            name="GET /experiments"
        ) as response:
            if response.status_code == 200:
                response.success()
            else:
                response.failure(f"Status {response.status_code}")

    @task(3)
    def get_experiment(self):
        """Получение эксперимента по ID."""
        experiment_id = random.choice(self.experiment_ids)
        with self.client.get(
            f"/api/v1/experiments/{experiment_id}",
            catch_response=True,
            name="GET /experiments/:id"
        ) as response:
            if response.status_code == 200:
                response.success()
            else:
                response.failure(f"Status {response.status_code}")

    @task(1)
    def create_experiment(self):
        """Создание эксперимента (реже создаем)."""
        with self.client.post(
            "/api/v1/experiments",
            json={
                "name": f"Load Test Experiment {random.randint(1, 10000)}",
                "project_id": 1,
                "config": {}
            },
            catch_response=True,
            name="POST /experiments"
        ) as response:
            if response.status_code == 201:
                data = response.json()
                if 'id' in data:
                    self.experiment_ids.append(data['id'])
                    response.success()
                else:
                    response.failure("No ID in response")
            else:
                response.failure(f"Status {response.status_code}")
```

### k6 тест для Experiment Service

```javascript
// load_test.js
import http from 'k6/http';
import { check, sleep } from 'k6';
import { Rate, Trend } from 'k6/metrics';

const errorRate = new Rate('errors');
const experimentCreationTime = new Trend('experiment_creation_time');

export const options = {
  stages: [
    { duration: '1m', target: 50 },
    { duration: '3m', target: 50 },
    { duration: '1m', target: 100 },
    { duration: '3m', target: 100 },
    { duration: '1m', target: 0 },
  ],
  thresholds: {
    http_req_duration: ['p(95)<500', 'p(99)<1000'],
    http_req_failed: ['rate<0.01'],
    errors: ['rate<0.01'],
  },
};

const BASE_URL = __ENV.BASE_URL || 'http://localhost:8000';

export function setup() {
  const loginRes = http.post(
    `${BASE_URL}/api/v1/auth/login`,
    JSON.stringify({
      username: 'test_user',
      password: 'password',
    }),
    { headers: { 'Content-Type': 'application/json' } }
  );

  if (loginRes.status !== 200) {
    throw new Error(`Login failed: ${loginRes.status}`);
  }

  return {
    token: loginRes.json('access_token'),
  };
}

export default function (data) {
  const params = {
    headers: {
      'Authorization': `Bearer ${data.token}`,
    },
    tags: { name: 'ExperimentAPI' },
  };

  // Список экспериментов
  const listRes = http.get(`${BASE_URL}/api/v1/experiments`, params);
  const listSuccess = check(listRes, {
    'list status is 200': (r) => r.status === 200,
    'list response time < 500ms': (r) => r.timings.duration < 500,
  });
  errorRate.add(!listSuccess);
  sleep(1);

  // Создание эксперимента
  const createStart = Date.now();
  const createRes = http.post(
    `${BASE_URL}/api/v1/experiments`,
    JSON.stringify({
      name: `Load Test Experiment ${__VU}-${__ITER}`,
      project_id: 1,
      config: {},
    }),
    {
      ...params,
      headers: {
        ...params.headers,
        'Content-Type': 'application/json',
      },
    }
  );
  const createDuration = Date.now() - createStart;
  experimentCreationTime.add(createDuration);

  const createSuccess = check(createRes, {
    'create status is 201': (r) => r.status === 201,
    'create response time < 500ms': (r) => r.timings.duration < 500,
  });
  errorRate.add(!createSuccess);
  sleep(1);
}
```

## Дополнительные материалы

### Полезные ссылки
- [Locust Documentation](https://docs.locust.io/)
- [k6 Documentation](https://k6.io/docs/)
- [Load Testing Best Practices](https://k6.io/docs/test-types/)

### Инструменты
- [Apache JMeter](https://jmeter.apache.org/) - альтернатива
- [Artillery](https://www.artillery.io/) - еще одна альтернатива
- [wrk](https://github.com/wg/wrk) - простой инструмент

### Статьи
- [Load Testing Guide](https://k6.io/docs/test-types/load-testing/)
- [Performance Testing Strategies](https://www.softwaretestinghelp.com/performance-testing-tutorial/)

## Вопросы для самопроверки

1. В чем разница между Load Testing и Stress Testing?
2. Какие метрики важны при нагрузочном тестировании?
3. Когда использовать Locust, а когда k6?
4. Как интерпретировать P95 latency?
5. Какие типы оптимизаций возможны после нагрузочного тестирования?

## Следующая неделя

На [Неделе 28](../week-28/README.md) начинаем финальный проект - применение всех изученных техник! 🚀

---

**Удачи с нагрузочным тестированием! 💪**

