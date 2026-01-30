#pragma once

#include <cstddef>
#include <cstdint>

/**
 * Базовый класс SPI-драйвера.
 * Наследники реализуют Init() и Transfer() под конкретный чип (RP2040, STM32).
 * Transfer() выполняет полнодуплексный обмен: реализация должна держать CS
 * активным на время обмена (CS low → обмен → CS high).
 */
class SpiBase {
 public:
  virtual ~SpiBase() = default;

  /** Инициализация SPI и пина CS. Возврат: 0 при успехе, -1 при ошибке. */
  virtual int Init() = 0;

  /**
   * Полнодуплексный обмен: отправить len байт из tx, принять len байт в rx.
   * Реализация держит CS активным на время всего обмена.
   * @param tx данные для отправки (не nullptr)
   * @param rx буфер приёма (не nullptr; может совпадать с tx для in-place)
   * @param len количество байт
   * @return 0 при успехе, -1 при ошибке
   */
  virtual int Transfer(const uint8_t *tx, uint8_t *rx, size_t len) = 0;
};
