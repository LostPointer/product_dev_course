# Как посмотреть логи в Alloy

Alloy предоставляет несколько способов для мониторинга и просмотра логов.

## 1. Логи самого контейнера Alloy

### Просмотр логов контейнера

```bash
# Последние 50 строк логов
docker logs alloy --tail 50

# Логи в реальном времени (follow)
docker logs alloy -f

# Логи за последние 10 минут
docker logs alloy --since 10m

# Логи с определенного времени
docker logs alloy --since "2024-01-12T10:00:00"

# Фильтрация по уровню (error, warn, info)
docker logs alloy 2>&1 | grep -i "error"
docker logs alloy 2>&1 | grep -i "warn"
```

### Типичные сообщения в логах

**Успешная работа:**
```
level=info msg="finished node evaluation" node_id=discovery.docker.containers
level=info msg="now listening for http traffic" service=http addr=0.0.0.0:12345
```

**Ошибки:**
```
level=error msg="failed to evaluate config" err="decoding configuration: ..."
level=error msg="final error sending batch" error="server returned HTTP status 400"
level=warn msg="could not transfer logs" err="read unix @->/run/docker.sock: use of closed network connection"
```

## 2. Метрики Alloy (Prometheus)

Alloy экспортирует метрики в формате Prometheus на порту 12345.

### Просмотр метрик

```bash
# Все метрики
curl http://localhost:12345/metrics

# Метрики сбора логов
curl http://localhost:12345/metrics | grep loki_source_docker

# Метрики отправки в Loki
curl http://localhost:12345/metrics | grep loki_write

# Количество собранных логов
curl http://localhost:12345/metrics | grep loki_source_docker_target_entries_total

# Количество отправленных логов
curl http://localhost:12345/metrics | grep loki_write_sent_entries_total

# Количество ошибок
curl http://localhost:12345/metrics | grep loki_write_dropped_entries_total
```

### Важные метрики

- `loki_source_docker_target_entries_total` - общее количество собранных логов
- `loki_write_sent_entries_total` - количество успешно отправленных логов в Loki
- `loki_write_dropped_entries_total` - количество отброшенных логов (ошибки)
- `loki_source_docker_target_parsing_errors_total` - ошибки парсинга

## 3. HTTP API Alloy

Alloy предоставляет REST API для получения информации о компонентах.

### Проверка статуса

```bash
# Проверка доступности
curl http://localhost:12345/-/healthy

# Информация о компонентах
curl http://localhost:12345/api/v0/components

# Экспорт конфигурации компонента
curl http://localhost:12345/api/v0/components/export/discovery.docker.containers
curl http://localhost:12345/api/v0/components/export/loki.source.docker.docker_logs
```

### Примеры использования API

```bash
# Получить список всех компонентов
curl -s http://localhost:12345/api/v0/components | python3 -m json.tool

# Проверить, какие контейнеры видит Alloy
curl -s http://localhost:12345/api/v0/components/export/discovery.docker.containers | \
  python3 -c "import sys, json; data=json.load(sys.stdin); \
  targets=data.get('discovery.docker.containers', {}).get('export', {}).get('targets', []); \
  print(f'Найдено контейнеров: {len(targets)}'); \
  [print(f\"  - {t.get('__meta_docker_container_name', 'unknown')}\") for t in targets[:10]]"
```

## 4. Веб-интерфейс Alloy (UI)

Alloy имеет встроенный веб-интерфейс для просмотра компонентов и их состояния.

### Доступ к UI

Откройте в браузере: **http://localhost:12345**

В UI можно:
- Просмотреть все компоненты и их состояние
- Посмотреть метрики компонентов
- Проверить конфигурацию
- Просмотреть граф компонентов

### Live Debugging

**Важно:** Live debugging может быть недоступен в стандартной сборке Alloy v1.12.2. Эта функция требует специальной сборки с определенными тегами компиляции.

Если вы получаете ошибку:
```
Error: Failed to connect, status code: 500, reason: the live debugging service is disabled
```

Это означает, что live debugging не поддерживается в текущей версии Alloy.

#### Альтернативные методы отладки:

1. **Используйте веб-интерфейс Alloy**: http://localhost:12345
   - Просмотр компонентов и их состояния
   - Метрики компонентов
   - Конфигурация

2. **Используйте REST API**:
   ```bash
   # Список компонентов
   curl http://localhost:12345/api/v0/components

   # Экспорт компонента
   curl http://localhost:12345/api/v0/components/export/discovery.docker.containers
   ```

3. **Используйте метрики Prometheus**:
   ```bash
   curl http://localhost:12345/metrics | grep loki_source_docker
   ```

4. **Просматривайте логи контейнера**:
   ```bash
   docker logs alloy -f
   ```

#### Если нужен live debugging:

Проверьте документацию Alloy для вашей версии:
- https://grafana.com/docs/alloy/latest/
- Возможно, потребуется специальная сборка Alloy с поддержкой live debugging

## 5. Проверка работы Alloy

### Быстрая проверка

```bash
# 1. Проверка, что контейнер запущен
docker ps --filter "name=alloy"

# 2. Проверка логов на ошибки
docker logs alloy --tail 50 2>&1 | grep -i error

# 3. Проверка метрик
curl -s http://localhost:12345/metrics | grep loki_source_docker_target_entries_total

# 4. Проверка доступности API
curl http://localhost:12345/-/healthy
```

### Проверка сбора логов

```bash
# Генерируем новые логи
curl http://localhost:8002/health
curl http://localhost:8001/health

# Ждем несколько секунд и проверяем метрики
sleep 5
curl -s http://localhost:12345/metrics | grep loki_source_docker_target_entries_total
```

## 6. Диагностика проблем

### Alloy не собирает логи

```bash
# Проверьте конфигурацию
docker exec alloy cat /etc/alloy/config.river

# Проверьте доступ к Docker socket
docker exec alloy ls -la /var/run/docker.sock

# Проверьте доступ к логам контейнеров
docker exec alloy ls -la /var/lib/docker/containers/ | head -5

# Проверьте логи Alloy на ошибки
docker logs alloy 2>&1 | grep -i "error\|failed\|unable"
```

### Alloy не отправляет логи в Loki

```bash
# Проверьте метрики отправки
curl -s http://localhost:12345/metrics | grep loki_write

# Проверьте ошибки отправки
docker logs alloy 2>&1 | grep -i "loki\|error\|failed"

# Проверьте доступность Loki
docker exec alloy wget -qO- http://loki:3100/ready
```

### Проверка конфигурации

```bash
# Просмотр конфигурации
docker exec alloy cat /etc/alloy/config.river

# Проверка синтаксиса (если Alloy запущен, синтаксис корректен)
docker logs alloy 2>&1 | grep -i "error.*config"
```

## 7. Полезные команды

### Мониторинг в реальном времени

```bash
# Логи Alloy + метрики
watch -n 2 'echo "=== Logs ===" && docker logs alloy --tail 5 && \
  echo -e "\n=== Metrics ===" && \
  curl -s http://localhost:12345/metrics | grep loki_source_docker_target_entries_total'
```

### Сравнение метрик

```bash
# Сохранить текущие метрики
curl -s http://localhost:12345/metrics > /tmp/alloy_metrics_before.txt

# ... выполнить действия ...

# Сравнить метрики
curl -s http://localhost:12345/metrics > /tmp/alloy_metrics_after.txt
diff /tmp/alloy_metrics_before.txt /tmp/alloy_metrics_after.txt | grep loki_source_docker
```

### Экспорт конфигурации

```bash
# Сохранить конфигурацию Alloy
docker exec alloy cat /etc/alloy/config.river > alloy_config_backup.river
```

## 8. Интеграция с Grafana

Метрики Alloy можно импортировать в Grafana для мониторинга.

### Добавление Alloy как источника метрик

1. В Grafana: Configuration → Data Sources → Add data source
2. Выберите Prometheus
3. URL: `http://alloy:12345`
4. Сохраните

### Примеры запросов в Grafana

```promql
# Количество собранных логов
rate(loki_source_docker_target_entries_total[5m])

# Количество ошибок отправки
rate(loki_write_dropped_entries_total[5m])

# Ошибки парсинга
rate(loki_source_docker_target_parsing_errors_total[5m])
```

## 9. Структура логов Alloy

Логи Alloy имеют следующий формат:

```
ts=2024-01-12T10:00:00.000Z level=info msg="message" component_path=/ component_id=... node_id=...
```

Где:
- `ts` - timestamp
- `level` - уровень (info, warn, error)
- `msg` - сообщение
- `component_path` - путь компонента
- `component_id` - ID компонента
- `node_id` - ID узла в графе

## 10. Частые проблемы и решения

### Проблема: "use of closed network connection"

**Причина**: Alloy пытается читать логи из остановленных контейнеров

**Решение**: Это нормально при перезапуске контейнеров. Alloy автоматически восстановится.

### Проблема: "entry too far behind"

**Причина**: Alloy пытается отправить старые логи, которые Loki отклоняет

**Решение**: Это нормально. Loki отклоняет логи старше retention периода (7 дней).

### Проблема: "could not transfer logs"

**Причина**: Проблема с доступом к Docker socket или файлам логов

**Решение**:
```bash
# Проверьте права доступа
docker exec alloy ls -la /var/run/docker.sock
docker exec alloy ls -la /var/lib/docker/containers/ | head -3
```

## Полезные ссылки

- [Alloy Documentation](https://grafana.com/docs/alloy/latest/)
- [Alloy API Reference](https://grafana.com/docs/alloy/latest/reference/http/)
- [Alloy Metrics](https://grafana.com/docs/alloy/latest/reference/metrics/)
