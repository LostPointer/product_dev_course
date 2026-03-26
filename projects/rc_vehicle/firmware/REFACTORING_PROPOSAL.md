# Firmware Refactoring — Status

560 тестов (544 unit + 16 integration), 0 глобальных классов, `main.cpp` 139 строк.
`VehicleControlUnified.cpp` — 265 строк (цель <200).

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
| ControlLoopProcessor | Тело `ControlTaskLoop` (~220 строк) → отдельный класс с методами Step/UpdateComponents/UpdateSensorsAndEkf/UpdateAutoDrive/UpdateStabilization/HandleFailsafe/UpdatePwm/UpdateTelemetry |
| CoM Offset Calibration | Круговая калибровка CW+CCW с вычислением смещения IMU |
| CoM Accel Correction | Коррекция акселерометра: центростремительная + тангенциальная |
| TestRunner | Фреймворк авто-тестов (Straight/Circle) |
| Steering Trim Calibration | Авто-калибровка trim + fix `IsActive()` |
| Web UI Charts | Canvas-графики телеметрии в реальном времени |
| Binary Telemetry HTTP | Бинарный эндпоинт `/log.bin` |

---

## Текущий долг: VCU — 473 строк → цель <200

| Блок в `.cpp` | Строк |
|---------------|-------|
| `ControlTaskLoop()` | 22 (теперь только while + watchdog) |
| `InitializeComponents()` | ~43 |
| `InitImuSubsystem()` | ~38 |
| `Init()` | ~38 |
| Делегаторы (Start*, RunSelfTest, OnWifi) | ~30 |

---

## Открытые пункты

| Пункт | Приоритет | Описание |
|-------|-----------|----------|
| `InitializeComponents` split | Низкий | Незначительный выигрыш (~10 строк) |
| `test_ws_command_registry.cpp` | Низкий | Требует mock `esp_http_server.h` |
