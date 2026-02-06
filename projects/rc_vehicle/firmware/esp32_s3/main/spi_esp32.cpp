#include "spi_esp32.hpp"

#include <cstring>

#include "esp_err.h"
#include "esp_log.h"

static const char* TAG = "spi_esp32";

SpiEsp32::SpiEsp32(spi_host_device_t host, gpio_num_t cs_pin, gpio_num_t sck_pin,
                   gpio_num_t mosi_pin, gpio_num_t miso_pin, int clock_hz)
    : host_(host),
      cs_pin_(cs_pin),
      sck_pin_(sck_pin),
      mosi_pin_(mosi_pin),
      miso_pin_(miso_pin),
      clock_hz_(clock_hz) {}

int SpiEsp32::Init() {
  if (inited_) return 0;

  spi_bus_config_t buscfg = {};
  buscfg.miso_io_num = miso_pin_;
  buscfg.mosi_io_num = mosi_pin_;
  buscfg.sclk_io_num = sck_pin_;
  buscfg.quadwp_io_num = GPIO_NUM_NC;
  buscfg.quadhd_io_num = GPIO_NUM_NC;
  buscfg.max_transfer_sz = 64;

  esp_err_t e = spi_bus_initialize(host_, &buscfg, SPI_DMA_CH_AUTO);
  if (e != ESP_OK && e != ESP_ERR_INVALID_STATE) {
    ESP_LOGE(TAG, "spi_bus_initialize failed: %s", esp_err_to_name(e));
    return -1;
  }

  spi_device_interface_config_t devcfg = {};
  devcfg.clock_speed_hz = clock_hz_;
  devcfg.mode = 0;
  devcfg.spics_io_num = cs_pin_;
  devcfg.queue_size = 1;

  e = spi_bus_add_device(host_, &devcfg, &dev_);
  if (e != ESP_OK) {
    ESP_LOGE(TAG, "spi_bus_add_device failed: %s", esp_err_to_name(e));
    return -1;
  }

  inited_ = true;
  ESP_LOGI(TAG, "SPI initialized (host=%d, cs=%d, sck=%d, mosi=%d, miso=%d, %d Hz)",
           (int)host_, (int)cs_pin_, (int)sck_pin_, (int)mosi_pin_, (int)miso_pin_,
           clock_hz_);
  return 0;
}

int SpiEsp32::Transfer(std::span<const uint8_t> tx, std::span<uint8_t> rx) {
  if (!inited_) return -1;
  if (tx.size() == 0 || tx.size() != rx.size()) return -1;

  spi_transaction_t t = {};
  t.length = tx.size() * 8;
  t.tx_buffer = tx.data();
  t.rx_buffer = rx.data();

  esp_err_t e = spi_device_transmit(dev_, &t);
  return (e == ESP_OK) ? 0 : -1;
}

