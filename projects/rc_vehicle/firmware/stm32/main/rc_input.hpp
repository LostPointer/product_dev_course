#pragma once
#include <stdbool.h>

int RcInputInit(void);
bool RcInputReadThrottle(float *throttle);
bool RcInputReadSteering(float *steering);
