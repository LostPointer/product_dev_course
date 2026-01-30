#pragma once

#include <cstdint>

#include "spi_base.hpp"

/** Данные IMU: акселерометр (g), гироскоп (dps). */
struct ImuData {
  float ax{0.f}, ay{0.f}, az{0.f};
  float gx{0.f}, gy{0.f}, gz{0.f};
};

/**
 * Драйвер MPU-6050 по SPI.
 * Использует SpiBase для обмена; логика регистров и масштабирование — здесь.
 */
class Mpu6050Spi {
 public:
  explicit Mpu6050Spi(SpiBase *spi) : spi_(spi) {}

  /** Инициализация: проверка WHO_AM_I, сброс SLEEP. 0 — успех, -1 — ошибка. */
  int Init();

  /** Чтение акселерометра и гироскопа в data. 0 — успех, -1 — ошибка. */
  int Read(ImuData *data);

  /** Конвертация в формат телеметрии (mg, mdps → int16). */
  static void ConvertToTelem(const ImuData *data, int16_t *ax, int16_t *ay,
                             int16_t *az, int16_t *gx, int16_t *gy, int16_t *gz);

 private:
  SpiBase *spi_;
  bool initialized_{false};

  int ReadReg(uint8_t reg, uint8_t *value);
  int WriteReg(uint8_t reg, uint8_t value);
  int ReadReg16(uint8_t reg, int16_t *value);
};
