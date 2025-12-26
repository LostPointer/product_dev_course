/**
 * Утилиты для генерации UUID v4
 * Используются для trace_id и request_id
 */

/**
 * Генерирует UUID v4
 * @returns UUID строка в формате xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
 */
export function generateUUID(): string {
    return 'xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx'.replace(/[xy]/g, (c) => {
        const r = (Math.random() * 16) | 0
        const v = c === 'x' ? r : (r & 0x3) | 0x8
        return v.toString(16)
    })
}

/**
 * Генерирует trace_id для отслеживания запроса через несколько сервисов
 * Сохраняется в течение пользовательской сессии
 */
export function generateTraceId(): string {
    return generateUUID()
}

/**
 * Генерирует request_id для каждого отдельного HTTP запроса
 */
export function generateRequestId(): string {
    return generateUUID()
}

