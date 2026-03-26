# Firmware Refactoring — Status

560 тестов (544 unit + 16 integration), 0 глобальных классов, `main.cpp` 139 строк.
`VehicleControlUnified.cpp` — 473 строк (цель <200).

---

## Завершённые задачи

| Задача | Описание |
|--------|----------|
| Phase 6.3 | 16 интеграционных тестов через `SimPlatform`/`FakePlatform` |
| Phase 6.4 | WS Handler DI — 22 хендлера получают `IVehicleControl&` через параметр |
| AutoDriveCoordinator | 4 авто-процедуры с централизованным взаимным исключением |
| TelemetryBuilder | `BuildTelemetrySnapshot` / `BuildLogFrame` → free functions |
| DiagnosticsReporter | `PrintDiagnostics` → free function |
| VehicleEkf::UpdateFromImu | Predict + GyroZ + ZUPT за один вызов |
| ControlLoopHelpers | `BuildSensorSnapshot`, `CorrectImuForComOffset`, `BuildAutoDriveInput`, `SelectControlSource`, `UpdatePwmWithSlewRate`, `HandleAutoDriveCompletion`, `BuildSelfTestInput` → free functions |
| InitImuSubsystem / InitTelemetryLog | Блоки инициализации извлечены из `Init()` |
| CoM Offset Calibration | Круговая калибровка CW+CCW с вычислением смещения IMU |
| CoM Accel Correction | Коррекция акселерометра: центростремительная + тангенциальная |
| TestRunner | Фреймворк авто-тестов (Straight/Circle) |
| Steering Trim Calibration | Авто-калибровка trim + fix `IsActive()` |
| Web UI Charts | Canvas-графики телеметрии в реальном времени |
| Binary Telemetry HTTP | Бинарный эндпоинт `/log.bin` |

---

## Текущий долг: VCU — 473 строк → цель <200

| Блок в `.cpp` | Строк | Следующий шаг |
|---------------|-------|---------------|
| `ControlTaskLoop()` | ~220 | Извлечь `ControlLoopProcessor` (method-object) |
| `InitializeComponents()` | ~43 | Можно разбить на `InitHandlers()` + `InitStabilizers()` |
| `InitImuSubsystem()` | ~38 | Уже извлечён, в порядке |
| `Init()` | ~38 | Уже компактный |
| Делегаторы (Start*, RunSelfTest, OnWifi) | ~35 | Уже по 1–3 строки |

### Путь к <200 строк

Главный блок — `ControlTaskLoop()` (~220 строк). Для его сокращения:

1. **`ControlLoopProcessor`** — class, владеющий одной итерацией цикла.
   - Хранит ссылки на все подсистемы (platform, stab_mgr, calib_mgr, ...).
   - Метод `Step(uint32_t now, uint32_t dt_ms)` заменяет тело цикла.
   - `ControlTaskLoop()` сводится к инициализации переменных + `while(true) { processor.Step(...); }`.
   - Результат: VCU .cpp ~130 строк.

2. **`InitializeComponents()`** → `InitHandlers()` + `InitStabilizers()` (-10 строк).

---

## Открытые пункты

| Пункт | Приоритет | Описание |
|-------|-----------|----------|
| `ControlLoopProcessor` | Высокий | Сократит VCU до <200 строк |
| `InitializeComponents` split | Низкий | Незначительный выигрыш |
| `test_ws_command_registry.cpp` | Низкий | Требует mock `esp_http_server.h` |
