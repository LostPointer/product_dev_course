# Общий код для ESP32 прошивок

Этот каталог содержит общий код для ESP‑IDF прошивок:

- `firmware/esp32/` (ESP32‑C3 шлюз: Wi‑Fi + Web + UART→MCU)
- `firmware/esp32_s3/` (ESP32‑S3 all‑in‑one: Wi‑Fi + Web + управление)

Сюда вынесены:

- `wifi_ap.*` — SoftAP с SSID по MAC
- `http_server.*` — HTTP сервер и REST API; статика берётся из `web/` и вшивается через `#embed`
- `websocket_server.*` — WebSocket сервер и рассылка телеметрии

Статические файлы веб‑интерфейса:

- `web/index.html`
- `web/style.css`
- `web/app.js`

Различия между прошивками (куда применять команды управления) реализуются через
колбэк `WebSocketSetCommandHandler(...)`.

