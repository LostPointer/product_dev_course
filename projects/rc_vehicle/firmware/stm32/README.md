# STM32 прошивка для RC Vehicle

Прошивка для STM32 (F1/F4/G4), по функционалу аналогичная RP2040:
- PWM управление ESC и серво (50 Hz)
- Чтение RC-in сигналов (50 Hz)
- IMU MPU-6050 по I2C (50 Hz); датчик поддерживает и SPI — при необходимости можно реализовать в `imu.cpp`
- Failsafe (таймаут 250 мс)
- UART мост к ESP32 (команды и телеметрия)

## Выбор микроконтроллера

Сборка под конкретный MCU задаётся переменной `MCU`:

| MCU            | Плата / чип        | Семейство libopencm3 |
|----------------|--------------------|------------------------|
| `STM32F103C8`  | Blue Pill          | stm32f1                |
| `STM32F401CE`  | Nucleo-F401RE, F401 (512K Flash) | stm32f4       |
| `STM32F411CE`  | Black Pill         | stm32f4                |
| `STM32G431CB`  | STM32G431CBTx (схема rc_vehicle) | stm32g4       |

Добавить новый MCU: создать `cmake/boards/ИМЯ.cmake` и описать пины в `main/board_pins.hpp` для соответствующего `MCU_DEFINE`.

## Технологии

- **Язык**: C++17
- **HAL**: libopencm3
- **Сборка**: CMake + Make, тулчейн arm-none-eabi-gcc

## Структура

```
firmware/
├── common/                 # Общий код (RP2040 + STM32)
│   ├── protocol.hpp/cpp    # Протокол UART (AA 55, CRC16)
│   └── README.md
└── stm32/
    ├── main/
    │   ├── main.cpp           # Точка входа, цикл управления
    │   ├── config.hpp         # Общая конфигурация
    │   ├── board_pins.hpp     # Пины по семейству (F1/F4/G4)
    │   ├── platform.cpp/hpp   # Время (SysTick), задержки
    │   ├── pwm_control.*      # PWM ESC/серво (заглушка → TODO)
    │   ├── rc_input.*         # RC-in (заглушка → TODO)
    │   ├── imu.*              # IMU MPU-6050 (заглушка → TODO)
    │   ├── failsafe.*         # Failsafe
    │   └── uart_bridge.*      # UART ↔ ESP32 (использует common/protocol)
    ├── cmake/boards/       # Конфиги плат (MCU → linker script, флаги)
    │   ├── STM32F103C8.cmake
    │   ├── STM32F401CE.cmake
    │   ├── STM32F411CE.cmake
    │   └── STM32G431CB.cmake
    ├── CMakeLists.txt
    ├── Makefile            # make [MCU=...]
    └── README.md
```

## Сборка

### Требования

1. **libopencm3** (клон и сборка под нужные семейства):

   ```bash
   git clone https://github.com/libopencm3/libopencm3.git
   cd libopencm3
   make TARGETS="stm32/f1 stm32/f4 stm32/g4"
   ```

2. **Тулчейн** (тот же, что для RP2040):

   **Linux (apt):**
   ```bash
   sudo apt-get install cmake gcc-arm-none-eabi libnewlib-arm-none-eabi build-essential
   ```

   **macOS (Homebrew):** предпочтительно `gcc-arm-embedded` (cask), не `arm-none-eabi-gcc` (нет `nosys.specs`):
   ```bash
   brew install cmake
   brew install --cask gcc-arm-embedded
   ```
   Если `arm-none-eabi-gcc` не в PATH: `export PATH="/Applications/ArmGNUToolchain/15.2.Rel1/arm-none-eabi/bin:$PATH"` (версия может отличаться).

3. **Переменные окружения**

   ```bash
   export LIBOPENCM3_PATH=/path/to/libopencm3
   ```

### Команды make

Из каталога прошивки:

```bash
cd firmware/stm32
export LIBOPENCM3_PATH=/path/to/libopencm3

# Сборка для MCU по умолчанию (STM32F103C8)
make

# Сборка для выбранного MCU
make MCU=STM32F401CE
make MCU=STM32F411CE
make MCU=STM32G431CB

# Очистка (при смене MCU — обязательно make clean, затем make MCU=...)
make clean
```

Артефакты: `build/rc_vehicle_stm32.elf`, `build/rc_vehicle_stm32.bin`, `build/rc_vehicle_stm32.hex`.

### Параллельная сборка

- Linux: `make -j$(nproc)`
- macOS: `make -j$(sysctl -n hw.ncpu)`

## Прошивка

- **ST-Link:** `st-flash write build/rc_vehicle_stm32.bin 0x08000000`
- **OpenOCD:** через скрипт или IDE (STM32CubeIDE, VS Code + Cortex-Debug).
- **DFU (если включён):** `dfu-util -a 0 -D build/rc_vehicle_stm32.bin -s 0x08000000`

## Конфигурация

- **Пины и периферия:** `main/board_pins.hpp` (по семейству `STM32F1` / `STM32F4` / `STM32G4`). Подстройте под вашу схему.
- **Тайминги, UART, протокол:** `main/config.hpp` (как в RP2040).

## Протокол UART

Тот же, что у RP2040: префикс `AA 55`, версия, тип сообщения, длина, payload, CRC16. Типы: COMMAND (ESP32 → STM32), TELEM (STM32 → ESP32).

## Статус

- [x] Структура проекта, выбор MCU (Makefile + CMake)
- [x] Платформа (время, SysTick), failsafe, protocol
- [x] Заглушки PWM, RC-in, IMU, UART (логика в main готова)
- [ ] Реализация PWM/RC-in/IMU/UART на libopencm3 под выбранную плату
- [ ] Тактирование G4 в `main.cpp` (сейчас TODO)
- [ ] Проверка на железе

## Примечания

- При смене MCU выполните `make clean`, затем `make MCU=...`.
- Имена скриптов линкера и пути к ним зависят от версии libopencm3; при ошибках линковки проверьте `lib/stm32/<family>/` в вашей копии libopencm3.
