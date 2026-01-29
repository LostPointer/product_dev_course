#include "uart_bridge.hpp"
#include "uart_bridge_base.hpp"
// TODO: реализовать Init/Write/ReadAvailable на libopencm3 (USART, пины в
// board_pins.hpp)

class Stm32UartBridge : public UartBridgeBase {
public:
  int Init() override {
    // TODO: rcc_periph_clock_enable USART+GPIO, gpio_set_af,
    // usart_set_baudrate, usart_enable
    rx_pos_ = 0;
    return 0;
  }

  int Write(const uint8_t *data, size_t len) override {
    if (data == nullptr)
      return -1;
    // TODO: usart_send_blocking в цикле по data[0..len-1]
    (void)len;
    return 0;
  }

  int ReadAvailable(uint8_t *buf, size_t max_len) override {
    if (buf == nullptr || max_len == 0)
      return 0;
    // TODO: чтение из USART RX (poll или из кольцевого буфера прерываний)
    return 0;
  }
};

static Stm32UartBridge s_bridge;

int UartBridgeInit(void) { return s_bridge.Init(); }

int UartBridgeSendTelem(const void *telem_data) {
  if (telem_data == nullptr)
    return -1;
  return s_bridge.SendTelem(*static_cast<const TelemetryData *>(telem_data));
}

bool UartBridgeReceiveCommand(float *throttle, float *steering) {
  auto cmd = s_bridge.ReceiveCommand();
  if (cmd) {
    *throttle = cmd->throttle;
    *steering = cmd->steering;
    return true;
  }
  return false;
}
