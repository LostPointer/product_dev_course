#include "imu.hpp"
#include "config.hpp"
// TODO: реализовать на libopencm3 (I2C, MPU-6050). Пины в board_pins.hpp

static bool initialized = false;

int ImuInit(void) {
  // TODO: i2c_peripheral_init, проверка WHO_AM_I
  initialized = true;
  return 0;
}

int ImuRead(ImuData *data) {
  if (!initialized || !data)
    return -1;
  data->ax = data->ay = data->az = 0.0f;
  data->gx = data->gy = data->gz = 0.0f;
  // TODO: чтение регистров MPU-6050
  return 0;
}

void ImuConvertToTelem(const ImuData *data, int16_t *ax, int16_t *ay,
                       int16_t *az, int16_t *gx, int16_t *gy, int16_t *gz) {
  if (!data || !ax || !ay || !az || !gx || !gy || !gz)
    return;
  *ax = (int16_t)(data->ax * 1000);
  *ay = (int16_t)(data->ay * 1000);
  *az = (int16_t)(data->az * 1000);
  *gx = (int16_t)(data->gx * 1000);
  *gy = (int16_t)(data->gy * 1000);
  *gz = (int16_t)(data->gz * 1000);
}
