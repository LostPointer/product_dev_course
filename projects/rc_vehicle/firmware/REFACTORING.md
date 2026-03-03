# Предложения по рефакторингу firmware

Анализ кодовой базы: ~6600 строк кода + ~6600 строк тестов, 429 тестов.

---

## 1. God Class: `VehicleControlUnified` (923 строки, 9 ответственностей)

**Проблема:** Класс совмещает оркестрацию control loop, yaw PID, slip PID, pitch compensation, oversteer prediction, EKF интеграцию, калибровку, failsafe reset и telemetry log. `ControlTaskLoop()` — монолитный цикл на 250 строк.

**Предложение:** Выделить стадии control loop в отдельные классы-стратегии:

```
StabilizationPipeline (новый, orchestrator)
├── YawRateController      — yaw PID + adaptive scaling (строки 154–173)
├── PitchCompensator       — pitch compensation (строки 180–191)
├── SlipAngleController    — slip PID / drift assist (строки 197–207)
├── OversteerGuard         — oversteer detection + throttle cut (строки 213–227)
└── FailsafeHandler        — failsafe + reset всех PID (строки 233–250)
```

Каждый модуль реализует единый интерфейс `void Process(ControlState& state, float dt_sec)`, где `ControlState` — struct с `commanded_throttle`, `commanded_steering`, `stab_weight`, `mode_transition_weight`. `VehicleControlUnified` остаётся оркестратором, но `ControlTaskLoop()` сокращается до ~50 строк.

**Выигрыш:** Каждый модуль тестируется изолированно (сейчас у `VehicleControlUnified` ноль прямых тестов — integration test эмулирует loop вручную через `RunControlLoopTick()`).

---

## 2. Дублирование JSON-сериализации `StabilizationConfig` (3 места, ~300 строк)

**Проблема:** Одна и та же структура из 29 полей сериализуется/десериализуется:
- `main.cpp:122–176` — `get_stab_config` (50 строк `cJSON_Add*`)
- `main.cpp:177–342` — `set_stab_config` (110 строк `cJSON_Get*` + 50 строк `cJSON_Add*`)
- `control_components.cpp` — `BuildTelemJson()` (101 строка `ostringstream`)

Добавление нового поля требует правки в 3–4 местах.

**Предложение:** Добавить в `stabilization_config.hpp` (или `.cpp`) две функции:
```cpp
cJSON* StabilizationConfigToJson(const StabilizationConfig& cfg);
void   StabilizationConfigFromJson(StabilizationConfig& cfg, const cJSON* json);
```
`main.cpp` сократится на ~160 строк. `BuildTelemJson()` может переиспользовать `ToJson()` для вложенного блока `"stab"`.

---

## 3. Мёртвый код (4 файла, ~170 строк)

| Файл | Строк | Статус |
|------|-------|--------|
| `common/vehicle_control.hpp` | 47 | Старый класс, не включается нигде в build path |
| `common/base_component.hpp` | 17 | `BaseComponent` — не используется (`ControlComponent` — отдельный базовый класс) |
| `common/context.hpp` | 64 | Шаблонный реестр типов — нигде не используется |
| `common/config_common.hpp` | 33 | `#define`-макросы — не включаются `vehicle_control_unified.cpp` |

**Предложение:** Удалить. Они создают ложное впечатление использования (например, `config_common.hpp` определяет `WIFI_CMD_TIMEOUT_MS=250`, а реально используется `500` из анонимного namespace в `.cpp`).

---

## 4. Рассинхронизация констант (3 параллельных определения)

**Проблема:** Три независимых источника одних и тех же значений:

| Константа | `config_common.hpp` | `config.hpp` | `vehicle_control_unified.cpp` |
|---|---|---|---|
| WIFI_CMD_TIMEOUT | 250 ms | 500 ms | **500 ms** (используется) |
| SLEW_RATE_THROTTLE | 0.5 | **2.0** | **0.5** (используется) |
| SLEW_RATE_STEERING | 1.0 | **4.0** | **1.0** (используется) |

`config.hpp` содержит `constexpr` struct `VehicleConfig`, но он нигде не импортируется.

**Предложение:** Оставить единый источник — `config.hpp` с корректными значениями. `vehicle_control_unified.cpp` должен использовать `#include "config.hpp"` вместо локальных копий. Удалить `config_common.hpp`.

---

## 5. `StabilizationConfig` — отсутствие namespace + инлайн логика

**Проблема:**
- Struct в глобальном namespace (всё остальное в `rc_vehicle::`)
- 4 метода на ~120 строк инлайнены в header-only файле
- `mode` — сырой `uint8_t` вместо typed enum
- Magic number `0x53544142` повторяется 3 раза без именованной константы
- Нет поля версии для NVS-совместимости

**Предложение:**
```cpp
namespace rc_vehicle {

enum class DriveMode : uint8_t { Normal = 0, Sport = 1, Drift = 2 };

static constexpr uint32_t kStabilizationConfigMagic = 0x53544142;  // 'STAB'

struct StabilizationConfig {
  uint32_t magic{kStabilizationConfigMagic};
  uint8_t  version{1};  // для миграции NVS
  DriveMode mode{DriveMode::Normal};
  // ... поля ...
};

}  // namespace rc_vehicle
```
Вынести `IsValid()`, `Reset()`, `ApplyModeDefaults()`, `Clamp()` в `.cpp`.

---

## 6. `TelemetryHandler::BuildTelemJson()` — ручная сборка JSON через ostringstream

**Проблема:** 101 строка ручной конкатенации `oss << "\"key\":" << value << ","`. Хрупко (пропущенная запятая = невалидный JSON), дублирует структуру `get_stab_config`.

**Предложение:** Использовать `cJSON` (уже в проекте через ESP-IDF) и в `common/` коде. Или вынести формирование JSON в единую шаблонную функцию. Это также устранит проблему п.2.

---

## 7. `static uint32_t last_log_ms` внутри `ControlTaskLoop()`

**Проблема:** `vehicle_control_unified.cpp:273` — function-local `static` переменная. Невидима извне, не сбрасывается при failsafe, не тестируема.

**Предложение:** Перенести в member `VehicleControlUnified::last_log_ms_`, сбрасывать в failsafe-блоке.

---

## 8. Ручной `if/else` вместо `std::clamp` (pitch compensation)

Строки 185–188 — единственное место, где clamp выполняется вручную. Всё остальное использует `std::clamp`.

```cpp
// Было:
if (correction > max) correction = max;
if (correction < -max) correction = -max;
// Стало:
correction = std::clamp(correction, -max, max);
```

---

## 9. `TelemetryHandler` — чрезмерная связность (6 зависимостей в конструкторе)

**Проблема:** Конструктор принимает 6 const-ссылок + 2 указателя через setters. Каждое новое телеметрическое поле = изменение конструктора.

**Предложение:** Ввести `TelemetrySnapshot` struct, заполняемый в `ControlTaskLoop()`:
```cpp
struct TelemetrySnapshot {
  ImuData imu;
  float pitch_deg, roll_deg, yaw_deg;
  float filtered_gz;
  float ekf_vx, ekf_vy, slip_deg, speed_ms;
  float throttle, steering;
  bool oversteer;
  CalibStatus calib_status;
  // ...
};
```
`TelemetryHandler` получает только `const TelemetrySnapshot&` — полная декаплинг от остальных компонентов.

---

## 10. `esp32_s3/main/vehicle_control.hpp` — лишний слой-обёртка

**Проблема:** `VehicleControl` — singleton-обёртка над `VehicleControlUnified` singleton. Содержит 12 методов-делегатов + 12 `extern "C"` свободных функций. Двойной singleton — избыточная абстракция.

**Предложение:** Удалить класс `VehicleControl`, оставить только `extern "C"` функции, которые обращаются напрямую к `VehicleControlUnified::Instance()`.

---

## Приоритизация

| # | Рефакторинг | Сложность | Риск | Выигрыш |
|---|---|---|---|---|
| 3 | Удалить мёртвый код | Низкая | Минимальный | Чистота |
| 4 | Единый источник констант | Низкая | Минимальный | Корректность |
| 7 | `static` → member | Низкая | Минимальный | Тестируемость |
| 8 | `std::clamp` | Тривиальная | Нулевой | Консистентность |
| 2 | JSON-сериализация config | Средняя | Низкий | −160 строк дублирования |
| 5 | Namespace + enum + version | Средняя | Средний | Типобезопасность |
| 6 | cJSON вместо ostringstream | Средняя | Низкий | Надёжность JSON |
| 9 | TelemetrySnapshot | Средняя | Низкий | Декаплинг |
| 10 | Убрать VehicleControl обёртку | Средняя | Низкий | Простота |
| 1 | Разбить god class | Высокая | Средний | Тестируемость, расширяемость |
