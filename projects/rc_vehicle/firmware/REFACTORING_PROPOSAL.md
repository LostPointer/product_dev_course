# Firmware Refactoring — Status

Phases 1–6.4 + Phase 5 (DI) завершены. 560 тестов (544 unit + 16 integration), 0 глобальных классов, `main.cpp` 139 строк.

## Завершённые задачи (последняя серия)

| Задача | Описание |
|--------|----------|
| Phase 6.3 | 16 интеграционных тестов через `SimPlatform`/`FakePlatform`: failsafe, RC/Wi-Fi приоритет, config round-trip, kids mode, self-test, telemetry log, TestRunner, CoM calibration, steering trim calibration, взаимное исключение процедур |
| Phase 6.4 | WS Handler DI — все 22 хендлера получают `IVehicleControl&` через параметр вместо глобального singleton. `vehicle_control.hpp` очищен от обёрток (осталось только `Init` и `OnWifiCommand`) |
| AutoDriveCoordinator | Извлечение 4 авто-процедур из VCU в отдельный класс с централизованным взаимным исключением |
| CoM Offset Calibration | Круговая калибровка CW+CCW с вычислением смещения IMU относительно центра масс |
| CoM Accel Correction | Коррекция акселерометра в control loop: центростремительная + тангенциальная составляющие |
| TestRunner | Фреймворк авто-тестов (Straight/Circle) с взаимным исключением |
| Steering Trim Calibration | Авто-калибровка trim руля + fix `IsActive()` (исключение Done/Failed фаз) |
| Web UI Charts | Canvas-графики телеметрии в реальном времени (акселерометр, гироскоп, EKF, управление) |
| Binary Telemetry HTTP | Бинарный эндпоинт `/log.bin` для быстрого скачивания телеметрии |

## Открытые пункты

| Пункт | Приоритет | Описание |
|-------|-----------|----------|
| `test_ws_command_registry.cpp` | Низкий | Требует mock `esp_http_server.h` |

## Известные долги

`VehicleControlUnified` — 765 строк (цель <200). Следующие шаги: вынести `BuildTelemetrySnapshot`/`BuildLogFrame` в TelemetryBuilder, EKF-блок в VehicleEkf, `PrintDiagnostics` в отдельный класс.
