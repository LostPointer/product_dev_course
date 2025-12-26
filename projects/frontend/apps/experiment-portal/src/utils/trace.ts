/**
 * Управление trace_id для отслеживания запросов через несколько сервисов
 * trace_id сохраняется в течение пользовательской сессии
 */
import { generateTraceId } from './uuid'

// Генерируем trace_id один раз при инициализации модуля
// Он будет использоваться для всех запросов в течение сессии
let sessionTraceId = generateTraceId()

/**
 * Получить текущий trace_id
 */
export function getTraceId(): string {
    return sessionTraceId
}

/**
 * Сбросить trace_id (генерирует новый)
 * Используется при начале нового действия пользователя
 */
export function resetTraceId(): void {
    sessionTraceId = generateTraceId()
}

