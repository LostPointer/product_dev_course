import axios, { AxiosError, AxiosHeaders, CanceledError, type AxiosAdapter, type InternalAxiosRequestConfig } from 'axios'
import { HttpError } from '@lostpointer/web-http'
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { apiClient } from './client'
import { projectsClient } from './http/projectsClient'
import { projectsApi, usersApi } from './projects'

type Reply = { status?: number; data?: unknown; headers?: Record<string, string> }

/** Replaces the HTTP adapter: each call takes the next reply, the last one repeats. */
function serve(client: { defaults: { adapter?: unknown } }, ...replies: Reply[]) {
    let call = 0
    const adapter = vi.fn<AxiosAdapter>(async (config: InternalAxiosRequestConfig) => {
        const reply = replies[Math.min(call++, replies.length - 1)]
        const status = reply.status ?? 200
        const body = reply.data === undefined ? '' : typeof reply.data === 'string' ? reply.data : JSON.stringify(reply.data)
        const response = {
            config, data: body, status, statusText: '',
            headers: new AxiosHeaders(reply.headers ?? { 'content-type': 'application/json' }),
        }
        if (status >= 400) throw new AxiosError('failed', 'ERR_BAD_RESPONSE', config, undefined, response)
        return response
    })
    client.defaults.adapter = adapter
    return adapter
}

const projectsAdapter = projectsClient.defaults.adapter
const legacyAdapter = apiClient.defaults.adapter
const location = window.location

beforeEach(() => {
    Object.defineProperty(window, 'location', { configurable: true, value: { ...location, href: 'http://localhost/projects' } })
})

afterEach(() => {
    projectsClient.defaults.adapter = projectsAdapter
    apiClient.defaults.adapter = legacyAdapter
    Object.defineProperty(window, 'location', { configurable: true, value: location })
    document.cookie = 'csrf_token=; expires=Thu, 01 Jan 1970 00:00:00 GMT'
    vi.restoreAllMocks()
})

describe('projectsApi on the shared transport', () => {
    it('lists projects with params and unwraps {items,total}', async () => {
        const items = [{ id: 'p1', name: 'P1' }, { id: 'p2', name: 'P2' }]
        const adapter = serve(projectsClient, { data: { items, total: 2 } })
        await expect(projectsApi.list({ search: 'foo', role: 'owner', limit: 10, offset: 0 })).resolves.toEqual({ projects: items, total: 2 })
        expect(adapter.mock.lastCall?.[0]).toMatchObject({
            url: '/projects', method: 'get', params: { search: 'foo', role: 'owner', limit: 10, offset: 0 },
            withCredentials: true, timeout: 30000,
        })
        serve(projectsClient, { data: { total: 0 } })
        await expect(projectsApi.list()).resolves.toEqual({ projects: [], total: 0 })
    })

    it.each([
        ['get', () => projectsApi.get('p1'), 'get', '/projects/p1', undefined],
        ['create', () => projectsApi.create({ name: 'P1', description: 'd' } as never), 'post', '/projects', { name: 'P1', description: 'd' }],
        ['update', () => projectsApi.update('p1', { name: 'New' } as never), 'put', '/projects/p1', { name: 'New' }],
        ['listMembers', () => projectsApi.listMembers('p1'), 'get', '/projects/p1/members', undefined],
        ['addMember', () => projectsApi.addMember('p1', { user_id: 'u1', role: 'editor' } as never), 'post', '/projects/p1/members', { user_id: 'u1', role: 'editor' }],
        ['updateMemberRole', () => projectsApi.updateMemberRole('p1', 'u1', { role: 'viewer' } as never), 'put', '/projects/p1/members/u1/role', { role: 'viewer' }],
    ])('%s calls the right endpoint and returns the body', async (_name, call, method, url, body) => {
        const adapter = serve(projectsClient, { data: { id: 'x' } })
        await expect(call()).resolves.toEqual({ id: 'x' })
        const config = adapter.mock.lastCall![0]
        expect(config).toMatchObject({ method, url })
        expect(config.data === undefined ? undefined : JSON.parse(String(config.data))).toEqual(body)
    })

    it.each([
        ['delete', () => projectsApi.delete('p1'), '/projects/p1'],
        ['removeMember', () => projectsApi.removeMember('p1', 'u1'), '/projects/p1/members/u1'],
    ])('%s accepts an empty 204 body', async (_name, call, url) => {
        const adapter = serve(projectsClient, { status: 204 })
        await expect(call()).resolves.toBeUndefined()
        expect(adapter.mock.lastCall?.[0]).toMatchObject({ method: 'delete', url })
    })

    it('sends trace and request IDs, and CSRF only on state-changing requests', async () => {
        document.cookie = 'csrf_token=csrf-1'
        const adapter = serve(projectsClient, { data: { id: 'p1' } })
        await projectsApi.get('p1')
        const read = AxiosHeaders.from(adapter.mock.lastCall![0].headers)
        expect(read.get('X-Trace-Id')).toBeTruthy()
        expect(read.get('X-Request-Id')).toBeTruthy()
        expect(read.has('X-CSRF-Token')).toBe(false)
        await projectsApi.update('p1', { name: 'N' } as never)
        expect(AxiosHeaders.from(adapter.mock.lastCall![0].headers).get('X-CSRF-Token')).toBe('csrf-1')
    })
})

describe('errors and session recovery', () => {
    it('refreshes the session once on 401 and repeats the request', async () => {
        const refresh = vi.spyOn(axios, 'post').mockResolvedValue({ data: {} })
        const adapter = serve(projectsClient, { status: 401, data: { error: 'expired' } }, { data: { id: 'p1' } })
        await expect(projectsApi.get('p1')).resolves.toEqual({ id: 'p1' })
        expect(refresh).toHaveBeenCalledTimes(1)
        expect(refresh.mock.calls[0][0]).toMatch(/\/auth\/refresh$/)
        expect(adapter).toHaveBeenCalledTimes(2)
        expect(window.location.href).toBe('http://localhost/projects')
    })

    it('goes to the login page when the refresh fails, without a second retry', async () => {
        vi.spyOn(axios, 'post').mockRejectedValue(new AxiosError('refresh failed'))
        const adapter = serve(projectsClient, { status: 401, data: { error: 'expired' } })
        await expect(projectsApi.get('p1')).rejects.toBeInstanceOf(HttpError)
        expect(adapter).toHaveBeenCalledTimes(1)
        expect(window.location.href).toBe('/login')
    })

    it('does not loop when the repeated request is still unauthorized', async () => {
        const refresh = vi.spyOn(axios, 'post').mockResolvedValue({ data: {} })
        const adapter = serve(projectsClient, { status: 401, data: { error: 'expired' } })
        await expect(projectsApi.get('p1')).rejects.toMatchObject({ kind: 'api', status: 401 })
        expect(refresh).toHaveBeenCalledTimes(1)
        expect(adapter).toHaveBeenCalledTimes(2)
    })

    it('keeps the server message so screens can show it', async () => {
        serve(projectsClient, { status: 409, data: { error: 'Проект с таким именем уже есть' } })
        await expect(projectsApi.create({ name: 'Dup' } as never)).rejects.toMatchObject({
            kind: 'api', status: 409, message: 'Проект с таким именем уже есть',
        })
    })

    it('normalizes timeout, abort, network failure and non-JSON bodies', async () => {
        projectsClient.defaults.adapter = async (config) => { throw new AxiosError('timeout', AxiosError.ECONNABORTED, config) }
        await expect(projectsApi.get('p1')).rejects.toMatchObject({ kind: 'timeout' })

        projectsClient.defaults.adapter = async () => { throw new CanceledError() }
        await expect(projectsApi.get('p1')).rejects.toMatchObject({ kind: 'aborted' })

        projectsClient.defaults.adapter = async (config) => { throw new AxiosError('offline', AxiosError.ERR_NETWORK, config) }
        await expect(projectsApi.get('p1')).rejects.toMatchObject({ kind: 'network' })

        serve(projectsClient, { status: 502, data: '<html>bad gateway</html>', headers: { 'content-type': 'text/html' } })
        await expect(projectsApi.get('p1')).rejects.toMatchObject({ kind: 'api', status: 502 })

        serve(projectsClient, { data: '{"id":' })
        await expect(projectsApi.get('p1')).rejects.toMatchObject({ kind: 'invalid_response' })
    })
})

describe('usersApi (legacy client, unchanged)', () => {
    it('calls GET /api/v1/users/search and wraps the array in {users}', async () => {
        const users = [{ id: 'u1', username: 'alice', email: 'a@x' }]
        const adapter = serve(apiClient, { data: users })
        await expect(usersApi.search({ q: 'al', exclude_project_id: 'p1' })).resolves.toEqual({ users })
        expect(adapter.mock.lastCall?.[0]).toMatchObject({ url: '/api/v1/users/search', params: expect.objectContaining({ q: 'al', exclude_project_id: 'p1' }) })
        serve(apiClient, { data: { not: 'array' } })
        await expect(usersApi.search({ q: 'al' })).resolves.toEqual({ users: [] })
    })
})
