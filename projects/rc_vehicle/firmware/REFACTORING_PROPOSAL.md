# Firmware Refactoring Proposal

## Выполненные фазы (сводка)

| Фаза | Содержание | Статус |
|------|-----------|--------|
| Phase 1 | Magic numbers → `config.hpp`, static → instance в `DelayUntilNextTick`, namespace consistency | ✅ |
| Phase 2 | `StabilizationConfig` nested structs (`PidConfig`, `FilterConfig`, etc.), JSON serialization | ✅ |
| Phase 3 | Extract `CalibrationManager`, `StabilizationManager`, `TelemetryManager`; `WsCommandRegistry`; `VehicleControl` wrapper removal | ✅ |
| Phase 4 | `Result<T,E>` в `result.hpp`, platform interface на `Result`, item 3 (error conversion) — пропущен как избыточный | ✅ 95% |
| Phase 6.1 | Atomic config snapshot — `GetConfig()` вызывается 1 раз за итерацию control loop | ✅ |
| Phase 6.2 | Sensor Snapshot Pattern — `SensorSnapshot` struct, все датчики читаются один раз за итерацию | ✅ |
| TelemetryHandler | Убран `ControlComponent`, `Update()` → `SendTelemetry()`, добавлен `wifi_cmd` в snapshot | ✅ |
| Phase 5 | Убран singleton `Instance()`, конструктор public, экземпляр в ESP32-слое (`vehicle_control.hpp`) | ✅ |
| Unit-тесты | `test_calibration_manager.cpp` (13), `test_stabilization_manager.cpp` (11), `test_telemetry_manager.cpp` (10) | ✅ |

---

## Открытые пункты

### Phase 6.3: Control Loop Simulator (Низкий приоритет)

Интеграционное тестирование control loop невозможно без реального железа. `ControlLoopSimulator` — прогоняет N итераций с `FakePlatform`, проверяя инварианты (failsafe, переходы режимов, limits).

Phase 5 (DI) выполнен — `VehicleControlUnified` теперь обычный класс с public-конструктором.

---

### Phase 6.4: WS Handler Dependency Injection (Низкий приоритет)

WS-хендлеры напрямую вызывают `VehicleControlUnified::Instance()`. Ввести интерфейс `IVehicleControl` и передавать ссылку при регистрации:

```cpp
registry.Register("control", [&vc](cJSON* json, httpd_req_t* req) {
  HandleControl(vc, json, req);
});
```

Phase 5 (DI) выполнен — singleton убран.

---

### Тесты: нереализованные

| Тест | Статус | Зависимости |
|------|--------|-------------|
| `test_ws_command_registry.cpp` | ❌ | Требует mock для `esp_http_server.h` |
| Integration: full control loop | ❌ | Phase 6.3 (simulator) |
| Integration: config persistence round-trip | ❌ | Phase 6.3 (simulator) |
| Integration: failsafe scenarios | ❌ | Phase 6.3 (simulator) |

---

## Метрики

| Metric | До рефакторинга | Цель | Текущее (март 2026) | Статус |
|--------|-----------------|------|---------------------|--------|
| `VehicleControlUnified` lines | ~600 | <200 | 645 | ❌ |
| `main.cpp` lines | 273 | <150 | 139 | ✅ |
| `StabilizationConfig` top-level fields | 30+ | 10 | ~10 | ✅ |
| Global classes | 5 | 0 | 0 | ✅ |
| Unit tests total | ~420 | >450 | 457 | ✅ |

**Примечание:** `VehicleControlUnified` вырос из-за новых фич (EKF, oversteer guard, kids mode, forward calib). Phase 6.2 (sensor snapshot) реализован — дальнейшее сокращение через вынесение формирования `TelemetryLogFrame` в отдельный метод.

---

## Приоритеты

1. **Phase 6.3 + 6.4** — когда потребуется интеграционное тестирование
2. **`test_ws_command_registry.cpp`** — требует mock `esp_http_server.h`
