# Распиновка STM32G431CBTxZ для RC Vehicle проекта

## Обзор

STM32G431CBTxZ (LQFP48, 7×7 мм) имеет 14 таймеров с поддержкой PWM генерации и Input Capture для чтения PWM сигналов.

## Рекомендуемая распиновка для RC Vehicle

### PWM выходы (генерация для ESC и серво)

Для генерации PWM сигналов 50 Hz (1-2 мс) для ESC и сервопривода рекомендуется использовать следующие пины:

**⚠️ ВАЖНО: Уровни сигналов для ESC и серво**

- **STM32G431** работает на **3.3V логике**
- **Большинство ESC и серво** ожидают **5V логику** (стандарт RC индустрии)
- **Рекомендация для разводки платы:** Использовать **TXS0104E** для надежного преобразования уровней

**TXS0104E (level-shifter для разводки платы):**

- **Характеристики:**
  - Автоматический двунаправленный level-shifter
  - Поддерживает 3.3V ↔ 5V (A port: 1.65-3.6V, B port: 2.3-5.5V)
  - Максимальная скорость: 24 Mbps (push-pull) — более чем достаточно для PWM 50 Hz
  - 4 канала (можно использовать для ESC + Servo + запас)
  - Не требует direction control — автоматически определяет направление
- **Как работает:**
  - Каждый канал работает в **одном направлении в каждый момент времени**
  - Направление автоматически определяется по уровню сигнала на каждой стороне
  - Для PWM выходов (STM32 → ESC/Servo) это идеально — нужно только одно направление
  - Разные каналы могут работать в разных направлениях одновременно (например, канал 1: 3.3V→5V, канал 2: 5V→3.3V)
- **Подключение для PWM выходов и входов одновременно:**

     ```
     VCCA → 3.3V (сторона STM32)
     VCCB → 5V (сторона ESC/Servo и RC приёмник)
     GND → общая земля

     PWM выходы (STM32 → ESC/Servo):
     - A1 → PA8 (STM32 PWM выход, TIM1_CH1)
     - B1 → ESC Signal (5V)
     - A2 → PA9 (STM32 PWM выход, TIM1_CH2)
     - B2 → Servo Signal (5V)

     PWM входы (RC приёмник → STM32):
     - B3 → RC Receiver CH1 (5V, от приёмника)
     - A3 → PA0 (STM32 PWM вход, TIM2_CH1, Input Capture)
     - B4 → RC Receiver CH2 (5V, от приёмника)
     - A4 → PA1 (STM32 PWM вход, TIM2_CH2, Input Capture)
     ```

- **Преимущества использования для входов и выходов:**
  - ✅ Одна микросхема для всех PWM сигналов (4 канала)
  - ✅ Автоматическое преобразование уровней 3.3V ↔ 5V
  - ✅ Не нужны делители напряжения для RC приёмника (если выдаёт 5V)
  - ✅ Защита STM32 от 5V сигналов с RC приёмника
  - ✅ Универсальное решение для всех PWM сигналов
- **Доступность:** Texas Instruments, доступен у дистрибьюторов (проверить наличие в России)
- **KiCad библиотека:**
  - **Символ (Symbol):** Ищите в библиотеке `74xx` или `Texas_Instruments` → `TXS0104E`
  - **Footprint:** Зависит от корпуса:
    - **TSSOP-14:** `Package_SO:TSSOP-14_4.4x5mm_P0.65mm`
    - **VQFN-14:** `Package_DFN_QFN:Texas_VQFN-14-EP_3.5x3.5mm_P0.5mm`
    - **SOIC-14:** `Package_SO:SOIC-14_3.9x8.7mm_P1.27mm`
  - **Как найти в KiCad:**
       1. В редакторе схемы: `Place → Symbol` или нажмите `A`
       2. В поиске введите: `TXS0104E` или `TXS`
       3. Если не найдено в стандартных библиотеках:
          - Скачайте с **SnapEDA**: <https://www.snapeda.com/parts/TXS0104EYZTR/Texas%20Instruments/view-part/>
          - Или с **Ultra Librarian**: <https://app.ultralibrarian.com/details/16c1134f-103f-11e9-ab3a-0a3560a4cccc/Texas-Instruments/TXS0104ERGYR>
          - Импортируйте символ и footprint в свои библиотеки
  - **Рекомендуемый корпус:** TSSOP-14 (TXS0104EPWR) — удобен для пайки, доступен в России
- **Преимущества использования для входов и выходов одновременно:**
  - ✅ Одна микросхема для всех PWM сигналов (4 канала)
  - ✅ Автоматическое преобразование уровней 3.3V ↔ 5V
  - ✅ Не нужны делители напряжения для RC приёмника (если выдаёт 5V)
  - ✅ Защита STM32 от 5V сигналов с RC приёмника
  - ✅ Универсальное решение для всех PWM сигналов
  - ✅ Надежное решение для разводки платы — не нужно переделывать потом

#### Вариант 1 (рекомендуемый): TIM1

- **ESC (Throttle)**: `PA8` → `TIM1_CH1` (AF2)
- **Servo (Steering)**: `PA9` → `TIM1_CH2` (AF2)

**Преимущества:**

- TIM1 — advanced timer с высокой точностью
- Поддержка dead-time generation (полезно для мостов)
- Хорошая производительность
- Пины PA8/PA9 часто доступны на большинстве плат

#### Вариант 2: TIM2

- **ESC (Throttle)**: `PA0` → `TIM2_CH1` (AF1)
- **Servo (Steering)**: `PA1` → `TIM2_CH2` (AF1)

**Преимущества:**

- TIM2 — 32-bit timer (высокая точность)
- Хорошая альтернатива, если PA8/PA9 заняты

#### Вариант 3: TIM3

- **ESC (Throttle)**: `PA6` → `TIM3_CH1` (AF1)
- **Servo (Steering)**: `PA7` → `TIM3_CH2` (AF1)

#### Вариант 4: TIM4

- **ESC (Throttle)**: `PB6` → `TIM4_CH1` (AF2)
- **Servo (Steering)**: `PB7` → `TIM4_CH2` (AF2)

### PWM входы (чтение сигналов с RC приёмника)

Для чтения PWM сигналов с RC приёмника рекомендуется использовать **PWM Input Mode** таймеров, который позволяет аппаратно измерять период и ширину импульса.

**⚠️ ВАЖНО: Уровни сигналов для RC приёмника**

- **STM32G431** работает на **3.3V логике**
- **Многие RC приёмники** выдают **5V логику** (стандарт RC индустрии)
- **Рекомендация:** Использовать **TXS0104E** (те же 4 канала) для преобразования уровней:
  - Каналы 1-2: PWM выходы (STM32 → ESC/Servo)
  - Каналы 3-4: PWM входы (RC приёмник → STM32)
- **Примечание:** Если RC приёмник выдаёт 5V, рекомендуется использовать TXS0104E (те же 4 канала). Делители напряжения (10kΩ + 20kΩ) можно использовать только если TXS0104E недоступен, но для разводки платы лучше сразу использовать TXS0104E

#### Вариант 1 (рекомендуемый): TIM2 в режиме PWM Input

- **RC Throttle (CH1)**: `PA0` (Input) → `TIM2_CH1` (AF1) — Input Capture для периода
- **RC Steering (CH2)**: `PA1` (Input) → `TIM2_CH2` (AF1) — Input Capture для ширины импульса

**Подключение через TXS0104E (если RC приёмник выдаёт 5V):**

- RC Receiver CH1 (5V) → TXS0104E B3 (5V сторона)
- TXS0104E A3 (3.3V сторона) → PA0 (STM32, Input)
- RC Receiver CH2 (5V) → TXS0104E B4 (5V сторона)
- TXS0104E A4 (3.3V сторона) → PA1 (STM32, Input)

**Примечание:** В PWM Input Mode один канал измеряет период, другой — ширину импульса. Для двух независимых каналов можно использовать два разных таймера.

#### Вариант 2: TIM3 в режиме PWM Input

- **RC Throttle (CH1)**: `PA6` → `TIM3_CH1` (AF1)
- **RC Steering (CH2)**: `PA7` → `TIM3_CH2` (AF1)

#### Вариант 3: TIM4 в режиме PWM Input

- **RC Throttle (CH1)**: `PB6` → `TIM4_CH1` (AF2)
- **RC Steering (CH2)**: `PB7` → `TIM4_CH2` (AF2)

#### Вариант 4: Использование Input Capture на разных таймерах

Если нужны два независимых канала:

- **RC Throttle (CH1)**: `PA0` → `TIM2_CH1` (AF1) — Input Capture
- **RC Steering (CH2)**: `PA6` → `TIM3_CH1` (AF1) — Input Capture

### Дополнительные интерфейсы

#### UART (связь с ESP32-C3)

**Вариант 1 (рекомендуемый): USART2 ↔ UART1 ESP32-C3 (GPIO4/5)**

- **STM32 TX**: `PA2` (Output) → `USART2_TX` (AF7) → **ESP32-C3 GPIO4** (Input, UART1 RX) — переназначенный
- **STM32 RX**: `PA3` (Input) → `USART2_RX` (AF7) ← **ESP32-C3 GPIO5** (Output, UART1 TX) — переназначенный

**Вариант 2: USART2 ↔ UART1 ESP32-C3 (GPIO0/1)**

- **STM32 TX**: `PA2` → `USART2_TX` (AF7) → **ESP32-C3 GPIO0** (UART1 RX) — переназначенный
- **STM32 RX**: `PA3` → `USART2_RX` (AF7) ← **ESP32-C3 GPIO1** (UART1 TX) — переназначенный

**Вариант 3: USART2 ↔ UART1 ESP32-C3 (GPIO6/7)**

- **STM32 TX**: `PA2` → `USART2_TX` (AF7) → **ESP32-C3 GPIO6** (UART1 RX) — переназначенный
- **STM32 RX**: `PA3` → `USART2_RX` (AF7) ← **ESP32-C3 GPIO7** (UART1 TX) — переназначенный

**Вариант 4: USART1 ↔ UART1 ESP32-C3**

- **STM32 TX**: `PA9` → `USART1_TX` (AF7) → **ESP32-C3 GPIO4** (UART1 RX) — переназначенный
- **STM32 RX**: `PA10` → `USART1_RX` (AF7) ← **ESP32-C3 GPIO5** (UART1 TX) — переназначенный

**Вариант 5: USART3 ↔ UART1 ESP32-C3**

- **STM32 TX**: `PB10` → `USART3_TX` (AF7) → **ESP32-C3 GPIO4** (UART1 RX) — переназначенный
- **STM32 RX**: `PB11` → `USART3_RX` (AF7) ← **ESP32-C3 GPIO5** (UART1 TX) — переназначенный

**Рекомендации:**

- Использовать USART2 (PA2/PA3), если PA9/PA10 заняты под PWM
- ⚠️ **НЕ использовать** GPIO18/19 на ESP32-C3 — они для USB
- ⚠️ **НЕ использовать** GPIO2, GPIO8, GPIO9 — strapping пины (контроль загрузки)
- **Рекомендуемые GPIO для UART1:** GPIO0, GPIO1, GPIO3, GPIO4, GPIO5, GPIO6, GPIO7, GPIO10
- Уровни сигналов совместимы (оба 3.3V), делители не нужны
- Скорость: 115200 baud (рекомендуется для MVP)
- **Переназначение UART1:** в ESP-IDF используйте `uart_set_pin(UART_NUM_1, GPIO_NUM_4, GPIO_NUM_5, ...)`

#### SPI (подключение IMU, например LSM6DSM)

**Вариант 1 (рекомендуемый): SPI2**

- **SPI SCK**: `PB13` (Output) → `SPI2_SCK` (AF5) → **LSM6DSM SCL/SPC** (Pin 13)
- **SPI MOSI**: `PB15` (Output) → `SPI2_MOSI` (AF5) → **LSM6DSM SDA/SDI** (Pin 14)
- **SPI MISO**: `PB14` (Input) → `SPI2_MISO` (AF5) → **LSM6DSM SDO/SA0** (Pin 1)
- **CS**: `PB12` (Output) → GPIO → **LSM6DSM CS** (Pin 12, активный LOW)

**Вариант 2: SPI1**

- **SPI SCK**: `PA5` (Output) → `SPI1_SCK` (AF5) → **LSM6DSM SCL/SPC** (Pin 13)
- **SPI MOSI**: `PA7` (Output) → `SPI1_MOSI` (AF5) → **LSM6DSM SDA/SDI** (Pin 14)
- **SPI MISO**: `PA6` (Input) → `SPI1_MISO` (AF5) → **LSM6DSM SDO/SA0** (Pin 1)
- **CS**: любой свободный GPIO (Output) → **LSM6DSM CS** (Pin 12)

**Вариант 3: SPI3** (если доступен в LQFP48)

- **SPI SCK**: `PC10` (Output) → `SPI3_SCK` (AF6) → **LSM6DSM SCx/SCK** (Pin 3)
- **SPI MOSI**: `PC12` (Output) → `SPI3_MOSI` (AF6) → **LSM6DSM SDx/SDI** (Pin 2)
- **SPI MISO**: `PC11` (Input) → `SPI3_MISO` (AF6) → **LSM6DSM SDO** (Pin 1)
- **CS**: любой свободный GPIO (Output) → **LSM6DSM CS**

**Примечание:** LSM6DSM поддерживает как I2C, так и SPI. SPI обеспечивает более высокую скорость передачи данных.

#### I2C (альтернативный вариант для IMU)

- **I2C SDA**: `PB7` → `I2C1_SDA` (AF4) или `PB11` → `I2C2_SDA` (AF4)
- **I2C SCL**: `PB6` → `I2C1_SCL` (AF4) или `PB10` → `I2C2_SCL` (AF4)

**Примечание:** Если PB6/PB7 используются для PWM, используйте I2C2 (PB10/PB11).

## Рекомендуемая конфигурация (без конфликтов)

**Рекомендация: Использовать TXS0104E для всех PWM сигналов (входы и выходы)**

TXS0104E имеет 4 канала, которые могут работать в разных направлениях одновременно:

- **Каналы 1-2:** PWM выходы (STM32 3.3V → ESC/Servo 5V)
- **Каналы 3-4:** PWM входы (RC приёмник 5V → STM32 3.3V)

**Преимущества:**

- ✅ Одна микросхема для всех PWM сигналов
- ✅ Автоматическое преобразование уровней 3.3V ↔ 5V
- ✅ Не нужны делители напряжения для RC приёмника
- ✅ Защита STM32 от 5V сигналов
- ✅ Универсальное решение

### Вариант A: TIM1 для PWM выхода, TIM2 для PWM входа (с TXS0104E)

```
PWM выходы (через TXS0104E, каналы 1-2):
- PA8 (Output, TIM1_CH1) → TXS0104E A1 → TXS0104E B1 → ESC Signal (5V)
- PA9 (Output, TIM1_CH2) → TXS0104E A2 → TXS0104E B2 → Servo Signal (5V)

PWM входы (через TXS0104E, каналы 3-4):
- RC Receiver CH1 (5V) → TXS0104E B3 → TXS0104E A3 → PA0 (Input, TIM2_CH1) - Input Capture
- RC Receiver CH2 (5V) → TXS0104E B4 → TXS0104E A4 → PA1 (Input, TIM2_CH2) - Input Capture

TXS0104E подключение:
- VCCA → 3.3V (сторона STM32)
- VCCB → 5V (сторона ESC/Servo/RC приёмник)
- GND → общая земля

UART:
- PA2 (Output, USART2_TX) → ESP32-C3 GPIO4 (Input, UART1 RX) — переназначенный
- PA3 (Input, USART2_RX) ← ESP32-C3 GPIO5 (Output, UART1 TX) — переназначенный

SPI (LSM6DSM):
- PB13 (Output, SPI2_SCK) → LSM6DSM SCL/SPC (Pin 13)
- PB15 (Output, SPI2_MOSI) → LSM6DSM SDA/SDI (Pin 14)
- PB14 (Input, SPI2_MISO) → LSM6DSM SDO/SA0 (Pin 1)
- PB12 (Output, GPIO) → LSM6DSM CS (Pin 12, активный LOW)
```

**Преимущества использования TXS0104E:**

- ✅ Одна микросхема для всех PWM сигналов (4 канала)
- ✅ Автоматическое преобразование 3.3V ↔ 5V
- ✅ Не нужны делители напряжения для RC приёмника
- ✅ Защита STM32 от 5V сигналов

### Вариант B: TIM2 для PWM выхода, TIM3 для PWM входа (с TXS0104E)

```
PWM выходы (через TXS0104E, каналы 1-2):
- PA0 (Output, TIM2_CH1) → TXS0104E A1 → TXS0104E B1 → ESC Signal (5V)
- PA1 (Output, TIM2_CH2) → TXS0104E A2 → TXS0104E B2 → Servo Signal (5V)

PWM входы (через TXS0104E, каналы 3-4):
- RC Receiver CH1 (5V) → TXS0104E B3 → TXS0104E A3 → PA6 (Input, TIM3_CH1) - Input Capture
- RC Receiver CH2 (5V) → TXS0104E B4 → TXS0104E A4 → PA7 (Input, TIM3_CH2) - Input Capture

TXS0104E подключение:
- VCCA → 3.3V (сторона STM32)
- VCCB → 5V (сторона ESC/Servo/RC приёмник)
- GND → общая земля

UART:
- PA2 (Output, USART2_TX) → ESP32-C3 GPIO4 (Input, UART1 RX) — переназначенный
- PA3 (Input, USART2_RX) ← ESP32-C3 GPIO5 (Output, UART1 TX) — переназначенный

SPI (LSM6DSM):
- PB13 (Output, SPI2_SCK) → LSM6DSM SCL/SPC (Pin 13)
- PB15 (Output, SPI2_MOSI) → LSM6DSM SDA/SDI (Pin 14)
- PB14 (Input, SPI2_MISO) → LSM6DSM SDO/SA0 (Pin 1)
- PB12 (Output, GPIO) → LSM6DSM CS (Pin 12, активный LOW)
```

### Вариант C: TIM3 для PWM выхода, TIM4 для PWM входа (с TXS0104E)

```
PWM выходы (через TXS0104E, каналы 1-2):
- PA6 (Output, TIM3_CH1) → TXS0104E A1 → TXS0104E B1 → ESC Signal (5V)
- PA7 (Output, TIM3_CH2) → TXS0104E A2 → TXS0104E B2 → Servo Signal (5V)

PWM входы (через TXS0104E, каналы 3-4):
- RC Receiver CH1 (5V) → TXS0104E B3 → TXS0104E A3 → PB6 (Input, TIM4_CH1) - Input Capture
- RC Receiver CH2 (5V) → TXS0104E B4 → TXS0104E A4 → PB7 (Input, TIM4_CH2) - Input Capture

TXS0104E подключение:
- VCCA → 3.3V (сторона STM32)
- VCCB → 5V (сторона ESC/Servo/RC приёмник)
- GND → общая земля

UART:
- PA2 (Output, USART2_TX) → ESP32-C3 GPIO4 (Input, UART1 RX) — переназначенный
- PA3 (Input, USART2_RX) ← ESP32-C3 GPIO5 (Output, UART1 TX) — переназначенный

SPI (LSM6DSM):
- PB13 (Output, SPI2_SCK) → LSM6DSM SCL/SPC (Pin 13)
- PB15 (Output, SPI2_MOSI) → LSM6DSM SDA/SDI (Pin 14)
- PB14 (Input, SPI2_MISO) → LSM6DSM SDO/SA0 (Pin 1)
- PB12 (Output, GPIO) → LSM6DSM CS (Pin 12, активный LOW)
```

## Таблица альтернативных функций (краткая)

| Пин | Основная функция | Альтернативные функции (AF) |
|-----|------------------|------------------------------|
| PA0 | GPIO | TIM2_CH1, USART2_CTS |
| PA1 | GPIO | TIM2_CH2, USART2_RTS |
| PA2 | GPIO | USART2_TX, TIM15_CH1 |
| PA3 | GPIO | USART2_RX, TIM15_CH2 |
| PA6 | GPIO | TIM3_CH1, TIM16_BKIN |
| PA7 | GPIO | TIM3_CH2, TIM17_BKIN |
| PA8 | GPIO | TIM1_CH1, MCO |
| PA9 | GPIO | TIM1_CH2, USART1_TX |
| PA10 | GPIO | TIM1_CH3, USART1_RX |
| PB6 | GPIO | TIM4_CH1, I2C1_SCL |
| PB7 | GPIO | TIM4_CH2, I2C1_SDA |
| PB10 | GPIO | TIM2_CH3, I2C2_SCL |
| PB11 | GPIO | TIM2_CH4, I2C2_SDA |
| PB12 | GPIO | SPI2_NSS (опционально) |
| PB13 | GPIO | SPI2_SCK (AF5) |
| PB14 | GPIO | SPI2_MISO (AF5) |
| PB15 | GPIO | SPI2_MOSI (AF5) |
| PA5 | GPIO | SPI1_SCK (AF5) |
| PA6 | GPIO | TIM3_CH1, SPI1_MISO (AF5) |
| PA7 | GPIO | TIM3_CH2, SPI1_MOSI (AF5) |

## Настройка таймеров

### PWM выход (50 Hz, 1-2 мс)

Для генерации PWM 50 Hz с шириной импульса 1-2 мс:

```c
// Пример для TIM1_CH1 (PA8)
// Частота таймера: 170 MHz (типично для STM32G4)
// Период: 20 мс (50 Hz)
// Prescaler: 170 (для получения 1 MHz)
// Auto-reload: 20000 (20 мс при 1 MHz)
// CCR для 1 мс: 1000
// CCR для 2 мс: 2000
// CCR для 1.5 мс (нейтраль): 1500
```

### PWM Input Mode (чтение RC сигналов)

В PWM Input Mode таймер автоматически измеряет:

- **Период** (CCR1) — время между двумя фронтами
- **Ширину импульса** (CCR2) — время высокого уровня

Настройка:

- Канал 1: Input Capture на оба фронта (rising/falling)
- Канал 2: Input Capture на rising edge
- Канал 2 подключен к каналу 1 через внутреннюю связь

### SPI (подключение LSM6DSM)

Для работы с LSM6DSM через SPI:

```c
// Пример настройки SPI2 для LSM6DSM
// Частота SPI: до 10 MHz (проверьте datasheet LSM6DSM)
// Режим: SPI_MODE_3 (CPOL=1, CPHA=1) или SPI_MODE_0 (CPOL=0, CPHA=0)
// Размер данных: 8 бит
// Порядок битов: MSB first
// CS управляется программно через GPIO (PB12)
```

**Важные параметры:**

- **Частота SPI**: до 10 MHz (типично для LSM6DSM)
- **Режим SPI**: Режим 3 (CPOL=1, CPHA=1) или Режим 0 (CPOL=0, CPHA=0) — проверьте datasheet LSM6DSM
- **CS (Chip Select)**: управляется программно через GPIO, активный уровень LOW
- **Размер данных**: 8 бит
- **Порядок битов**: MSB first

**Подключение LSM6DSM (основной SPI 4-wire интерфейс):**

- **SCL/SPC** (Pin 13) → PB13 (Output, SPI2_SCK)
- **SDA/SDI** (Pin 14) → PB15 (Output, SPI2_MOSI)
- **SDO/SA0** (Pin 1) → PB14 (Input, SPI2_MISO)
- **CS** (Pin 12) → PB12 (Output, GPIO, активный LOW)
- **VDD** (Pin 8) → 3.3V
- **VDDIO** (Pin 5) → 3.3V
- **GND** (Pin 6, 7) → GND

**Названия пинов на LSM6DSM для основного SPI 4-wire:**

- Pin 1: **SDO/SA0** — Serial Data Output (MISO)
- Pin 12: **CS** — Chip Select (активный LOW) / Mode selection (LOW = SPI, HIGH = I2C)
- Pin 13: **SCL** — SPI Serial Port Clock (SPC) / SCK
- Pin 14: **SDA** — SPI Serial Data Input (SDI) / MOSI

**Примечание:** Pin 2 (SDx) и Pin 3 (SCx) используются для вспомогательного SPI (Auxiliary SPI), не для основного интерфейса.

## Важные замечания

1. **Уровни сигналов:**
   - STM32G431 работает на **3.3V логике**
   - **PWM выходы (ESC/Servo):**
     - Большинство ESC и серво ожидают **5V логику** (стандарт RC)
     - **Рекомендация для разводки платы:** Использовать **TXS0104E** для преобразования уровней 3.3V → 5V
   - **PWM входы (RC приёмник):**
     - Если RC приёмник выдаёт 5V, использовать **TXS0104E** (те же 4 канала) для преобразования 5V → 3.3V
     - **Альтернатива (не рекомендуется для разводки платы):** делители напряжения (10kΩ + 20kΩ)
   - **PWM выходы STM32:** не являются 5V-tolerant (работают только на 3.3V)

2. **Питание:**
   - VDD: 3.3V (или 1.71V - 3.6V)
   - VDDA: 3.3V (для ADC)
   - VSS: GND

3. **Подтягивающие резисторы:**
   - Для Input Capture рекомендуется внутренняя подтяжка (pull-up или pull-down)
   - Для SPI: подтягивающие резисторы обычно не требуются (CS управляется программно)
   - Для I2C (если используется): внешние подтягивающие резисторы 4.7kΩ (если модуль IMU не имеет встроенных)

4. **Конфликты:**
   - Проверьте, что выбранные пины не используются для других функций (boot, reset, debug)
   - PA13/PA14/PA15 используются для SWD debug — избегайте их для пользовательских функций

## Проверка распиновки

Перед проектированием платы рекомендуется:

1. Использовать STM32CubeMX для визуализации распиновки
2. Проверить доступность выбранных пинов на конкретной плате/модуле
3. Убедиться, что нет конфликтов с другими периферийными устройствами

## Ссылки

- [STM32G431 Datasheet](https://www.st.com/resource/en/datasheet/stm32g431c6.pdf)
- [STM32G4 Reference Manual](https://www.st.com/resource/en/reference_manual/rm0440-stm32g4-series-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)
- STM32CubeMX для конфигурации пинов и генерации кода
