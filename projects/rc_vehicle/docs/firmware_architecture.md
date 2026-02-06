# Архитектура ПО прошивок RC Vehicle

Документ описывает текущую архитектуру прошивок (ESP32, RP2040, STM32) и предлагает шаги по **максимальной унификации** кода и API при сохранении платформенной гибкости.

---

## 1. Текущее состояние

### 1.1 Роли платформ

| Платформа | Роль | Ключевые компоненты |
|----------|------|---------------------|
| **ESP32** | Шлюз: Wi‑Fi AP, HTTP, WebSocket, UART ↔ MCU | UART bridge (приём телеметрии, отправка команд, PING/PONG) |
| **RP2040** / **STM32** | MCU на машине: PWM, RC-in, IMU, UART ↔ ESP32 | UART bridge, PWM, RC input, IMU (MPU-6050), Failsafe |

### 1.2 Общий код (`firmware/common/`)

- **Протокол** — `protocol.hpp/cpp`: кадры TELEM/COMMAND/PING/PONG, CRC16, парсинг.
- **UART-мост (база)** — `uart_bridge_base.hpp/cpp`: абстрактный класс с логикой протокола; наследники реализуют `Init()`, `Write()`, `ReadAvailable()`.
- **SPI (база)** — `spi_base.hpp`: абстрактный драйвер; наследники — под Pico/STM32.
- **MPU-6050** — `mpu6050_spi.hpp/cpp`: драйвер по SPI (через `SpiBase*`).
- **Failsafe** — `failsafe_core.hpp/cpp`: таймаут, rc_active/wifi_active.
- **Утилиты** — `slew_rate.hpp`, `rc_vehicle_common.hpp` (PWM/RC нормализация).
- **Контекст** — `context.hpp`: типобезопасная регистрация компонентов по типу (`Set<T>`, `Get<T>`). **Пока не используется в main.**
- **Базовый компонент** — `base_component.hpp`: виртуальные `Init()`, `Name()`.

### 1.3 Несогласованности (между RP2040 и STM32)

1. **UART bridge C-API**
   - RP2040: `std::optional<UartBridgeCommand> UartBridgeReceiveCommand()`, `UartBridgeSendTelem(const TelemetryData&)`.
   - STM32: `bool UartBridgeReceiveCommand(float *throttle, float *steering)`, `UartBridgeSendTelem(const void*)`.
2. **Конфиг** — интервалы и лимиты дублируются в `rp2040/main/config.hpp` и `stm32/main/config.hpp`; общая часть может жить в `common/`.
3. **Main loop** — логика RP2040 и STM32 почти одинаковая (порядок инициализации, таймеры, шаги цикла), но код разнесён по двум файлам без общего ядра.

---

## 2. Целевая архитектура (унификация)

### 2.1 Слои

```
┌─────────────────────────────────────────────────────────────────┐
│  App (main.cpp)                                                  │
│  — порядок инициализации, главный цикл / таски                   │
│  — по возможности общий «каркас» для MCU (RP2040/STM32)         │
└─────────────────────────────────────────────────────────────────┘
                                  │
┌─────────────────────────────────────────────────────────────────┐
│  Platform (платформа)                                            │
│  — C-обёртки или компоненты: UartBridge, PWM, RC, IMU, Failsafe  │
│  — реализация абстракций: UartBridgeBase, SpiBase, platform_*    │
└─────────────────────────────────────────────────────────────────┘
                                  │
┌─────────────────────────────────────────────────────────────────┐
│  Common (firmware/common/)                                       │
│  — протокол, uart_bridge_base, spi_base, mpu6050_spi, failsafe   │
│  — context, base_component, slew_rate, rc_vehicle_common          │
└─────────────────────────────────────────────────────────────────┘
```

### 2.2 Единый C/C++ API для MCU (RP2040 и STM32)

Цель: один и тот же контракт для main loop, чтобы можно было вынести общую логику или копипастить минимально.

| Функция/тип | Единый вид | Примечание |
|-------------|------------|------------|
| UART bridge init | `int UartBridgeInit(void)` | уже так |
| Отправка телеметрии | `int UartBridgeSendTelem(const TelemetryData *telem)` | STM32 перейти с `const void*` на `const TelemetryData*` |
| Приём команды | `std::optional<UartBridgeCommand> UartBridgeReceiveCommand(void)` или для C: обёртка `bool UartBridgeReceiveCommand(float *throttle, float *steering)` с внутренним optional | унифицировать с RP2040: один тип команды, один способ возврата |
| PING/PONG | `bool UartBridgeReceivePing(void)`, `int UartBridgeSendPong(void)` | уже так |
| Failsafe | `void FailsafeInit(void)`, `bool FailsafeUpdate(bool rc_active, bool wifi_active)`, `bool FailsafeIsActive(void)` | RP2040 уже так; STM32 — обёртка над core с получением `now_ms` внутри (как в RP2040) |
| Время | За платформой: `platform_get_time_ms()` (STM32) или `time_us_32()/1000` (RP2040) | общий цикл получает `now_ms` от платформы через один абстрактный вызов |

Рекомендация: в **STM32** привести `uart_bridge.hpp` к тому же API, что и RP2040 (`TelemetryData&`, `std::optional<UartBridgeCommand>`), а в main использовать один и тот же стиль (optional). Если позже понадобится чистый C — добавить тонкие C-обёртки в одном месте.

### 2.3 Общий конфиг (common)

Вынести в `common/config_common.hpp` (или `config_defaults.hpp`) всё, что не зависит от пинов и периферии:

- интервалы: `PWM_UPDATE_INTERVAL_MS`, `RC_IN_POLL_INTERVAL_MS`, `IMU_READ_INTERVAL_MS`, `TELEM_SEND_INTERVAL_MS`, `FAILSAFE_TIMEOUT_MS`;
- лимиты PWM/RC: `PWM_NEUTRAL_US`, `PWM_MIN_US`, `PWM_MAX_US`, `RC_IN_PULSE_*`, `RC_IN_TIMEOUT_MS`;
- slew rate: `SLEW_RATE_THROTTLE_MAX_PER_SEC`, `SLEW_RATE_STEERING_MAX_PER_SEC`;
- прочее: `UART_BAUD_RATE`, `UART_BUF_SIZE`, `PWM_FREQUENCY_HZ`.

В `rp2040/main/config.hpp` и `stm32/main/config.hpp` (и при необходимости `board_pins.hpp`) оставить только:

- идентификаторы периферии (UART_ID, SPI_ID и т.п.);
- пины (GPIO, USART, SPI);
- переопределения по платформе (если нужно).

Подключение: `#include "config_common.hpp"` в общем коде и в платформенном config.hpp после переопределений.

### 2.4 Общий главный цикл MCU (опционально, но желательно)

Логика цикла на RP2040 и STM32 совпадает:

1. Получить `now_ms`.
2. Обновить PWM (с slew rate) по таймеру.
3. Опрос RC; при активном RC — обновить throttle/steering, сбросить wifi_active.
4. Обработать PING → SendPong (и при желании LED).
5. Принять команду по UART; если нет RC — применить throttle/steering, выставить wifi_active.
6. Чтение IMU по таймеру.
7. Failsafe по таймеру; при активном failsafe — нейтраль.
8. Сборка и отправка телеметрии по таймеру.
9. Короткая задержка (platform_delay_ms(1) или аналог).

Варианты унификации:

- **Вариант A (минимальный):** оставить два main.cpp, но описать этот порядок и интервалы в общем документе (например, в `common/README.md` или в этом файле) и строго ему следовать — уже даёт единообразие.
- **Вариант B (общий цикл в common):** ввести в common заголовок/модуль `mcu_app_loop.hpp` (или `app_loop.hpp`), в котором одна функция вида:
  - `void McuAppLoopOnce(McuAppLoopState *state, uint32_t now_ms, const McuAppLoopCallbacks *cb);`
  - или класс `McuAppLoop` с внедрёнными зависимостями (указатели на функции или интерфейсы: get_time_ms, set_pwm, read_rc, uart_send_telem, …).

В обоих случаях платформа предоставляет:

- `uint32_t get_time_ms(void);`
- `void set_pwm(float throttle, float steering);` / `void set_neutral(void);`
- `bool read_rc(float *throttle, float *steering);`
- доступ к UART bridge (SendTelem, ReceiveCommand, PING/PONG), IMU, Failsafe.

Тогда main на RP2040 и STM32 сводится к инициализации платформы и вызову общего цикла (или к одному и тому же списку шагов в одном порядке).

### 2.5 Использование Context и BaseComponent

Сейчас в main компоненты создаются статически (синглтоны типа `static Stm32UartBridge s_bridge`) и вызываются через C-обёртки. Для дальнейшей унификации можно:

1. Объявить тип контекста для MCU, например:
   - `using McuContext = Context<UartBridgeBase, SpiBase, Mpu6050Spi>;`
   (или без Mpu6050Spi, если IMU остаётся за обёрткой Imu*).
2. В main (или в одном месте инициализации) создавать экземпляры платформенных классов, регистрировать их в контексте: `ctx.Set<UartBridgeBase>(&uart_impl); ctx.Set<SpiBase>(&spi_impl);` и т.д.
3. Общую логику (например, будущий McuAppLoop) получать зависимости через `ctx.Get<UartBridgeBase>()` и т.п., не зная конкретной платформы.

Компоненты (PWM, RC, IMU, UART bridge) могут наследовать `BaseComponent` и при инициализации принимать `Context&`, если им нужен доступ к другим сервисам. Это не обязательно делать сразу: достаточно зафиксировать в архитектуре, что контекст — единая точка регистрации зависимостей для MCU.

### 2.6 ESP32

ESP32 играет другую роль (шлюз, задачи FreeRTOS), поэтому полная унификация main с MCU нецелесообразна. Унифицировать стоит:

- **Протокол и UART bridge (база)** — уже общие.
- **Формат телеметрии и команд** — общий (protocol.hpp).
- При необходимости — общий конфиг констант протокола и таймингов (PING интервал и т.д.) из `config_common.hpp` или аналога.

---

## 3. План внедрения (по шагам)

1. **Унифицировать C-API UART bridge на STM32**
   Привести к тому же виду, что на RP2040: `UartBridgeSendTelem(const TelemetryData *)`, `std::optional<UartBridgeCommand> UartBridgeReceiveCommand()`. Обновить STM32 main под новый API.

2. **Вынести общий конфиг**
   Создать `common/config_common.hpp` с интервалами и лимитами; подключить в RP2040 и STM32, убрать дублирование.

3. **Зафиксировать контракт главного цикла MCU**
   Описать в `common/README.md` или в этом документе единый порядок шагов и интервалы; при необходимости вынести общий цикл в `mcu_app_loop.hpp` и использовать его в обоих main.

4. **Опционально: внедрить Context в MCU**
   В main создавать контекст, регистрировать UartBridgeBase, SpiBase и при необходимости другие компоненты, передавать контекст в общий цикл или в компоненты. Это упростит добавление новых платформ и тестов.

5. **Документация**
   Обновить `firmware/README.md` и `common/README.md`: ссылка на этот документ, описание слоёв и единого API.

---

## 4. Итог

- **Общий код** уже хорошо выделен в `common/` (протокол, UART base, SPI base, MPU-6050, failsafe, context, base_component).
- **Унификация по максимуму** достигается за счёт: единого C/C++ API для MCU (UART bridge, при необходимости Failsafe), общего конфига интервалов/лимитов, общего контракта или реализации главного цикла MCU и при желании — использования Context как единой точки зависимостей.
- ESP32 остаётся отдельным по структуре main (задачи, Wi‑Fi, WebSocket), но использует тот же протокол и ту же базу UART bridge, что и MCU.

После выполнения плана добавление новой MCU-платформы сведётся к реализации UartBridgeBase, SpiBase, platform_* (время, задержка), PWM и RC под новую плату и к подключению общего цикла и общего конфига.
