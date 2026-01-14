import { describe, expect, it, vi, beforeEach } from 'vitest'
import {
  sanitizeHeaders,
  sanitizeBody,
  truncateString,
  formatHttpDebugText,
  shouldEmitDebugToast,
} from './httpDebug'

describe('httpDebug', () => {
  beforeEach(() => {
    // Stabilize time for dedupe tests
    vi.useFakeTimers()
    vi.setSystemTime(new Date('2026-01-01T00:00:00.000Z'))
  })

  it('sanitizeHeaders masks Authorization/Cookie/CSRF and token-like headers', () => {
    const out = sanitizeHeaders({
      Authorization: 'Bearer secret',
      cookie: 'sid=abc',
      'X-CSRF-Token': 'csrf',
      'X-Api-Key': 'k',
      'X-Some-Token': 't',
      'Content-Type': 'application/json',
    })

    expect(out.Authorization).toBe('[REDACTED]')
    expect(out.cookie).toBe('[REDACTED]')
    expect(out['X-CSRF-Token']).toBe('[REDACTED]')
    expect(out['X-Api-Key']).toBe('[REDACTED]')
    expect(out['X-Some-Token']).toBe('[REDACTED]')
    expect(out['Content-Type']).toBe('application/json')
  })

  it('sanitizeBody masks common secret fields recursively', () => {
    const out = sanitizeBody({
      username: 'u',
      password: 'p',
      nested: {
        token: 't',
        refresh_token: 'rt',
        deep: [{ access_token: 'at' }, { ok: true }],
      },
    }) as any

    expect(out.password).toBe('[REDACTED]')
    expect(out.nested.token).toBe('[REDACTED]')
    expect(out.nested.refresh_token).toBe('[REDACTED]')
    expect(out.nested.deep[0].access_token).toBe('[REDACTED]')
    expect(out.nested.deep[1].ok).toBe(true)
  })

  it('truncateString truncates long strings and adds a marker', () => {
    const s = 'a'.repeat(20)
    const out = truncateString(s, 10)
    expect(out.startsWith('a'.repeat(10))).toBe(true)
    expect(out).toContain('[truncated')
  })

  it('formatHttpDebugText includes request/response and correlation IDs', () => {
    const text = formatHttpDebugText({
      kind: 'http',
      message: 'boom',
      request: {
        method: 'GET',
        url: 'http://example/api',
        headers: { 'X-Trace-Id': 't', 'X-Request-Id': 'r', Authorization: '[REDACTED]' },
      },
      response: {
        status: 500,
        headers: { 'x-request-id': 'srv-r' },
        body: { error: 'fail' },
      },
      correlation: { trace_id: 't', request_id: 'r', x_request_id: 'srv-r' },
    })

    expect(text).toContain('Request:')
    expect(text).toContain('Response:')
    expect(text).toContain('Correlation:')
    expect(text).toContain('trace_id: t')
    expect(text).toContain('request_id: r')
    expect(text).toContain('x-request-id: srv-r')
  })

  it('shouldEmitDebugToast dedupes identical errors in a short window', () => {
    const info = {
      kind: 'http' as const,
      message: 'Network Error',
      request: { method: 'GET', url: 'http://x' },
      response: undefined,
      correlation: undefined,
    }

    expect(shouldEmitDebugToast(info)).toBe(true)
    expect(shouldEmitDebugToast(info)).toBe(false)

    vi.advanceTimersByTime(4000)
    expect(shouldEmitDebugToast(info)).toBe(true)
  })
})

