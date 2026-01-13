import { describe, it, expect, vi, beforeEach } from 'vitest'

// Используем vi.hoisted для правильного порядка инициализации
const { mockAxiosInstance, mockCreate } = vi.hoisted(() => {
    const instance = {
        post: vi.fn(),
        get: vi.fn(),
        interceptors: {
            request: {
                use: vi.fn(),
            },
            response: {
                use: vi.fn(),
            },
        },
    }
    const create = vi.fn(() => instance)
    return { mockAxiosInstance: instance, mockCreate: create }
})

// Мокаем axios ДО импорта authApi
vi.mock('axios', () => {
    return {
        default: {
            create: mockCreate,
        },
    }
})

// Импортируем после мока
import { authApi } from './auth'

const requestInterceptor = mockAxiosInstance.interceptors.request.use.mock.calls[0]?.[0] as
    | ((cfg: any) => any)
    | undefined

describe('authApi', () => {
    beforeEach(() => {
        vi.clearAllMocks()
    })

    describe('login', () => {
        it('calls POST /auth/login with credentials', async () => {
            const mockResponse = {
                data: { expires_in: 900, token_type: 'bearer' },
            }
            mockAxiosInstance.post.mockResolvedValueOnce(mockResponse)

            const credentials = { username: 'testuser', password: 'password123' }
            const result = await authApi.login(credentials)

            expect(mockAxiosInstance.post).toHaveBeenCalledWith(
                '/auth/login',
                credentials
            )
            expect(result).toEqual(mockResponse.data)
        })

        it('handles login errors', async () => {
            const error = new Error('Invalid credentials')
            mockAxiosInstance.post.mockRejectedValueOnce(error)

            await expect(
                authApi.login({ username: 'testuser', password: 'wrong' })
            ).rejects.toThrow('Invalid credentials')
        })
    })

    describe('logout', () => {
        it('calls POST /auth/logout', async () => {
            mockAxiosInstance.post.mockResolvedValueOnce({ data: {} })

            await authApi.logout()

            expect(mockAxiosInstance.post).toHaveBeenCalledWith('/auth/logout')
        })
    })

    describe('refresh', () => {
        it('calls POST /auth/refresh', async () => {
            const mockResponse = {
                data: { expires_in: 900, token_type: 'bearer' },
            }
            mockAxiosInstance.post.mockResolvedValueOnce(mockResponse)

            const result = await authApi.refresh()

            expect(mockAxiosInstance.post).toHaveBeenCalledWith('/auth/refresh')
            expect(result).toEqual(mockResponse.data)
        })
    })

    describe('me', () => {
        it('calls GET /auth/me and returns user', async () => {
            const mockUser = {
                id: '1',
                username: 'testuser',
                email: 'test@example.com',
                is_active: true,
                created_at: '2024-01-01T00:00:00Z',
            }
            mockAxiosInstance.get.mockResolvedValueOnce({ data: mockUser })

            const result = await authApi.me()

            expect(mockAxiosInstance.get).toHaveBeenCalledWith('/auth/me')
            expect(result).toEqual(mockUser)
        })

        it('handles unauthorized error', async () => {
            const error = { response: { status: 401 } }
            mockAxiosInstance.get.mockRejectedValueOnce(error)

            await expect(authApi.me()).rejects.toEqual(error)
        })
    })

    describe('csrf', () => {
        it('adds X-CSRF-Token for state-changing requests when csrf_token cookie exists', () => {
            document.cookie = 'csrf_token=csrf123'
            expect(requestInterceptor).toBeDefined()
            const cfg = requestInterceptor!({ method: 'post', headers: {}, url: '/auth/logout' })
            expect(cfg.headers['X-CSRF-Token']).toBe('csrf123')
        })
    })
})

