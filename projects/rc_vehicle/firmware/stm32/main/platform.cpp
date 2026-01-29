#include "platform.hpp"
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/systick.h>

static volatile uint32_t _millis = 0;

extern "C" void sys_tick_handler(void) { _millis++; }

uint32_t platform_get_time_ms(void) { return _millis; }

void platform_delay_ms(uint32_t ms) {
  uint32_t end = _millis + ms;
  while (_millis < end) {
    __asm__("nop");
  }
}

// 1 ms тик: SysTick = CPU_freq/1000 - 1. Подставьте частоту под вашу плату (F1:
// 72M, F4: 96/168M, G4: 170M).
#ifndef SYSTICK_RELOAD
#define SYSTICK_RELOAD (72000000 / 1000 - 1) // 72 MHz по умолчанию (STM32F1)
#endif

void platform_init(void) {
  systick_set_clocksource(STK_CSR_CLKSOURCE_AHB);
  systick_set_reload(SYSTICK_RELOAD);
  systick_interrupt_enable();
  systick_counter_enable();
  nvic_set_priority(NVIC_SYSTICK_IRQ, 0x80);
}
