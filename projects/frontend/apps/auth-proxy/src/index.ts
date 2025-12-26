import 'dotenv/config'
import fastify, { FastifyReply, FastifyRequest } from 'fastify'
import cors from '@fastify/cors'
import cookie from '@fastify/cookie'
import rateLimit from '@fastify/rate-limit'
import httpProxy from '@fastify/http-proxy'
import { randomUUID } from 'crypto'

type Config = {
    port: number
    targetExperimentUrl: string
    authUrl: string
    corsOrigins: string[]
    cookieDomain?: string
    cookieSecure: boolean
    cookieSameSite: 'lax' | 'strict' | 'none'
    accessCookieName: string
    refreshCookieName: string
    accessTtlSec: number
    refreshTtlSec: number
    rateLimitWindowMs: number
    rateLimitMax: number
    logLevel: string
}

function parseConfig(): Config {
    const num = (value: string | undefined, fallback: number) => {
        const parsed = value ? Number(value) : NaN
        return Number.isFinite(parsed) ? parsed : fallback
    }

    const bool = (value: string | undefined, fallback: boolean) => {
        if (value === undefined) return fallback
        return ['1', 'true', 'yes', 'on'].includes(value.toLowerCase())
    }

    return {
        port: num(process.env.PORT, 8080),
        targetExperimentUrl:
            process.env.TARGET_EXPERIMENT_URL || 'http://localhost:8002',
        authUrl: process.env.AUTH_URL || 'http://localhost:8001',
        corsOrigins: (process.env.CORS_ORIGINS || 'http://localhost:3000')
            .split(',')
            .map((o) => o.trim())
            .filter(Boolean),
        cookieDomain: process.env.COOKIE_DOMAIN,
        cookieSecure: bool(process.env.COOKIE_SECURE, false),
        cookieSameSite:
            (process.env.COOKIE_SAMESITE as Config['cookieSameSite']) || 'lax',
        accessCookieName: process.env.ACCESS_COOKIE_NAME || 'access_token',
        refreshCookieName: process.env.REFRESH_COOKIE_NAME || 'refresh_token',
        accessTtlSec: num(process.env.ACCESS_TTL_SEC, 900),
        refreshTtlSec: num(process.env.REFRESH_TTL_SEC, 1_209_600),
        rateLimitWindowMs: num(process.env.RATE_LIMIT_WINDOW_MS, 60_000),
        rateLimitMax: num(process.env.RATE_LIMIT_MAX, 60),
        logLevel: process.env.LOG_LEVEL || 'info',
    }
}

type AuthTokens = {
    access_token: string
    refresh_token?: string
    expires_in?: number
    refresh_expires_in?: number
    token_type?: string
    [key: string]: unknown
}

function parseCookies(header: string | undefined): Record<string, string> {
    if (!header) return {}
    return header
        .split(';')
        .map((v) => v.trim())
        .filter(Boolean)
        .reduce<Record<string, string>>((acc, pair) => {
            const idx = pair.indexOf('=')
            if (idx === -1) return acc
            const key = decodeURIComponent(pair.slice(0, idx).trim())
            const val = decodeURIComponent(pair.slice(idx + 1).trim())
            acc[key] = val
            return acc
        }, {})
}

function setAuthCookies(
    reply: FastifyReply,
    cfg: Config,
    tokens: AuthTokens,
    opts?: { skipRefresh?: boolean }
) {
    const accessTtl = tokens.expires_in ?? cfg.accessTtlSec
    reply.setCookie(cfg.accessCookieName, tokens.access_token, {
        httpOnly: true,
        secure: cfg.cookieSecure,
        sameSite: cfg.cookieSameSite,
        domain: cfg.cookieDomain,
        path: '/',
        maxAge: accessTtl,
    })

    if (!opts?.skipRefresh && tokens.refresh_token) {
        const refreshTtl = tokens.refresh_expires_in ?? cfg.refreshTtlSec
        reply.setCookie(cfg.refreshCookieName, tokens.refresh_token, {
            httpOnly: true,
            secure: cfg.cookieSecure,
            sameSite: cfg.cookieSameSite,
            domain: cfg.cookieDomain,
            path: '/',
            maxAge: refreshTtl,
        })
    }
}

function clearAuthCookies(reply: FastifyReply, cfg: Config) {
    reply.clearCookie(cfg.accessCookieName, { path: '/' })
    reply.clearCookie(cfg.refreshCookieName, { path: '/' })
}

/**
 * Извлекает или генерирует trace_id и request_id из заголовков
 */
function getTraceContext(request: FastifyRequest): {
    traceId: string
    requestId: string
} {
    const traceId = (request.headers['x-trace-id'] as string) || randomUUID()
    const requestId = (request.headers['x-request-id'] as string) || request.id || randomUUID()
    return { traceId, requestId }
}

async function buildServer(config: Config) {
    const app = fastify({
        logger: {
            level: config.logLevel,
            serializers: {
                req: (req) => {
                    const { traceId, requestId } = getTraceContext(req as FastifyRequest)
                    return {
                        method: req.method,
                        url: req.url,
                        trace_id: traceId,
                        request_id: requestId,
                    }
                },
                res: (res) => {
                    return {
                        statusCode: res.statusCode,
                    }
                },
            },
            redact: [
                'req.headers.authorization',
                'request.headers.authorization',
                'req.headers.cookie',
                'request.headers.cookie',
                'reply.headers.set-cookie',
                'response.headers.set-cookie',
            ],
        },
        trustProxy: true,
    })

    await app.register(cookie)

    await app.register(cors, {
        origin: config.corsOrigins,
        credentials: true,
    })

    await app.register(rateLimit, {
        max: config.rateLimitMax,
        timeWindow: config.rateLimitWindowMs,
        allowList: (req) => req.url === '/health',
    })

    // Middleware для извлечения и логирования trace_id и request_id
    app.addHook('onRequest', async (request, reply) => {
        const { traceId, requestId } = getTraceContext(request)

            // Сохраняем в request для использования в обработчиках
            ; (request as any).traceId = traceId
            ; (request as any).requestId = requestId

        // Добавляем trace_id и request_id в контекст логгера для всех последующих логов
        request.log = request.log.child({
            trace_id: traceId,
            request_id: requestId,
            service: 'auth-proxy',
        })

        // Логирование запроса с trace_id и request_id
        request.log.info({
            method: request.method,
            url: request.url,
        }, 'Incoming request')
    })

    // Hook для логирования ответов
    app.addHook('onResponse', async (request, reply) => {
        request.log.info({
            method: request.method,
            url: request.url,
            statusCode: reply.statusCode,
        }, 'Request completed')
    })

    // Auth routes (login/refresh/logout/me) — устанавливают куки
    app.post('/auth/login', async (request, reply) => {
        const { traceId, requestId } = getTraceContext(request)

        const res = await fetch(`${config.authUrl}/auth/login`, {
            method: 'POST',
            headers: {
                'content-type': 'application/json',
                'X-Trace-Id': traceId,
                'X-Request-Id': requestId,
            },
            body: JSON.stringify(request.body ?? {}),
        })

        if (!res.ok) {
            reply.status(res.status)
            return res.json().catch(() => ({}))
        }

        const data = (await res.json()) as AuthTokens
        if (!data.access_token) {
            reply.status(502)
            return { error: 'Auth service response missing access_token' }
        }

        setAuthCookies(reply, config, data)

        const { access_token, refresh_token, ...rest } = data
        return rest
    })

    app.post('/auth/refresh', async (request, reply) => {
        const refreshToken =
            request.cookies[config.refreshCookieName] ??
            (request.body as Record<string, unknown> | undefined)?.[
            config.refreshCookieName
            ]

        if (!refreshToken) {
            reply.status(401)
            return { error: 'Refresh token not provided' }
        }

        const { traceId, requestId } = getTraceContext(request)

        const res = await fetch(`${config.authUrl}/auth/refresh`, {
            method: 'POST',
            headers: {
                'content-type': 'application/json',
                'X-Trace-Id': traceId,
                'X-Request-Id': requestId,
            },
            body: JSON.stringify({ refresh_token: refreshToken }),
        })

        if (!res.ok) {
            clearAuthCookies(reply, config)
            reply.status(res.status)
            return res.json().catch(() => ({}))
        }

        const data = (await res.json()) as AuthTokens
        if (!data.access_token) {
            reply.status(502)
            return { error: 'Auth service response missing access_token' }
        }

        setAuthCookies(reply, config, data)
        const { access_token, refresh_token, ...rest } = data
        return rest
    })

    app.post('/auth/logout', async (request, reply) => {
        const { traceId, requestId } = getTraceContext(request)

        // Best-effort revoke
        try {
            await fetch(`${config.authUrl}/auth/logout`, {
                method: 'POST',
                headers: {
                    'content-type': 'application/json',
                    'X-Trace-Id': traceId,
                    'X-Request-Id': requestId,
                },
                body: JSON.stringify({}),
            })
        } catch (err) {
            request.log.warn(
                { err, trace_id: traceId, request_id: requestId },
                'Auth logout upstream failed'
            )
        }

        clearAuthCookies(reply, config)
        return { ok: true }
    })

    app.get('/auth/me', async (request, reply) => {
        const access = request.cookies[config.accessCookieName]
        if (!access) {
            reply.status(401)
            return { error: 'Unauthorized' }
        }

        const { traceId, requestId } = getTraceContext(request)

        const res = await fetch(`${config.authUrl}/auth/me`, {
            headers: {
                authorization: `Bearer ${access}`,
                'X-Trace-Id': traceId,
                'X-Request-Id': requestId,
            },
        })

        if (!res.ok) {
            if (res.status === 401) {
                clearAuthCookies(reply, config)
            }
            reply.status(res.status)
            return res.json().catch(() => ({}))
        }

        return res.json().catch(() => ({}))
    })

    // Experiment service proxy (+future gateway), with WS/SSE
    await app.register(httpProxy, {
        prefix: '/api',
        upstream: config.targetExperimentUrl,
        rewritePrefix: '/api',
        http2: false,
        websocket: true,
        replyOptions: {
            rewriteRequestHeaders: (req, headers) => {
                const cookies = parseCookies(req.headers.cookie as string | undefined)
                const access = cookies[config.accessCookieName]
                const traceId = (req.headers['x-trace-id'] as string) || randomUUID()
                const requestId = (req.headers['x-request-id'] as string) || req.id || randomUUID()

                return {
                    ...headers,
                    'X-Trace-Id': traceId,
                    'X-Request-Id': requestId,
                    ...(access ? { authorization: `Bearer ${access}` } : {}),
                }
            },
        },
    })

    app.get('/health', async () => ({ status: 'ok' }))

    return app
}

async function start() {
    const config = parseConfig()
    const app = await buildServer(config)

    try {
        await app.listen({ port: config.port, host: '0.0.0.0' })
        app.log.info(
            { port: config.port, upstream: config.targetExperimentUrl },
            'Auth proxy started'
        )
    } catch (err) {
        app.log.error(err, 'Failed to start auth proxy')
        process.exit(1)
    }
}

start()
