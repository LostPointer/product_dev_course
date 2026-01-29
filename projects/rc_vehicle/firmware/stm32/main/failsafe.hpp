#pragma once
#include <stdbool.h>

void FailsafeInit(void);
bool FailsafeUpdate(bool rc_active, bool wifi_active);
bool FailsafeIsActive(void);
