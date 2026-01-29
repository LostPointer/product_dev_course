#pragma once
#include <stdint.h>

// Время в мс с запуска (используется для таймеров и failsafe)
uint32_t platform_get_time_ms(void);
void platform_delay_ms(uint32_t ms);
