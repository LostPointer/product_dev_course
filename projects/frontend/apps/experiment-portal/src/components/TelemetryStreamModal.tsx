import { useEffect, useMemo, useRef, useState } from 'react'
import Modal from './Modal'
import { telemetryApi } from '../api/client'
import { createSSEParser } from '../utils/sse'
import type { TelemetryStreamRecord } from '../types'
import './TelemetryStreamModal.css'

type TelemetryStreamModalProps = {
    sensorId: string
    sensorToken?: string | null
    isOpen: boolean
    onClose: () => void
}

type StreamStatus = 'idle' | 'connecting' | 'streaming' | 'error'

function _clamp(n: number, min: number, max: number) {
    return Math.max(min, Math.min(max, n))
}

function _buildSparklinePath(values: number[], width: number, height: number): string {
    if (values.length === 0) return ''
    if (values.length === 1) return `M 0 ${height / 2} L ${width} ${height / 2}`

    const min = Math.min(...values)
    const max = Math.max(...values)
    const span = max - min || 1

    const stepX = width / (values.length - 1)
    const points = values.map((v, i) => {
        const x = i * stepX
        const y = height - ((v - min) / span) * height
        return [x, y] as const
    })

    const [x0, y0] = points[0]
    const rest = points.slice(1).map(([x, y]) => `L ${x.toFixed(2)} ${y.toFixed(2)}`).join(' ')
    return `M ${x0.toFixed(2)} ${y0.toFixed(2)} ${rest}`
}

export default function TelemetryStreamModal({ sensorId, sensorToken, isOpen, onClose }: TelemetryStreamModalProps) {
    const [token, setToken] = useState(sensorToken || '')
    const [sinceId, setSinceId] = useState('0')
    const [idleTimeoutSeconds, setIdleTimeoutSeconds] = useState('30')
    const [maxPoints, setMaxPoints] = useState('200')
    const [valueMode, setValueMode] = useState<'physical' | 'raw'>('physical')

    const [status, setStatus] = useState<StreamStatus>('idle')
    const [error, setError] = useState<string | null>(null)
    const [records, setRecords] = useState<TelemetryStreamRecord[]>([])
    const [lastId, setLastId] = useState<number>(0)

    const abortRef = useRef<AbortController | null>(null)
    const runningRef = useRef(false)

    useEffect(() => {
        if (!isOpen) {
            // ensure stopped when modal closes
            abortRef.current?.abort()
            abortRef.current = null
            runningRef.current = false
            setStatus('idle')
            setError(null)
            setRecords([])
            setLastId(0)
            setSinceId('0')
            setIdleTimeoutSeconds('30')
            setMaxPoints('200')
            setValueMode('physical')
            setToken(sensorToken || '')
        }
        // eslint-disable-next-line react-hooks/exhaustive-deps
    }, [isOpen])

    useEffect(() => {
        if (sensorToken) setToken(sensorToken)
    }, [sensorToken])

    const plottedValues = useMemo(() => {
        const vals = records
            .map((r) => (valueMode === 'physical' ? r.physical_value : r.raw_value))
            .map((v) => (typeof v === 'number' ? v : null))
            .filter((v): v is number => v !== null && Number.isFinite(v))
        return vals.slice(-50)
    }, [records, valueMode])

    const sparkPath = useMemo(() => _buildSparklinePath(plottedValues, 420, 80), [plottedValues])

    const stop = () => {
        abortRef.current?.abort()
        abortRef.current = null
        runningRef.current = false
        setStatus('idle')
    }

    const start = async () => {
        setError(null)
        if (!token.trim()) {
            setError('Токен датчика обязателен')
            return
        }

        const since = _clamp(Number(sinceId || '0'), 0, Number.MAX_SAFE_INTEGER)
        const idle = _clamp(Number(idleTimeoutSeconds || '30'), 1, 600)
        const max = _clamp(Number(maxPoints || '200'), 10, 2000)

        setRecords([])
        setLastId(since)
        setStatus('connecting')

        const abort = new AbortController()
        abortRef.current = abort
        runningRef.current = true

        try {
            const resp = await telemetryApi.stream(
                { sensor_id: sensorId, since_id: since, idle_timeout_seconds: idle },
                token.trim()
            )
            if (!resp.ok) {
                const text = await resp.text().catch(() => '')
                throw new Error(text || `Ошибка стрима: HTTP ${resp.status}`)
            }
            if (!resp.body) throw new Error('Stream body is empty')

            setStatus('streaming')

            const reader = resp.body.getReader()
            const decoder = new TextDecoder()
            const parser = createSSEParser((evt) => {
                if (evt.event === 'telemetry') {
                    try {
                        const parsed = JSON.parse(evt.data) as TelemetryStreamRecord
                        setLastId(parsed.id)
                        setRecords((prev) => {
                            const next = [...prev, parsed]
                            return next.length > max ? next.slice(next.length - max) : next
                        })
                    } catch (e: any) {
                        setError(e?.message || 'Ошибка парсинга telemetry event')
                        setStatus('error')
                    }
                }
                if (evt.event === 'error') {
                    setError(evt.data || 'Ошибка стрима')
                    setStatus('error')
                }
            })

            while (runningRef.current && !abort.signal.aborted) {
                const { value, done } = await reader.read()
                if (done) break
                if (value) parser.feed(decoder.decode(value, { stream: true }))
            }
        } catch (e: any) {
            if (e?.name === 'AbortError') {
                return
            }
            setError(e?.message || 'Ошибка подключения к стриму')
            setStatus('error')
        } finally {
            runningRef.current = false
        }
    }

    const canClose = status !== 'connecting'

    return (
        <Modal
            isOpen={isOpen}
            onClose={() => {
                if (!canClose) return
                stop()
                onClose()
            }}
            title="Live telemetry (SSE)"
            disabled={!canClose}
            className="telemetry-stream-modal"
        >
            <div className="telemetry-stream">
                {error && <div className="error">{error}</div>}

                <div className="form-grid">
                    <div className="form-group">
                        <label htmlFor="telemetry_stream_token">
                            Токен датчика <span className="required">*</span>
                        </label>
                        <input
                            id="telemetry_stream_token"
                            type="text"
                            value={token}
                            onChange={(e) => setToken(e.target.value)}
                            placeholder="Bearer token"
                            disabled={status === 'connecting' || status === 'streaming' || !!sensorToken}
                        />
                        {sensorToken && <small className="form-hint">Используется токен из ротации/регистрации</small>}
                    </div>

                    <div className="form-group">
                        <label htmlFor="telemetry_stream_since">since_id</label>
                        <input
                            id="telemetry_stream_since"
                            type="number"
                            value={sinceId}
                            onChange={(e) => setSinceId(e.target.value)}
                            disabled={status === 'connecting' || status === 'streaming'}
                            min={0}
                        />
                    </div>

                    <div className="form-group">
                        <label htmlFor="telemetry_stream_idle">idle_timeout_seconds</label>
                        <input
                            id="telemetry_stream_idle"
                            type="number"
                            value={idleTimeoutSeconds}
                            onChange={(e) => setIdleTimeoutSeconds(e.target.value)}
                            disabled={status === 'connecting' || status === 'streaming'}
                            min={1}
                            max={600}
                        />
                    </div>

                    <div className="form-group">
                        <label htmlFor="telemetry_stream_max">max points (UI)</label>
                        <input
                            id="telemetry_stream_max"
                            type="number"
                            value={maxPoints}
                            onChange={(e) => setMaxPoints(e.target.value)}
                            disabled={status === 'connecting' || status === 'streaming'}
                            min={10}
                            max={2000}
                        />
                    </div>
                </div>

                <div className="stream-actions">
                    {status !== 'streaming' ? (
                        <button className="btn btn-primary" onClick={start} disabled={status === 'connecting'}>
                            {status === 'connecting' ? 'Подключение…' : 'Старт'}
                        </button>
                    ) : (
                        <button className="btn btn-secondary" onClick={stop}>
                            Стоп
                        </button>
                    )}
                    <div className="stream-meta">
                        <span className="mono">sensor_id: {sensorId}</span>
                        <span className="mono">last_id: {lastId}</span>
                        <span>events: {records.length}</span>
                    </div>
                </div>

                <div className="sparkline-card">
                    <div className="sparkline-header">
                        <strong>График (последние 50)</strong>
                        <div className="mode-toggle">
                            <label>
                                <input
                                    type="radio"
                                    name="telemetry_value_mode"
                                    checked={valueMode === 'physical'}
                                    onChange={() => setValueMode('physical')}
                                />
                                physical
                            </label>
                            <label>
                                <input
                                    type="radio"
                                    name="telemetry_value_mode"
                                    checked={valueMode === 'raw'}
                                    onChange={() => setValueMode('raw')}
                                />
                                raw
                            </label>
                        </div>
                    </div>
                    <svg width="100%" height="90" viewBox="0 0 420 90" role="img" aria-label="telemetry sparkline">
                        <rect x="0" y="0" width="420" height="90" className="sparkline-bg" />
                        <path d={sparkPath} className="sparkline-path" />
                    </svg>
                </div>

                <div className="records">
                    <div className="records-header">
                        <strong>Последние события</strong>
                    </div>
                    {records.length === 0 ? (
                        <div className="empty">Событий пока нет</div>
                    ) : (
                        <div className="records-list">
                            {records
                                .slice()
                                .reverse()
                                .slice(0, 25)
                                .map((r) => (
                                    <div key={r.id} className="record-row">
                                        <span className="mono">#{r.id}</span>
                                        <span className="mono">{new Date(r.timestamp).toLocaleString()}</span>
                                        <span className="mono">
                                            raw={r.raw_value}
                                            {typeof r.physical_value === 'number' ? ` phys=${r.physical_value}` : ''}
                                        </span>
                                        {r.run_id && <span className="mono">run={r.run_id}</span>}
                                        {r.capture_session_id && <span className="mono">cs={r.capture_session_id}</span>}
                                    </div>
                                ))}
                        </div>
                    )}
                </div>
            </div>
        </Modal>
    )
}

