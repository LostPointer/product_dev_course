#pragma once
#include <stdint.h>

struct ImuData {
  float ax, ay, az;
  float gx, gy, gz;
};

int ImuInit(void);
int ImuRead(ImuData *data);
void ImuConvertToTelem(const ImuData *data, int16_t *ax, int16_t *ay,
                       int16_t *az, int16_t *gx, int16_t *gy, int16_t *gz);
