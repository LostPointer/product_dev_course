# UI Debugging (Portal): toast, trace_id, request_id

Этот документ — короткая инструкция как быстро дебажить проблемы “UI ↔ backend” без DevTools.

## 1) Debug toast на ошибки запросов

В Portal в dev-режиме при ошибках HTTP-запросов появляется toast снизу справа:

- **Короткий toast**: безопасное сообщение для пользователя (HTTP статус, путь, корреляционные id).
- **Debug toast**: доступен в dev, включает кнопку **Details** и **Copy**.

Внутри “Details”:

- request: method + url + params + headers (sanitized) + body (sanitized)
- response: status + headers + body (если есть)
- correlation: `trace_id` / `request_id` / `traceparent` (если доступно)

Секреты маскируются (token/password/cookie и т.п.).

## 2) Как связать UI-ошибку с логами

1. В toast нажмите **Copy**.
2. Возьмите `trace_id` или `request_id`.
3. Откройте Grafana Explore → Loki и выполните поиск:

```logql
{trace_id="<trace_id>"}
```

или

```logql
{request_id="<request_id>"}
```

Подробнее про лог-пайплайн: `docs/logging-flow.md`
Примеры фильтров: `docs/grafana-trace-filtering.md`

## 3) Быстрые подсказки

- Если видите `403 CSRF token missing or invalid` — проверьте, что запрос идёт через auth-proxy, и что есть cookie `csrf_token` + заголовок `X-CSRF-Token` (на POST/PUT/PATCH/DELETE).
- Если SSE не подключается — смотрите debug toast: там будет URL стрима + заголовки. Для auth-proxy SSE важны `X-Trace-Id`/`X-Request-Id`.

