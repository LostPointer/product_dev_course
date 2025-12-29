# TODO

## Архитектура и инфраструктура

### Разделение управления схемами БД от сервисов

**Проблема:** В настоящее время сервисы (auth-service, experiment-service) применяют миграции БД при старте приложения в функции `apply_migrations_on_startup`.

**Требование:** Сервисы не должны работать со схемами БД напрямую. Схемы должны применяться отдельно, например:
- При создании контейнера (init container в Kubernetes)
- Отдельным шагом в docker-compose
- Отдельным миграционным job'ом

**Задачи:**
- [ ] Удалить функцию `apply_migrations_on_startup` из `auth-service/src/auth_service/main.py`
- [ ] Удалить функцию `apply_migrations_on_startup` из `experiment-service/src/experiment_service/main.py`
- [ ] Удалить вызов `app.on_startup.append(apply_migrations_on_startup)` из обоих сервисов
- [ ] Настроить применение миграций через отдельный init container или миграционный job
- [ ] Обновить docker-compose.yml для применения миграций отдельным шагом
- [ ] Обновить документацию по развертыванию

**Текущее состояние:**
- `auth-service/src/auth_service/main.py` - применяет миграции на строках 44-161, регистрируется на строке 193
- `experiment-service/src/experiment_service/main.py` - применяет миграции на строках 47-163, регистрируется на строке 191
- Оба сервиса имеют скрипты миграций в `bin/migrate.py`, которые можно использовать отдельно

