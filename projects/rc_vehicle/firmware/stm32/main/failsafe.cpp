#include "failsafe.hpp"
#include "config.hpp"
#include "platform.hpp"

static bool failsafe_active = false;
static uint32_t last_active_source_time = 0;

void FailsafeInit(void) {
  failsafe_active = false;
  last_active_source_time = platform_get_time_ms();
}

bool FailsafeUpdate(bool rc_active, bool wifi_active) {
  uint32_t now = platform_get_time_ms();
  bool has_active = rc_active || wifi_active;
  if (has_active) {
    last_active_source_time = now;
    failsafe_active = false;
  } else {
    if ((now - last_active_source_time) >= FAILSAFE_TIMEOUT_MS)
      failsafe_active = true;
  }
  return failsafe_active;
}

bool FailsafeIsActive(void) { return failsafe_active; }
