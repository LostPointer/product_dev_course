#pragma once

#include <cstdint>
#include <cstddef>

#include "esp_err.h"

/**
 * @file crash_logger.hpp
 * @brief Сохранение информации о крэше в NVS для последующего получения через веб-интерфейс.
 *
 * Принцип работы:
 *   1. В управляющем цикле каждые kHeartbeatIntervalMs мс вызывается CrashLoggerTick().
 *      Она записывает текущее время (uptime) и имя задачи в RTC SRAM.
 *      RTC SRAM сохраняется после программного сброса (panic, WDT, esp_restart),
 *      но очищается при отключении питания (power-on reset).
 *   2. При старте CrashLoggerInit() проверяет причину перезагрузки через esp_reset_reason().
 *      Если это был крэш (PANIC, INT_WDT, TASK_WDT, WDT, BROWNOUT) — данные из RTC SRAM
 *      и причина перезагрузки сохраняются в NVS.
 *   3. HTTP-эндпоинт GET /api/crash.json возвращает сохранённые данные в JSON.
 *   4. DELETE /api/crash.json очищает данные.
 */

/// Интервал между обновлениями RTC-памяти (мс).
/// При 500 Гц (2 мс период) обновление происходит каждые 100 мс.
static constexpr uint32_t kCrashLoggerHeartbeatIntervalMs = 100;

/**
 * Инициализация crash logger.
 * Вызывать однократно при старте ПОСЛЕ nvs_flash_init.
 * При обнаружении крэша записывает информацию в NVS и логирует предупреждение.
 */
void CrashLoggerInit();

/**
 * Обновление "пульса" в RTC-памяти.
 * Вызывать из управляющего цикла с текущим значением uptime.
 * Безопасен для вызова из ISR-контекста и времязависимых задач — только запись в SRAM.
 * @param uptime_ms Текущее время с момента старта в миллисекундах.
 */
void CrashLoggerTick(uint32_t uptime_ms) noexcept;

/**
 * Возвращает true если в NVS есть сохранённые данные о крэше.
 */
bool CrashLoggerHasData();

/**
 * Формирует JSON-строку с данными о последнем крэше.
 * @param buf    Буфер для записи JSON.
 * @param len    Размер буфера.
 * @return true если данные есть и буфер достаточен.
 */
bool CrashLoggerGetJson(char* buf, size_t len);

/**
 * Очищает сохранённые данные о крэше из NVS.
 * @return ESP_OK при успехе.
 */
esp_err_t CrashLoggerClear();
