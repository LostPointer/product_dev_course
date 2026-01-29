# ESP32-C3 прошивка для RC Vehicle

Прошивка для ESP32-C3, обеспечивающая:
- Wi-Fi Access Point
- HTTP сервер для веб-интерфейса
- WebSocket сервер для команд управления и телеметрии
- UART мост к RP2040

## Технологии

- **Язык**: C++23
- **Фреймворк**: ESP-IDF v5.5 (требуется)
- **Целевая плата**: ESP32-C3 (DevKitM-1, WROOM-02 и совместимые)

## Структура проекта

```
firmware/esp32/
├── main/
│   ├── main.cpp               # Точка входа
│   ├── wifi_ap.cpp/hpp        # Wi-Fi Access Point
│   ├── http_server.cpp/hpp    # HTTP сервер (раздача веб-интерфейса)
│   ├── websocket_server.cpp/hpp # WebSocket сервер
│   ├── uart_bridge.cpp/hpp    # UART мост к RP2040
│   ├── protocol.cpp/hpp       # Парсинг/формирование UART кадров
│   └── config.hpp             # Конфигурация (SSID, порты, частоты)
├── web/
│   ├── index.html             # Веб-интерфейс управления
│   ├── style.css              # Стили
│   └── app.js                 # JavaScript логика
├── CMakeLists.txt             # Для ESP-IDF
├── sdkconfig.defaults          # Настройки по умолчанию
└── README.md
```

## Сборка

### Предварительные требования

1. **Установка ESP-IDF v5.5**
   ```bash
   # Клонирование ESP-IDF
   git clone --recursive https://github.com/espressif/esp-idf.git
   cd esp-idf
   git checkout v5.5
   ./install.sh esp32c3
   ```

2. **Настройка окружения**
   ```bash
   # Активация окружения ESP-IDF (Linux/macOS)
   . ./export.sh

   # Или добавьте в ~/.bashrc / ~/.zshrc:
   alias get_idf='. $HOME/esp/esp-idf/export.sh'
   ```

3. **Проверка установки**
   ```bash
   idf.py --version
   # Должно показать версию 5.5.x
   ```

   **Важно**: Проект требует ESP-IDF версии 5.5. Другие версии могут не работать корректно.

### Сборка проекта

1. **Переход в директорию проекта**
   ```bash
   cd projects/rc_vehicle/firmware/esp32
   ```

2. **Настройка целевой платы (обязательно!)**
   ```bash
   idf.py set-target esp32c3
   ```

   **Важно**: Без установки целевой платы проект будет собираться для esp32 по умолчанию, что приведёт к ошибкам.

3. **Конфигурация (опционально)**
   ```bash
   idf.py menuconfig
   ```
   Основные настройки уже заданы в `main/config.hpp`, но можно изменить:
   - Частоту CPU
   - Размеры буферов
   - Настройки Wi-Fi

4. **Сборка прошивки**
   ```bash
   idf.py build
   ```

   При успешной сборке будет создан файл `build/rc_vehicle_esp32.bin`

### Прошивка на устройство

1. **Подключение устройства**
   - Подключите ESP32-C3 к компьютеру через USB
   - Определите порт (Linux: `/dev/ttyUSB0` или `/dev/ttyACM0`, macOS: `/dev/cu.usbserial-*`, Windows: `COM*`)

2. **Прошивка**
   ```bash
   # Автоматическое определение порта
   idf.py flash

   # Или с указанием порта
   idf.py -p /dev/ttyUSB0 flash
   ```

3. **Мониторинг (просмотр логов)**
   ```bash
   # Запуск мониторинга после прошивки
   idf.py monitor

   # Или одной командой: прошивка + мониторинг
   idf.py flash monitor

   # Выход из мониторинга: Ctrl+]
   ```

### Подключение к RC Vehicle (веб-интерфейс)

После прошивки ESP32-C3 поднимает **Wi‑Fi точку доступа**. Подключиться к машине можно так:

1. **Включите питание** ESP32-C3 (и RP2040, если уже собрана схема).

2. **На телефоне/ноутбуке** в списке Wi‑Fi найдите сеть:
   - Имя (SSID): **`RC-Vehicle-XXYY`** (XX и YY — последние байты MAC, например `RC-Vehicle-A3B2`).
   - Пароль: **нет** (сеть открытая).

3. **Подключитесь** к этой сети.

4. **Откройте в браузере** любой из адресов:
   - **http://192.168.4.1** (основной IP точки доступа ESP32-C3)
   - или **http://rc-vehicle.local** (если mDNS настроен на устройстве).

5. Откроется **веб-интерфейс** управления: кнопки/джойстик для газа и руля, телеметрия (если RP2040 подключён по UART).

**Кратко:** Wi‑Fi → сеть `RC-Vehicle-XXYY` → в браузере **http://192.168.4.1**.

Чтобы заранее узнать SSID своей платы, после первой прошивки запустите `idf.py monitor` — в логах будет строка вида `Wi-Fi AP initialized. SSID: RC-Vehicle-XXXX`.

### Быстрые команды

```bash
# Полная сборка и прошивка с мониторингом
idf.py build flash monitor

# Очистка проекта
idf.py fullclean

# Очистка и пересборка
idf.py fullclean build

# Просмотр размера прошивки
idf.py size
idf.py size-components
idf.py size-files
```

### Решение проблем

#### Проблема: "Permission denied" при прошивке (Linux)
```bash
# Добавить пользователя в группу dialout
sudo usermod -a -G dialout $USER
# Перелогиниться или выполнить:
newgrp dialout
```

#### Проблема: Порт не найден
```bash
# Проверить доступные порты
ls /dev/ttyUSB* /dev/ttyACM*  # Linux
ls /dev/cu.*                   # macOS
```

#### Проблема: Ошибки компиляции
- Убедитесь, что используете ESP-IDF v5.5 (проверка: `idf.py --version`)
- **Установите целевую плату**: `idf.py set-target esp32c3` (обязательно перед первой сборкой!)
- Проверьте, что все зависимости установлены: `idf.py install`
- Очистите проект: `idf.py fullclean`

#### Проблема: Недостаточно памяти
- Используйте `idf.py size` для анализа использования памяти
- Отключите неиспользуемые компоненты в `menuconfig`

### Альтернативные способы сборки

#### PlatformIO
```ini
; platformio.ini
[env:esp32-c3-devkitm-1]
platform = espressif32
board = esp32-c3-devkitm-1
framework = espidf
board_build.mcu = esp32c3
```

#### Arduino Framework
Использовать Arduino IDE с поддержкой ESP32-C3 (требует адаптации кода).

## Стиль кода

- Заголовочные файлы: `.hpp` (не `.h`)
- Стандарт: Google C++ Style Guide
- Подробности: см. `docs/cpp_coding_style.md`

## Конфигурация

Основные параметры в `main/config.hpp`:
- SSID Wi-Fi AP
- Пароль (опционально)
- Порт HTTP (80)
- Порт WebSocket (81)
- Скорость UART (115200)
- Частоты отправки команд/телеметрии

## Протоколы

- **WebSocket**: JSON команды и телеметрия (см. `docs/interfaces_protocols.md`)
- **UART**: бинарные кадры с CRC16 (см. `docs/interfaces_protocols.md`)

## Статус реализации

- [ ] Wi-Fi AP
- [ ] HTTP сервер
- [ ] WebSocket сервер
- [ ] UART мост
- [ ] Протокол UART (парсинг/формирование кадров)
- [ ] Веб-интерфейс

