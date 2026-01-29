#include "pwm_control.hpp"
#include "config.hpp"
// TODO: реализовать на libopencm3 (timer PWM, 50 Hz). Пины в board_pins.hpp

static bool initialized = false;

int PwmControlInit(void) {
  // TODO: rcc_periph_clock_enable для TIM и GPIO, gpio_set_af для PWM пинов,
  // timer_set_period 50 Hz, timer_enable_oc_output
  initialized = true;
  return 0;
}

int PwmControlSetThrottle(float /*throttle*/) {
  if (!initialized)
    return -1;
  // TODO: timer_set_oc_value(...)
  return 0;
}

int PwmControlSetSteering(float /*steering*/) {
  if (!initialized)
    return -1;
  // TODO: timer_set_oc_value(...)
  return 0;
}

void PwmControlSetNeutral(void) {
  if (!initialized)
    return;
  // TODO: установить нейтральные значения PWM
}
