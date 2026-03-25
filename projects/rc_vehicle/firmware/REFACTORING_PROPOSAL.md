# Firmware Refactoring — Status

Phases 1–6.3 + Phase 5 (DI) завершены. 560 тестов (544 unit + 16 integration), 0 глобальных классов, `main.cpp` 139 строк.

## Завершённые задачи (последняя серия)

| Задача | Описание |
|--------|----------|
| Phase 6.3 | 16 интеграционных тестов через `SimPlatform`/`FakePlatform`: failsafe, RC/Wi-Fi приоритет, config round-trip, kids mode, self-test, telemetry log, TestRunner, CoM calibration, steering trim calibration, взаимное исключение процедур |
| CoM Offset Calibration | Круговая калибровка CW+CCW с вычислением смещения IMU относительно центра масс |
| CoM Accel Correction | Коррекция акселерометра в control loop: центростремительная + тангенциальная составляющие |
| TestRunner | Фреймворк авто-тестов (Straight/Circle) с взаимным исключением |
| Steering Trim Calibration | Авто-калибровка trim руля + fix `IsActive()` (исключение Done/Failed фаз) |
| Web UI Charts | Canvas-графики телеметрии в реальном времени (акселерометр, гироскоп, EKF, управление) |
| Binary Telemetry HTTP | Бинарный эндпоинт `/log.bin` для быстрого скачивания телеметрии |

## Открытые пункты

| Пункт | Приоритет | Описание |
|-------|-----------|----------|
| Phase 6.4 | Низкий | WS Handler DI — передавать `IVehicleControl&` при регистрации вместо прямого вызова |
| `test_ws_command_registry.cpp` | Низкий | Требует mock `esp_http_server.h` |

## Известные долги

`VehicleControlUnified` — 868 строк (цель <200). Вырос из-за EKF, oversteer guard, kids mode, forward calib, CoM correction, test runner, CoM calibration. Следующий шаг: вынести формирование `TelemetryLogFrame` и auto-drive координацию в отдельные классы.
