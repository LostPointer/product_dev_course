# Интерфейсы и протоколы (черновик)

Цель документа — зафиксировать минимальные протоколы взаимодействия:
- **браузер ↔ ESP32**: WebSocket команды управления и телеметрия в UI
- **ESP32 ↔ STM32**: UART кадры (COMMAND/TELEM) с CRC16

## 1) WebSocket (браузер ↔ ESP32)

### 1.1 Команда управления (клиент → ESP32)
- **Формат**: JSON
- **Частота**: до 50–100 Hz (MVP)
- **Диапазоны**:
  - `throttle`: `[-1..1]` (газ/тормоз; 0 = нейтраль)
  - `steering`: `[-1..1]` (руль; 0 = центр)
  - алиасы для совместимости: `thr` ≡ `throttle`, `steer` ≡ `steering`

Пример:

```json
{"type":"cmd","throttle":0.2,"steering":-0.1,"seq":123}
```

Поля:
- `type`: `"cmd"`
- `throttle`, `steering`: float
- `thr`, `steer`: float (алиасы, опционально)
- `seq`: int, опционально (для отладки/дедупликации)

### 1.2 Телеметрия (ESP32 → клиент)
- **Формат**: JSON
- **Рекомендуемая частота**: 10–50 Hz (MVP)

Пример:

```json
{
  "type":"telem",
  "ts_ms":1734690000000,
  "link":{"active_source":"rc","rc_ok":true,"wifi_ok":true},
  "imu":{"ax":0.01,"ay":0.02,"az":9.81,"gx":0.1,"gy":0.0,"gz":-0.2},
  "act":{"throttle":0.18,"steering":-0.08}
}
```

## 2) UART (ESP32 ↔ STM32)

### 2.1 Общий кадр
Все сообщения идут кадрами следующего вида:

```
AA 55 | VER | TYPE | LEN_LO LEN_HI | PAYLOAD... | CRC16_LO CRC16_HI
```

- `AA 55`: префикс (2 байта)
- `VER`: версия протокола (1 байт), MVP: `0x01`
- `TYPE`: тип сообщения (1 байт)
- `LEN`: длина `PAYLOAD` (uint16 LE)
- `CRC16`: CRC-16/IBM (Modbus), считается по `VER..PAYLOAD` (то есть без `AA55`, но включая `VER/TYPE/LEN/PAYLOAD`)

**Примечание**: точный вариант CRC можно заменить, но важно зафиксировать один и тот же на ESP32 и STM32.

### 2.2 Типы сообщений
- `0x01` **COMMAND** (ESP32 → STM32): целевые `throttle/steering` (алиасы в WS: `thr/steer`)
- `0x02` **TELEM** (STM32 → ESP32): состояние + IMU
- `0x03` **PING** / `0x04` **PONG** (опционально)

### 2.3 PAYLOAD: COMMAND (TYPE=0x01)

MVP бинарный payload:

```
SEQ_LO SEQ_HI | THR_I16 | STEER_I16 | FLAGS
```

- `SEQ`: uint16 LE
- `THR_I16`: int16 LE, нормализованный `thr * 32767`
- `STEER_I16`: int16 LE, нормализованный `steer * 32767`
- `FLAGS`:
  - bit0: `slew_enable` (если хотим включать/выключать ограничение скорости изменения)

### 2.4 PAYLOAD: TELEM (TYPE=0x02)

MVP бинарный payload:

```
SEQ_LO SEQ_HI | STATUS | AX_I16 AY_I16 AZ_I16 | GX_I16 GY_I16 GZ_I16
```

- `STATUS`:
  - bit0: `rc_ok`
  - bit1: `wifi_ok`
  - bit2: `failsafe_active`
  - bit3..7: reserved
- IMU поля: int16 LE (масштабирование фиксируем в прошивке, напр. mg и mdps)

## 3) Тайминги и failsafe (договорённости)
- RC имеет приоритет над Wi‑Fi.
- Если **активный источник управления** отсутствует > **250 мс**, STM32 включает **failsafe**:
  - газ = нейтраль/0
  - руль = центр/0
- Переключение источников управления не должно вызывать “скачок газа” (используем slew‑rate и/или “hold last safe”).


