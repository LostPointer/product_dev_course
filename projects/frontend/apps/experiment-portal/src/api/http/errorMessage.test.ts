import { HttpError } from '@lostpointer/web-http'
import { describe, expect, it } from 'vitest'
import { apiErrorMessage } from './errorMessage'

describe('apiErrorMessage', () => {
    it('shows the server message of a shared-transport error', () => {
        expect(apiErrorMessage(new HttpError('api', 409, 'conflict', 'Имя занято'), 'Ошибка')).toBe('Имя занято')
    })

    it.each([
        new HttpError('api', 500, 'http_error', 'HTTP 500'),
        new HttpError('network', undefined, 'network_error', 'Network request failed'),
        new HttpError('timeout', undefined, 'timeout', 'Request timed out'),
        new HttpError('aborted', undefined, 'aborted', 'Request aborted'),
        new HttpError('invalid_response', 200, 'invalid_json', 'Invalid JSON response'),
    ])('uses the fallback for $kind without a server message', (error) => {
        expect(apiErrorMessage(error, 'Ошибка сохранения')).toBe('Ошибка сохранения')
    })

    it('keeps reading legacy Axios error bodies', () => {
        expect(apiErrorMessage({ response: { data: { error: 'Нет прав' } } }, 'Ошибка')).toBe('Нет прав')
        expect(apiErrorMessage({ response: { data: { message: 'Не найдено' } } }, 'Ошибка')).toBe('Не найдено')
        expect(apiErrorMessage(new Error('boom'), 'Ошибка')).toBe('Ошибка')
        expect(apiErrorMessage(null, 'Ошибка')).toBe('Ошибка')
    })
})
