## Описание
Добавлена поддержка CORS (Cross-Origin Resource Sharing) для experiment-service и исправлена совместимость версий для auth-proxy. Это позволяет frontend приложению (experiment-portal) на `http://localhost:3000` делать запросы к backend API без ошибок CORS.

## Тип изменений
- [x] Новая функциональность (feat)
- [x] Исправление бага (fix)
- [ ] Рефакторинг (refactor)
- [ ] Документация (docs)
- [ ] Тесты (test)
- [ ] CI/CD (chore)
- [ ] Performance оптимизация

## Что было сделано

### Experiment Service (Backend)
- ✅ Добавлена библиотека `aiohttp-cors` для поддержки CORS в aiohttp приложении
- ✅ Настроен CORS middleware для всех routes с поддержкой:
  - Настраиваемых allowed origins через environment variable
  - Credentials (cookies, authorization headers)
  - Всех HTTP методов (GET, POST, PUT, DELETE, etc.)
  - Всех headers
- ✅ Добавлена настройка `CORS_ALLOWED_ORIGINS` в settings с валидатором для парсинга comma-separated строки
- ✅ Обновлен `env.example` с документацией новой настройки
- ✅ Добавлен `# type: ignore[import-untyped]` для mypy (aiohttp-cors не имеет type stubs)

### Auth Proxy (BFF)
- ✅ Исправлена совместимость версий: downgrade `@fastify/cors` и `@fastify/rate-limit` до v9 для совместимости с Fastify 4.x

## Как тестировать

### Локальное тестирование
1. Убедитесь, что experiment-service запущен на `http://localhost:8002`
2. Убедитесь, что frontend (experiment-portal) запущен на `http://localhost:3000`
3. Откройте браузер и перейдите на `http://localhost:3000`
4. Проверьте, что запросы к API проходят без ошибок CORS в консоли браузера
5. Проверьте Network tab в DevTools - заголовки `Access-Control-Allow-Origin` должны присутствовать

### Через curl
```bash
# Проверка CORS preflight запроса
curl -X OPTIONS http://localhost:8002/experiments \
  -H "Origin: http://localhost:3000" \
  -H "Access-Control-Request-Method: GET" \
  -v

# Должны быть заголовки:
# Access-Control-Allow-Origin: http://localhost:3000
# Access-Control-Allow-Methods: *
# Access-Control-Allow-Headers: *
```

### Настройка через environment variable
```bash
# В .env файле experiment-service
CORS_ALLOWED_ORIGINS=http://localhost:3000,http://localhost:8080,https://production.example.com
```

## Checklist
- [x] Код соответствует [Code Style Guide](../docs/code-style-guide.md)
- [ ] Добавлены/обновлены unit тесты
- [ ] Добавлены/обновлены integration тесты (если применимо)
- [x] Все тесты проходят локально (`make test-all`)
- [x] Линтеры проходят (`make lint`)
- [x] Код отформатирован (`make format`)
- [x] Добавлена/обновлена документация
- [x] Проведен self-review
- [x] Нет закомментированного кода
- [x] Нет debug prints/логов
- [x] Проверены edge cases (пустая строка, один origin, несколько origins)
- [x] Обработка ошибок добавлена (валидатор с fallback значениями)
- [x] Type hints используются

## Breaking Changes
- [ ] Да
- [x] Нет

Нет breaking changes. Добавлена новая функциональность с обратной совместимостью.

## Performance Impact
- [ ] Улучшает
- [ ] Ухудшает
- [x] Не влияет
- [ ] Не измерялось

CORS middleware добавляет минимальные overhead на обработку OPTIONS запросов и добавление заголовков в ответы. Влияние на производительность пренебрежимо мало.

## Связанные задачи
<!-- Ссылки на issues -->
Fixes #<!-- номер issue с CORS ошибкой, если есть -->

## Скриншоты (если применимо)
<!-- До: ошибка CORS в консоли браузера -->
<!-- После: успешные запросы без ошибок CORS -->

## Дополнительные комментарии

### Технические детали
- Использован `aiohttp-cors` версии 0.7.0 для совместимости с aiohttp 3.10+
- CORS настраивается через `ResourceOptions` для каждого allowed origin
- Валидатор использует `mode="before"` для корректного парсинга строки из environment variable до валидации типа
- По умолчанию разрешены `http://localhost:3000` и `http://localhost:8080` для локальной разработки

### Безопасность
- CORS origins настраиваются через environment variable, что позволяет ограничить доступ только нужным доменам
- В production рекомендуется указать только production домены
- Credentials разрешены для поддержки cookie-based аутентификации

## Для reviewer
- Проверьте корректность настройки CORS middleware
- Убедитесь, что валидатор правильно парсит comma-separated строку
- Проверьте, что все routes получают CORS заголовки
- Убедитесь, что нет security issues с слишком открытыми настройками CORS

---
**Автор:** @LostPointer
**Reviewers:** @<!-- запросите review у кого-то -->
