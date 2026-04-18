#include "crash_logger.hpp"

#include <cstdio>
#include <cstring>

#include "esp_attr.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char* TAG = "crash_logger";
static const char* NVS_NAMESPACE  = "crash_log";
static const char* NVS_KEY_VALID  = "valid";
static const char* NVS_KEY_REASON = "reason";
static const char* NVS_KEY_COUNT  = "count";
static const char* NVS_KEY_UPTIME = "uptime";
static const char* NVS_KEY_TASK   = "task";
static const char* NVS_KEY_RTC_OK = "rtc_ok";

// Маркер валидности RTC-данных.
static constexpr uint32_t kHeartbeatMagic = 0xC0FFEE42u;

// RTC SRAM — сохраняется после программного сброса (panic, WDT, esp_restart),
// но НЕ после отключения питания (power-on reset).
static RTC_DATA_ATTR uint32_t s_magic;
static RTC_DATA_ATTR uint32_t s_uptime_ms;
static RTC_DATA_ATTR char     s_task_name[16];
static RTC_DATA_ATTR uint32_t s_tick_counter;  // для дросселирования обновлений

static const char* ResetReasonName(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON:    return "POWER_ON";
    case ESP_RST_EXT:        return "EXTERNAL";
    case ESP_RST_SW:         return "SOFTWARE";
    case ESP_RST_PANIC:      return "PANIC";
    case ESP_RST_INT_WDT:    return "INT_WATCHDOG";
    case ESP_RST_TASK_WDT:   return "TASK_WATCHDOG";
    case ESP_RST_WDT:        return "WATCHDOG";
    case ESP_RST_DEEPSLEEP:  return "DEEP_SLEEP";
    case ESP_RST_BROWNOUT:   return "BROWNOUT";
    case ESP_RST_SDIO:       return "SDIO";
    default:                 return "UNKNOWN";
  }
}

static bool IsCrashReason(esp_reset_reason_t r) {
  return r == ESP_RST_PANIC    ||
         r == ESP_RST_INT_WDT  ||
         r == ESP_RST_TASK_WDT ||
         r == ESP_RST_WDT      ||
         r == ESP_RST_BROWNOUT;
}

void CrashLoggerInit() {
  const esp_reset_reason_t reason = esp_reset_reason();

  if (!IsCrashReason(reason)) {
    // Нормальная загрузка — сбросить RTC-маркер, чтобы устаревшие данные
    // не загрязнили следующий отчёт.
    s_magic = 0;
    return;
  }

  ESP_LOGW(TAG, "Crash detected! Reset reason: %s (code %d)",
           ResetReasonName(reason), static_cast<int>(reason));

  const bool rtc_valid = (s_magic == kHeartbeatMagic);
  if (rtc_valid) {
    ESP_LOGW(TAG, "Heartbeat snapshot: uptime=%lu ms, task=\"%s\"",
             static_cast<unsigned long>(s_uptime_ms), s_task_name);
  } else {
    ESP_LOGW(TAG, "No heartbeat data (no RTC snapshot before crash)");
  }

  nvs_handle_t handle;
  if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to open NVS namespace — crash info not saved");
    return;
  }

  // Накапливаем счётчик крэшей
  uint32_t count = 0;
  nvs_get_u32(handle, NVS_KEY_COUNT, &count);
  nvs_set_u32(handle, NVS_KEY_COUNT, count + 1);

  nvs_set_u8(handle, NVS_KEY_VALID,  1);
  nvs_set_u8(handle, NVS_KEY_REASON, static_cast<uint8_t>(reason));
  nvs_set_u8(handle, NVS_KEY_RTC_OK, rtc_valid ? 1 : 0);

  if (rtc_valid) {
    nvs_set_u32(handle, NVS_KEY_UPTIME, s_uptime_ms);
    nvs_set_str(handle, NVS_KEY_TASK,   s_task_name);
  } else {
    nvs_erase_key(handle, NVS_KEY_UPTIME);
    nvs_erase_key(handle, NVS_KEY_TASK);
  }

  nvs_commit(handle);
  nvs_close(handle);

  // Сброс маркера: следующая нормальная перезагрузка не будет снова сохранять данные
  s_magic = 0;

  ESP_LOGI(TAG, "Crash info saved to NVS (crash_count=%lu)",
           static_cast<unsigned long>(count + 1));
}

void CrashLoggerTick(uint32_t uptime_ms) noexcept {
  // Дроссель: обновлять RTC SRAM не чаще kCrashLoggerHeartbeatIntervalMs мс.
  // RTC SRAM — обычная SRAM, запись быстрая. Счётчик обновляем всегда, чтобы
  // определить интервал без вызова esp_timer (который небезопасен в критическом контексте).
  ++s_tick_counter;
  // При периоде control loop 2 мс: 50 тиков = 100 мс
  if (s_tick_counter < 50) return;
  s_tick_counter = 0;

  s_magic      = kHeartbeatMagic;
  s_uptime_ms  = uptime_ms;

  // pcTaskGetName(nullptr) возвращает имя текущей задачи FreeRTOS.
  // Безопасно для вызова из задачного контекста.
  const char* name = pcTaskGetName(nullptr);
  if (name) {
    strncpy(s_task_name, name, sizeof(s_task_name) - 1);
    s_task_name[sizeof(s_task_name) - 1] = '\0';
  }
}

bool CrashLoggerHasData() {
  nvs_handle_t handle;
  if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
    return false;
  }
  uint8_t valid = 0;
  nvs_get_u8(handle, NVS_KEY_VALID, &valid);
  nvs_close(handle);
  return valid != 0;
}

bool CrashLoggerGetJson(char* buf, size_t len) {
  nvs_handle_t handle;
  if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
    snprintf(buf, len, "{\"has_data\":false}");
    return false;
  }

  uint8_t valid = 0;
  nvs_get_u8(handle, NVS_KEY_VALID, &valid);

  if (!valid) {
    nvs_close(handle);
    snprintf(buf, len, "{\"has_data\":false}");
    return false;
  }

  uint8_t  reason_code = 0;
  uint32_t count       = 0;
  uint32_t uptime_ms   = 0;
  uint8_t  rtc_ok      = 0;
  char     task[16]    = "<unknown>";

  nvs_get_u8 (handle, NVS_KEY_REASON, &reason_code);
  nvs_get_u32(handle, NVS_KEY_COUNT,  &count);
  nvs_get_u8 (handle, NVS_KEY_RTC_OK, &rtc_ok);

  if (rtc_ok) {
    nvs_get_u32(handle, NVS_KEY_UPTIME, &uptime_ms);
    size_t task_len = sizeof(task);
    nvs_get_str(handle, NVS_KEY_TASK, task, &task_len);
  }

  nvs_close(handle);

  const char* reason_name =
      ResetReasonName(static_cast<esp_reset_reason_t>(reason_code));

  int written;
  if (rtc_ok) {
    written = snprintf(buf, len,
        "{"
          "\"has_data\":true,"
          "\"crash_count\":%lu,"
          "\"last_crash\":{"
            "\"reason\":\"%s\","
            "\"reason_code\":%u,"
            "\"uptime_ms\":%lu,"
            "\"last_task\":\"%s\","
            "\"heartbeat_valid\":true"
          "}"
        "}",
        static_cast<unsigned long>(count),
        reason_name,
        static_cast<unsigned>(reason_code),
        static_cast<unsigned long>(uptime_ms),
        task);
  } else {
    written = snprintf(buf, len,
        "{"
          "\"has_data\":true,"
          "\"crash_count\":%lu,"
          "\"last_crash\":{"
            "\"reason\":\"%s\","
            "\"reason_code\":%u,"
            "\"heartbeat_valid\":false"
          "}"
        "}",
        static_cast<unsigned long>(count),
        reason_name,
        static_cast<unsigned>(reason_code));
  }

  return (written > 0 && static_cast<size_t>(written) < len);
}

esp_err_t CrashLoggerClear() {
  nvs_handle_t handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Failed to open NVS for clear: %s", esp_err_to_name(err));
    return err;
  }

  err = nvs_erase_all(handle);
  if (err == ESP_OK) {
    err = nvs_commit(handle);
  }
  nvs_close(handle);

  if (err == ESP_OK) {
    ESP_LOGI(TAG, "Crash log cleared");
  } else {
    ESP_LOGW(TAG, "Failed to clear crash log: %s", esp_err_to_name(err));
  }
  return err;
}
