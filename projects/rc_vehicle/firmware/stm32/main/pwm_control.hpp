#pragma once
#include <stdint.h>

int PwmControlInit(void);
int PwmControlSetThrottle(float throttle);
int PwmControlSetSteering(float steering);
void PwmControlSetNeutral(void);
