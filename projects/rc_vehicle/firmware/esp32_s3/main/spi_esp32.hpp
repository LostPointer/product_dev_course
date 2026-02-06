#pragma once

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "spi_base.hpp"

/** Реализация SpiBase для ESP32 (ESP-IDF SPI master). */
class SpiEsp32 : public SpiBase {
 public:
  SpiEsp32(spi_host_device_t host, gpio_num_t cs_pin, gpio_num_t sck_pin,
           gpio_num_t mosi_pin, gpio_num_t miso_pin, int clock_hz);

  int Init() override;
  int Transfer(std::span<const uint8_t> tx, std::span<uint8_t> rx) override;

 private:
  spi_host_device_t host_;
  gpio_num_t cs_pin_;
  gpio_num_t sck_pin_;
  gpio_num_t mosi_pin_;
  gpio_num_t miso_pin_;
  int clock_hz_;

  spi_device_handle_t dev_{nullptr};
  bool inited_{false};
};

