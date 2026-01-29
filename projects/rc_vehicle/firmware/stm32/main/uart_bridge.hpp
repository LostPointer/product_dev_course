#pragma once
#include <stdint.h>

int UartBridgeInit(void);
int UartBridgeSendTelem(const void *telem_data);
bool UartBridgeReceiveCommand(float *throttle, float *steering);
