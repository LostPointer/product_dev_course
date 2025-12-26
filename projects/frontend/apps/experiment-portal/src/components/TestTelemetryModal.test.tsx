import { describe, it, expect, vi, beforeEach } from 'vitest'
import { render, screen, waitFor } from '@testing-library/react'
import userEvent from '@testing-library/user-event'
import { QueryClient, QueryClientProvider } from '@tanstack/react-query'
import TestTelemetryModal from './TestTelemetryModal'
import { telemetryApi } from '../api/client'

// Мокаем telemetryApi
vi.mock('../api/client', () => ({
    telemetryApi: {
        ingest: vi.fn(),
    },
}))

// Мокаем axios для прямого запроса
vi.mock('axios', async () => {
    const actual = await vi.importActual('axios')
    return {
        ...actual,
        default: {
            post: vi.fn(),
        },
    }
})

const createWrapper = () => {
    const queryClient = new QueryClient({
        defaultOptions: {
            queries: { retry: false },
            mutations: { retry: false },
        },
    })
    return ({ children }: { children: React.ReactNode }) => (
        <QueryClientProvider client={queryClient}>
            {children}
        </QueryClientProvider>
    )
}

describe('TestTelemetryModal', () => {
    const mockOnClose = vi.fn()

    beforeEach(() => {
        vi.clearAllMocks()
        mockOnClose.mockClear()
    })

    it('does not render when isOpen is false', () => {
        render(
            <TestTelemetryModal
                sensorId="sensor-1"
                isOpen={false}
                onClose={mockOnClose}
            />,
            { wrapper: createWrapper() }
        )

        expect(screen.queryByRole('heading', { name: /тестовая отправка телеметрии/i })).not.toBeInTheDocument()
    })

    it('renders modal when isOpen is true', () => {
        render(
            <TestTelemetryModal
                sensorId="sensor-1"
                isOpen={true}
                onClose={mockOnClose}
            />,
            { wrapper: createWrapper() }
        )

        expect(screen.getByRole('heading', { name: /тестовая отправка телеметрии/i })).toBeInTheDocument()
        expect(screen.getByLabelText(/токен датчика/i)).toBeInTheDocument()
        expect(screen.getByLabelText(/run id/i)).toBeInTheDocument()
        expect(screen.getByLabelText(/capture session id/i)).toBeInTheDocument()
        expect(screen.getByLabelText(/meta/i)).toBeInTheDocument()
        expect(screen.getByLabelText(/readings/i)).toBeInTheDocument()
    })

    it('pre-fills token when sensorToken is provided', () => {
        render(
            <TestTelemetryModal
                sensorId="sensor-1"
                sensorToken="test-token-123"
                isOpen={true}
                onClose={mockOnClose}
            />,
            { wrapper: createWrapper() }
        )

        const tokenInput = screen.getByLabelText(/токен датчика/i) as HTMLInputElement
        expect(tokenInput.value).toBe('test-token-123')
        expect(tokenInput).toBeDisabled()
    })

    it('closes modal when close button is clicked', async () => {
        const user = userEvent.setup()
        render(
            <TestTelemetryModal
                sensorId="sensor-1"
                isOpen={true}
                onClose={mockOnClose}
            />,
            { wrapper: createWrapper() }
        )

        const closeButton = screen.getByRole('button', { name: /×/i })
        await user.click(closeButton)

        expect(mockOnClose).toHaveBeenCalledTimes(1)
    })

    it('validates required token field', async () => {
        const user = userEvent.setup()
        render(
            <TestTelemetryModal
                sensorId="sensor-1"
                isOpen={true}
                onClose={mockOnClose}
            />,
            { wrapper: createWrapper() }
        )

        const tokenInput = screen.getByLabelText(/токен датчика/i) as HTMLInputElement
        // Очищаем поле (если оно было заполнено)
        await user.clear(tokenInput)

        const submitButton = screen.getByRole('button', { name: /отправить телеметрию/i })
        await user.click(submitButton)

        // HTML5 validation может сработать, но также должна быть наша проверка
        // Проверяем, что либо HTML5 validation, либо наша ошибка
        await waitFor(() => {
            const hasHtml5Validation = tokenInput.validity.valueMissing
            const hasCustomError = screen.queryByText(/токен датчика обязателен/i)
            expect(hasHtml5Validation || hasCustomError).toBeTruthy()
        })
    })

    it('submits form with correct data', async () => {
        const user = userEvent.setup()
        const mockIngest = vi.mocked(telemetryApi.ingest)
        mockIngest.mockResolvedValueOnce({
            status: 'accepted',
            accepted: 1,
        })

        render(
            <TestTelemetryModal
                sensorId="sensor-1"
                sensorToken="test-token-123"
                isOpen={true}
                onClose={mockOnClose}
            />,
            { wrapper: createWrapper() }
        )

        const readingsTextarea = screen.getByLabelText(/readings/i)
        await user.clear(readingsTextarea)
        await user.paste(JSON.stringify([
            {
                timestamp: '2024-01-01T00:00:00Z',
                raw_value: 25.5,
                meta: { signal: 'test.signal' },
            },
        ]))

        const submitButton = screen.getByRole('button', { name: /отправить телеметрию/i })
        await user.click(submitButton)

        await waitFor(() => {
            expect(mockIngest).toHaveBeenCalledWith(
                {
                    sensor_id: 'sensor-1',
                    readings: [
                        {
                            timestamp: '2024-01-01T00:00:00Z',
                            raw_value: 25.5,
                            meta: { signal: 'test.signal' },
                        },
                    ],
                },
                'test-token-123'
            )
        })
    })

    it('creates default reading when readings are empty', async () => {
        const user = userEvent.setup()
        const mockIngest = vi.mocked(telemetryApi.ingest)
        mockIngest.mockResolvedValueOnce({
            status: 'accepted',
            accepted: 1,
        })

        render(
            <TestTelemetryModal
                sensorId="sensor-1"
                sensorToken="test-token-123"
                isOpen={true}
                onClose={mockOnClose}
            />,
            { wrapper: createWrapper() }
        )

        const submitButton = screen.getByRole('button', { name: /отправить телеметрию/i })
        await user.click(submitButton)

        await waitFor(() => {
            expect(mockIngest).toHaveBeenCalled()
            const call = mockIngest.mock.calls[0]
            const data = call[0]
            expect(data.readings).toHaveLength(1)
            expect(data.readings[0]).toHaveProperty('timestamp')
            expect(data.readings[0]).toHaveProperty('raw_value')
            expect(data.readings[0].meta).toHaveProperty('signal', 'test.signal')
        })
    })

    it('includes optional fields when provided', async () => {
        const user = userEvent.setup()
        const mockIngest = vi.mocked(telemetryApi.ingest)
        mockIngest.mockResolvedValueOnce({
            status: 'accepted',
            accepted: 2,
        })

        render(
            <TestTelemetryModal
                sensorId="sensor-1"
                sensorToken="test-token-123"
                isOpen={true}
                onClose={mockOnClose}
            />,
            { wrapper: createWrapper() }
        )

        await user.type(screen.getByLabelText(/run id/i), 'run-1')
        await user.type(screen.getByLabelText(/capture session id/i), 'session-1')

        const metaTextarea = screen.getByLabelText(/meta/i)
        await user.clear(metaTextarea)
        await user.paste('{"test": true}')

        const readingsTextarea = screen.getByLabelText(/readings/i)
        await user.clear(readingsTextarea)
        await user.paste(JSON.stringify([
            {
                timestamp: '2024-01-01T00:00:00Z',
                raw_value: 25.5,
                meta: { signal: 'test.signal' },
            },
        ]))

        const submitButton = screen.getByRole('button', { name: /отправить телеметрию/i })
        await user.click(submitButton)

        await waitFor(() => {
            expect(mockIngest).toHaveBeenCalledWith(
                {
                    sensor_id: 'sensor-1',
                    run_id: 'run-1',
                    capture_session_id: 'session-1',
                    meta: { test: true },
                    readings: [
                        {
                            timestamp: '2024-01-01T00:00:00Z',
                            raw_value: 25.5,
                            meta: { signal: 'test.signal' },
                        },
                    ],
                },
                'test-token-123'
            )
        })
    })

    it('shows error for invalid JSON in meta', async () => {
        const user = userEvent.setup()
        render(
            <TestTelemetryModal
                sensorId="sensor-1"
                sensorToken="test-token-123"
                isOpen={true}
                onClose={mockOnClose}
            />,
            { wrapper: createWrapper() }
        )

        const metaTextarea = screen.getByLabelText(/meta/i)
        await user.clear(metaTextarea)
        await user.paste('{invalid json}')

        const submitButton = screen.getByRole('button', { name: /отправить телеметрию/i })
        await user.click(submitButton)

        await waitFor(() => {
            expect(screen.getByText(/ошибка в формате json для meta/i)).toBeInTheDocument()
        })

        expect(vi.mocked(telemetryApi.ingest)).not.toHaveBeenCalled()
    })

    it('shows error for invalid JSON in readings', async () => {
        const user = userEvent.setup()
        render(
            <TestTelemetryModal
                sensorId="sensor-1"
                sensorToken="test-token-123"
                isOpen={true}
                onClose={mockOnClose}
            />,
            { wrapper: createWrapper() }
        )

        const readingsTextarea = screen.getByLabelText(/readings/i)
        await user.clear(readingsTextarea)
        await user.paste('{invalid json}')

        const submitButton = screen.getByRole('button', { name: /отправить телеметрию/i })
        await user.click(submitButton)

        await waitFor(() => {
            expect(screen.getByText(/ошибка в формате json для readings/i)).toBeInTheDocument()
        })

        expect(vi.mocked(telemetryApi.ingest)).not.toHaveBeenCalled()
    })

    it('shows error when readings is not an array', async () => {
        const user = userEvent.setup()
        render(
            <TestTelemetryModal
                sensorId="sensor-1"
                sensorToken="test-token-123"
                isOpen={true}
                onClose={mockOnClose}
            />,
            { wrapper: createWrapper() }
        )

        const readingsTextarea = screen.getByLabelText(/readings/i)
        await user.clear(readingsTextarea)
        await user.paste('{"not": "array"}')

        const submitButton = screen.getByRole('button', { name: /отправить телеметрию/i })
        await user.click(submitButton)

        await waitFor(() => {
            expect(screen.getByText(/readings должен быть массивом/i)).toBeInTheDocument()
        })

        expect(vi.mocked(telemetryApi.ingest)).not.toHaveBeenCalled()
    })

    it('shows error on API failure', async () => {
        const user = userEvent.setup()
        const mockIngest = vi.mocked(telemetryApi.ingest)
        mockIngest.mockRejectedValueOnce({
            response: {
                data: { error: 'Invalid token' },
            },
        })

        render(
            <TestTelemetryModal
                sensorId="sensor-1"
                sensorToken="test-token-123"
                isOpen={true}
                onClose={mockOnClose}
            />,
            { wrapper: createWrapper() }
        )

        const submitButton = screen.getByRole('button', { name: /отправить телеметрию/i })
        await user.click(submitButton)

        await waitFor(() => {
            expect(screen.getByText(/invalid token/i)).toBeInTheDocument()
        })
    })

    it('shows success message on successful submission', async () => {
        const user = userEvent.setup()
        const mockIngest = vi.mocked(telemetryApi.ingest)
        mockIngest.mockResolvedValueOnce({
            status: 'accepted',
            accepted: 2,
        })

        render(
            <TestTelemetryModal
                sensorId="sensor-1"
                sensorToken="test-token-123"
                isOpen={true}
                onClose={mockOnClose}
            />,
            { wrapper: createWrapper() }
        )

        const submitButton = screen.getByRole('button', { name: /отправить телеметрию/i })
        await user.click(submitButton)

        await waitFor(() => {
            expect(screen.getByText(/телеметрия успешно отправлена/i)).toBeInTheDocument()
            expect(screen.getByText(/принято записей: 2/i)).toBeInTheDocument()
        })
    })

    it('fills example data when example button is clicked', async () => {
        const user = userEvent.setup()
        render(
            <TestTelemetryModal
                sensorId="sensor-1"
                sensorToken="test-token-123"
                isOpen={true}
                onClose={mockOnClose}
            />,
            { wrapper: createWrapper() }
        )

        const exampleButton = screen.getByRole('button', { name: /заполнить примером/i })
        await user.click(exampleButton)

        const readingsTextarea = screen.getByLabelText(/readings/i) as HTMLTextAreaElement
        const metaTextarea = screen.getByLabelText(/meta/i) as HTMLTextAreaElement

        expect(readingsTextarea.value).toContain('temperature.c')
        expect(readingsTextarea.value).toContain('pressure.hpa')
        expect(metaTextarea.value).toContain('"test": true')
    })
})

