#include "config.hpp"
#include "failsafe.hpp"
#include "imu.hpp"
#include "platform.hpp"
#include "protocol.hpp"
#include "pwm_control.hpp"
#include "rc_input.hpp"
#include "uart_bridge.hpp"

#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/systick.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>

#if defined(STM32F1)
#include <libopencm3/stm32/f1/rcc.h>
#endif
#if defined(STM32F4)
#include <libopencm3/stm32/f4/rcc.h>
#endif
#if defined(STM32G4)
#include <libopencm3/stm32/g4/rcc.h>
#endif

static const char *TAG = "main";
static float current_throttle = 0.0f;
static float current_steering = 0.0f;
static bool rc_active = false;
static bool wifi_active = false;

static float ApplySlewRate(float target, float current, float max_per_sec,
                           uint32_t dt_ms) {
  float max_change = max_per_sec * (dt_ms / 1000.0f);
  float diff = target - current;
  if (diff > max_change)
    return current + max_change;
  if (diff < -max_change)
    return current - max_change;
  return target;
}

static void clock_setup(void) {
#if defined(STM32F1)
  rcc_clock_setup_pll(&rcc_hse_8mhz_3v3[RCC_CLOCK_3V3_72MHZ]);
#endif
#if defined(STM32F4)
  rcc_clock_setup_pll(&rcc_hse_25mhz_3v3[RCC_CLOCK_3V3_168MHZ]);
#endif
#if defined(STM32G4)
  // TODO: настройте тактирование G4 под вашу плату (HSE + PLL). См. примеры
  // libopencm3-examples.
#endif
}

int main(void) {
  clock_setup();
  platform_init();

  PwmControlInit();
  RcInputInit();
  ImuInit();
  UartBridgeInit();
  FailsafeInit();

  uint32_t last_pwm = platform_get_time_ms();
  uint32_t last_rc = platform_get_time_ms();
  uint32_t last_imu = platform_get_time_ms();
  uint32_t last_telem = platform_get_time_ms();
  uint32_t last_failsafe = platform_get_time_ms();

  ImuData imu_data = {0};
  TelemetryData telem_data = {0};
  uint16_t telem_seq = 0;

  while (1) {
    uint32_t now = platform_get_time_ms();

    if (now - last_pwm >= PWM_UPDATE_INTERVAL_MS) {
      uint32_t dt = now - last_pwm;
      last_pwm = now;
      float tt = ApplySlewRate(current_throttle, current_throttle,
                               SLEW_RATE_THROTTLE_MAX_PER_SEC, dt);
      float ts = ApplySlewRate(current_steering, current_steering,
                               SLEW_RATE_STEERING_MAX_PER_SEC, dt);
      current_throttle = tt;
      current_steering = ts;
      PwmControlSetThrottle(current_throttle);
      PwmControlSetSteering(current_steering);
    }

    if (now - last_rc >= RC_IN_POLL_INTERVAL_MS) {
      last_rc = now;
      float rc_thr, rc_str;
      rc_active = RcInputReadThrottle(&rc_thr) && RcInputReadSteering(&rc_str);
      if (rc_active) {
        current_throttle = rc_thr;
        current_steering = rc_str;
        wifi_active = false;
      }
    }

    float wifi_thr, wifi_str;
    if (UartBridgeReceiveCommand(&wifi_thr, &wifi_str)) {
      if (!rc_active) {
        current_throttle = wifi_thr;
        current_steering = wifi_str;
        wifi_active = true;
      } else {
        wifi_active = false;
      }
    } else {
      wifi_active = false;
    }

    if (now - last_imu >= IMU_READ_INTERVAL_MS) {
      last_imu = now;
      ImuRead(&imu_data);
    }

    if (now - last_failsafe >= 10) {
      last_failsafe = now;
      if (FailsafeUpdate(rc_active, wifi_active)) {
        current_throttle = 0.0f;
        current_steering = 0.0f;
        PwmControlSetNeutral();
      }
    }

    if (now - last_telem >= TELEM_SEND_INTERVAL_MS) {
      last_telem = now;
      telem_data.seq = telem_seq++;
      telem_data.status = 0;
      if (rc_active)
        telem_data.status |= 0x01;
      if (wifi_active)
        telem_data.status |= 0x02;
      if (FailsafeIsActive())
        telem_data.status |= 0x04;
      ImuConvertToTelem(&imu_data, &telem_data.ax, &telem_data.ay,
                        &telem_data.az, &telem_data.gx, &telem_data.gy,
                        &telem_data.gz);
      UartBridgeSendTelem(&telem_data);
    }

    platform_delay_ms(1);
  }

  return 0;
}
