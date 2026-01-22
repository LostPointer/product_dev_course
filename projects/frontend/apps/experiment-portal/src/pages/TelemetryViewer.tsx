import { useEffect, useMemo, useRef, useState } from 'react'
import { createPortal } from 'react-dom'
import { useQuery } from '@tanstack/react-query'
import Plotly from 'plotly.js-dist-min'
import { captureSessionsApi, experimentsApi, projectsApi, runsApi, sensorsApi, telemetryApi } from '../api/client'
import { EmptyState, Error as ErrorComponent, FloatingActionButton, Loading, MaterialSelect } from '../components/common'
import TelemetryPanel from '../components/TelemetryPanel'
import { setActiveProjectId } from '../utils/activeProject'
import { generateUUID } from '../utils/uuid'
import type { Sensor, TelemetryQueryRecord } from '../types'
import './TelemetryViewer.css'

type TelemetryViewerState = {
    projectId: string
    experimentId: string
    runId: string
    filtersOpen: boolean
}

type TelemetryViewMode = 'live' | 'history'
type HistoryValueMode = 'physical' | 'raw'

const HISTORY_MAX_POINTS_DEFAULT = 5000
const HISTORY_PAGE_SIZE = 2000

function TelemetryViewer() {
    const [projectId, setProjectId] = useState<string>('')
    const [experimentId, setExperimentId] = useState<string>('')
    const [runId, setRunId] = useState<string>('')
    const [panelIds, setPanelIds] = useState<string[]>([])
    const [filtersOpen, setFiltersOpen] = useState(true)
    const [viewMode, setViewMode] = useState<TelemetryViewMode>('live')
    const [draggingPanelId, setDraggingPanelId] = useState<string | null>(null)
    const [dragOverPanelId, setDragOverPanelId] = useState<string | null>(null)
    const panelsLoadedRef = useRef(false)
    const viewerStateLoadedRef = useRef(false)
    const [panelSizes, setPanelSizes] = useState<Record<string, { width: number; height: number }>>({})
    const panelsWrapRef = useRef<HTMLDivElement | null>(null)
    const [panelsWrapWidth, setPanelsWrapWidth] = useState(0)
    const historyPlotRef = useRef<HTMLDivElement | null>(null)

    const [historyCaptureSessionId, setHistoryCaptureSessionId] = useState('')
    const [historySensorIds, setHistorySensorIds] = useState<string[]>([])
    const [historyValueMode, setHistoryValueMode] = useState<HistoryValueMode>('physical')
    const [historyIncludeLate, setHistoryIncludeLate] = useState(true)
    const [historyMaxPoints, setHistoryMaxPoints] = useState(HISTORY_MAX_POINTS_DEFAULT)
    const [historyLoading, setHistoryLoading] = useState(false)
    const [historyError, setHistoryError] = useState<string | null>(null)
    const [historyPoints, setHistoryPoints] = useState<TelemetryQueryRecord[]>([])

    useEffect(() => {
        if (typeof window === 'undefined') return
        const raw = window.localStorage.getItem('telemetry_panel_ids')
        if (!raw) {
            panelsLoadedRef.current = true
            return
        }
        try {
            const parsed = JSON.parse(raw)
            if (Array.isArray(parsed)) {
                const next = parsed.filter((id) => typeof id === 'string' && id.trim().length > 0)
                if (next.length > 0) setPanelIds(next)
            }
        } catch {
            // ignore malformed local storage
        } finally {
            panelsLoadedRef.current = true
        }
    }, [])

    useEffect(() => {
        if (typeof window === 'undefined') return
        const raw = window.localStorage.getItem('telemetry_viewer_state')
        if (!raw) {
            viewerStateLoadedRef.current = true
            return
        }
        try {
            const parsed = JSON.parse(raw) as Partial<TelemetryViewerState>
            if (typeof parsed.projectId === 'string') setProjectId(parsed.projectId)
            if (typeof parsed.experimentId === 'string') setExperimentId(parsed.experimentId)
            if (typeof parsed.runId === 'string') setRunId(parsed.runId)
            if (typeof parsed.filtersOpen === 'boolean') setFiltersOpen(parsed.filtersOpen)
        } catch {
            // ignore malformed local storage
        } finally {
            viewerStateLoadedRef.current = true
        }
    }, [])

    useEffect(() => {
        if (typeof window === 'undefined' || !viewerStateLoadedRef.current) return
        const payload: TelemetryViewerState = {
            projectId,
            experimentId,
            runId,
            filtersOpen,
        }
        window.localStorage.setItem('telemetry_viewer_state', JSON.stringify(payload))
    }, [projectId, experimentId, runId, filtersOpen])

    useEffect(() => {
        if (typeof window === 'undefined' || !panelsLoadedRef.current) return
        window.localStorage.setItem('telemetry_panel_ids', JSON.stringify(panelIds))
    }, [panelIds])

    useEffect(() => {
        const element = panelsWrapRef.current
        if (!element || typeof ResizeObserver === 'undefined') return

        const updateSize = () => {
            const rect = element.getBoundingClientRect()
            setPanelsWrapWidth(Math.max(0, Math.round(rect.width)))
        }

        updateSize()
        const observer = new ResizeObserver(updateSize)
        observer.observe(element)

        return () => observer.disconnect()
    }, [])

    const handlePanelSizeChange = (panelId: string, size: { width: number; height: number }) => {
        setPanelSizes((prev) => {
            const existing = prev[panelId]
            if (existing && existing.width === size.width && existing.height === size.height) return prev
            return { ...prev, [panelId]: size }
        })
    }

    const { data: projectsData, isLoading: projectsLoading } = useQuery({
        queryKey: ['projects'],
        queryFn: () => projectsApi.list(),
    })

    useEffect(() => {
        if (!projectsData?.projects?.length) return
        const exists = projectsData.projects.some((project) => project.id === projectId)
        if (!projectId || !exists) {
            const id = projectsData.projects[0].id
            setProjectId(id)
            setExperimentId('')
            setRunId('')
            setActiveProjectId(id)
        }
    }, [projectId, projectsData])

    useEffect(() => {
        if (projectId) setActiveProjectId(projectId)
    }, [projectId])

    const { data: sensorsData, isLoading, error } = useQuery({
        queryKey: ['sensors', projectId],
        queryFn: () => sensorsApi.list({ project_id: projectId }),
        enabled: !!projectId,
    })

    const { data: experimentsData, isLoading: experimentsLoading, error: experimentsError } = useQuery({
        queryKey: ['experiments', projectId],
        queryFn: () => experimentsApi.list({ project_id: projectId, page_size: 100 }),
        enabled: !!projectId,
    })

    const { data: runsData, isLoading: runsLoading, error: runsError } = useQuery({
        queryKey: ['runs', experimentId],
        queryFn: () => runsApi.list(experimentId, { page_size: 100 }),
        enabled: !!experimentId,
    })

    const { data: captureSessionsData, isLoading: captureSessionsLoading, error: captureSessionsError } = useQuery({
        queryKey: ['capture-sessions', runId],
        queryFn: () => captureSessionsApi.list(runId, { page_size: 200 }),
        enabled: !!runId && viewMode === 'history',
    })

    useEffect(() => {
        if (!experimentId) return
        const exists = experimentsData?.experiments?.some((experiment) => experiment.id === experimentId)
        if (!exists) {
            setExperimentId('')
            setRunId('')
        }
    }, [experimentId, experimentsData])

    useEffect(() => {
        if (!runId) return
        const exists = runsData?.runs?.some((run) => run.id === runId)
        if (!exists) setRunId('')
    }, [runId, runsData])

    useEffect(() => {
        if (viewMode !== 'history') return
        const sessions = captureSessionsData?.capture_sessions || []
        const exists = sessions.some((s) => s.id === historyCaptureSessionId)
        if (!historyCaptureSessionId || !exists) {
            setHistoryCaptureSessionId(sessions[0]?.id || '')
        }
    }, [captureSessionsData, historyCaptureSessionId, viewMode])

    const sensors = sensorsData?.sensors || []
    const experiments = experimentsData?.experiments || []
    const runs = runsData?.runs || []
    const captureSessions = captureSessionsData?.capture_sessions || []
    const hasProjects = !!projectsData?.projects?.length
    const isLiveMode = viewMode === 'live'
    const canAddPanel =
        isLiveMode &&
        !!projectId &&
        sensors.length > 0 &&
        !projectsLoading &&
        !experimentsLoading &&
        !runsLoading &&
        !isLoading &&
        !error

    const addPanel = () => {
        setPanelIds((prev) => [...prev, generateUUID()])
    }

    const removePanel = (id: string) => {
        setPanelIds((prev) => prev.filter((panelId) => panelId !== id))
    }

    const movePanel = (sourceId: string, targetId: string) => {
        if (sourceId === targetId) return
        setPanelIds((prev) => {
            const next = [...prev]
            const fromIndex = next.indexOf(sourceId)
            const toIndex = next.indexOf(targetId)
            if (fromIndex === -1 || toIndex === -1) return prev
            next.splice(fromIndex, 1)
            next.splice(toIndex, 0, sourceId)
            return next
        })
    }

    const panelTitleSeed = useMemo(() => {
        if (!projectId) return 'Панель'
        const projectName = projectsData?.projects.find((p) => p.id === projectId)?.name
        return projectName ? `Панель: ${projectName}` : 'Панель'
    }, [projectId, projectsData])

    const availableHistorySensors = useMemo(
        () => sensors.filter((s) => !historySensorIds.includes(s.id)),
        [sensors, historySensorIds]
    )

    const historySensorsById = useMemo(() => {
        const map = new Map<string, Sensor>()
        sensors.forEach((sensor) => map.set(sensor.id, sensor))
        return map
    }, [sensors])

    const historySeriesData = useMemo(() => {
        const bySensor = new Map<string, TelemetryQueryRecord[]>()
        historyPoints.forEach((point) => {
            const list = bySensor.get(point.sensor_id) || []
            list.push(point)
            bySensor.set(point.sensor_id, list)
        })
        return Array.from(bySensor.entries()).map(([sensorId, points]) => {
            const sensor = historySensorsById.get(sensorId)
            const ordered = points.slice().sort((a, b) => a.id - b.id)
            return {
                id: sensorId,
                name: sensor?.name || sensorId,
                x: ordered.map((p) => p.timestamp),
                y: ordered.map((p) =>
                    historyValueMode === 'physical' ? p.physical_value : p.raw_value
                ),
            }
        })
    }, [historyPoints, historySensorsById, historyValueMode])

    const historyHasData = useMemo(
        () => historySeriesData.some((series) => series.y.length > 0),
        [historySeriesData]
    )

    const historyPlotlyData = useMemo(
        () =>
            historySeriesData.map((series, index) => ({
                x: series.x,
                y: series.y,
                type: 'scattergl' as const,
                mode: 'lines' as const,
                name: series.name,
                line: {
                    color: ['#2563eb', '#16a34a', '#f97316', '#a855f7', '#06b6d4', '#e11d48'][index % 6],
                    width: 2,
                },
                hovertemplate: '%{x}<br>%{y:.3f}<extra></extra>',
            })),
        [historySeriesData]
    )

    const historyPlotlyLayout = useMemo(
        () => ({
            autosize: true,
            margin: { l: 42, r: 14, t: 12, b: 24 },
            showlegend: true,
            paper_bgcolor: 'rgba(0,0,0,0)',
            plot_bgcolor: 'rgba(15, 23, 42, 0.04)',
            xaxis: {
                showgrid: true,
                zeroline: false,
                gridcolor: 'rgba(15, 23, 42, 0.12)',
                tickfont: { size: 10, color: '#475569' },
                ticks: 'outside' as const,
                tickcolor: 'rgba(15, 23, 42, 0.12)',
            },
            yaxis: {
                showgrid: true,
                zeroline: false,
                gridcolor: 'rgba(15, 23, 42, 0.12)',
                tickfont: { size: 10, color: '#475569' },
                ticks: 'outside' as const,
                tickcolor: 'rgba(15, 23, 42, 0.12)',
            },
        }),
        []
    )

    const historyPlotlyConfig = useMemo(
        () => ({
            responsive: true,
            displayModeBar: false,
            displaylogo: false,
        }),
        []
    )

    useEffect(() => {
        const element = historyPlotRef.current
        if (!element) return
        const data = historyHasData ? historyPlotlyData : []
        Plotly.react(element, data, historyPlotlyLayout, historyPlotlyConfig)
    }, [historyPlotlyData, historyPlotlyLayout, historyPlotlyConfig, historyHasData])

    useEffect(() => {
        const element = historyPlotRef.current
        if (!element || typeof ResizeObserver === 'undefined') return
        const observer = new ResizeObserver(() => {
            Plotly.Plots.resize(element)
        })
        observer.observe(element)
        return () => observer.disconnect()
    }, [])

    useEffect(() => {
        return () => {
            const element = historyPlotRef.current
            if (element) Plotly.purge(element)
        }
    }, [])

    const addHistorySensor = (sensorId: string) => {
        if (!sensorId) return
        setHistorySensorIds((prev) => (prev.includes(sensorId) ? prev : [...prev, sensorId]))
    }

    const removeHistorySensor = (sensorId: string) => {
        setHistorySensorIds((prev) => prev.filter((id) => id !== sensorId))
    }

    const loadHistory = async () => {
        if (!historyCaptureSessionId) return
        setHistoryError(null)
        setHistoryLoading(true)
        setHistoryPoints([])
        try {
            let sinceId = 0
            const collected: TelemetryQueryRecord[] = []
            while (collected.length < historyMaxPoints) {
                const pageLimit = Math.min(HISTORY_PAGE_SIZE, historyMaxPoints - collected.length)
                const resp = await telemetryApi.query({
                    capture_session_id: historyCaptureSessionId,
                    sensor_id: historySensorIds.length > 0 ? historySensorIds : undefined,
                    since_id: sinceId,
                    limit: pageLimit,
                    include_late: historyIncludeLate,
                })
                collected.push(...resp.points)
                if (!resp.next_since_id || resp.points.length === 0) break
                sinceId = resp.next_since_id
            }
            setHistoryPoints(collected)
        } catch (err: any) {
            setHistoryError(err?.message || 'Ошибка загрузки истории')
        } finally {
            setHistoryLoading(false)
        }
    }

    return (
        <div className="telemetry-view">
            {projectsLoading && <Loading message="Загрузка проектов..." />}

            {!projectsLoading && !hasProjects && (
                <EmptyState message="У вас нет проектов. Создайте проект, чтобы просматривать телеметрию." />
            )}

            {hasProjects && (
                <div className={`telemetry-view__filters card${filtersOpen ? '' : ' telemetry-view__filters--collapsed'}`}>
                    <div
                        className={`telemetry-view__filters-body${filtersOpen ? ' telemetry-view__filters-body--open' : ''
                            }`}
                        aria-hidden={!filtersOpen}
                    >
                        <div className="telemetry-view__grid">
                            <div className="form-group telemetry-view__mode">
                                <label>Режим</label>
                                <div className="telemetry-view__mode-toggle">
                                    <label>
                                        <input
                                            type="radio"
                                            name="telemetry-view-mode"
                                            checked={viewMode === 'live'}
                                            onChange={() => setViewMode('live')}
                                        />
                                        live
                                    </label>
                                    <label>
                                        <input
                                            type="radio"
                                            name="telemetry-view-mode"
                                            checked={viewMode === 'history'}
                                            onChange={() => setViewMode('history')}
                                        />
                                        history
                                    </label>
                                </div>
                            </div>
                            <MaterialSelect
                                id="telemetry_project_id"
                                label="Проект"
                                placeholder="Выберите проект"
                                value={projectId}
                                onChange={(id) => {
                                    setProjectId(id)
                                    setActiveProjectId(id)
                                    setExperimentId('')
                                    setRunId('')
                                }}
                                disabled={projectsLoading}
                            >
                                {projectsData?.projects.map((project) => (
                                    <option key={project.id} value={project.id}>
                                        {project.name}
                                    </option>
                                ))}
                            </MaterialSelect>

                            <MaterialSelect
                                id="telemetry_experiment_id"
                                label="Эксперимент"
                                placeholder="Выберите эксперимент"
                                value={experimentId}
                                onChange={(id) => {
                                    setExperimentId(id)
                                    setRunId('')
                                }}
                                disabled={!projectId || experimentsLoading || projectsLoading}
                            >
                                {experiments.map((experiment) => (
                                    <option key={experiment.id} value={experiment.id}>
                                        {experiment.name}
                                    </option>
                                ))}
                            </MaterialSelect>

                            <MaterialSelect
                                id="telemetry_run_id"
                                label="Пуск"
                                value={runId}
                                onChange={setRunId}
                                placeholder="Выберите пуск"
                                disabled={!experimentId || runsLoading || experimentsLoading || projectsLoading}
                            >
                                {runs.map((run) => (
                                    <option key={run.id} value={run.id}>
                                        {run.name}
                                    </option>
                                ))}
                            </MaterialSelect>

                            {viewMode === 'history' && (
                                <MaterialSelect
                                    id="telemetry_capture_session_id"
                                    label="Capture session"
                                    value={historyCaptureSessionId}
                                    onChange={setHistoryCaptureSessionId}
                                    placeholder="Выберите сессию"
                                    disabled={!runId || captureSessionsLoading}
                                >
                                    {captureSessions.map((session) => (
                                        <option key={session.id} value={session.id}>
                                            #{session.ordinal_number} · {session.status}
                                        </option>
                                    ))}
                                </MaterialSelect>
                            )}
                        </div>

                        {isLoading && <Loading message="Загрузка датчиков..." />}
                        {error && (
                            <ErrorComponent
                                message={
                                    error instanceof Error
                                        ? error.message
                                        : 'Ошибка загрузки датчиков. Убедитесь, что выбран проект.'
                                }
                            />
                        )}
                        {!isLoading && !error && projectId && sensors.length === 0 && (
                            <EmptyState message="В выбранном проекте нет датчиков." />
                        )}
                        {experimentsLoading && <Loading message="Загрузка экспериментов..." />}
                        {!experimentsLoading && experimentsError && (
                            <ErrorComponent
                                message={
                                    experimentsError instanceof Error
                                        ? experimentsError.message
                                        : 'Ошибка загрузки экспериментов.'
                                }
                            />
                        )}
                        {runsLoading && <Loading message="Загрузка запусков..." />}
                        {!runsLoading && runsError && (
                            <ErrorComponent
                                message={runsError instanceof Error ? runsError.message : 'Ошибка загрузки запусков.'}
                            />
                        )}
                        {viewMode === 'history' && captureSessionsLoading && <Loading message="Загрузка сессий..." />}
                        {viewMode === 'history' && captureSessionsError && (
                            <ErrorComponent
                                message={
                                    captureSessionsError instanceof Error
                                        ? captureSessionsError.message
                                        : 'Ошибка загрузки сессий.'
                                }
                            />
                        )}
                    </div>


                    <button
                        type="button"
                        className="telemetry-view__collapse"
                        onClick={() => setFiltersOpen((prev) => !prev)}
                        aria-label={filtersOpen ? 'Свернуть фильтры' : 'Развернуть фильтры'}
                    >
                        {filtersOpen ? '︿' : '﹀'}
                    </button>

                </div>
            )}

            {canAddPanel && panelIds.length === 0 && (
                <EmptyState message="Добавьте панель, чтобы начать просмотр графиков." />
            )}

            {isLiveMode ? (
                <div className="telemetry-view__panels" ref={panelsWrapRef}>
                    {panelIds.map((panelId, index) => {
                        const panelSize = panelSizes[panelId]
                        const isWide = panelsWrapWidth > 0 && panelSize ? panelSize.width > panelsWrapWidth / 2 : false
                        return (
                            <div
                                key={panelId}
                                className={`telemetry-view__panel-item${draggingPanelId === panelId ? ' telemetry-view__panel-item--dragging' : ''
                                    }${dragOverPanelId === panelId ? ' telemetry-view__panel-item--over' : ''}${isWide ? ' telemetry-view__panel-item--full' : ''
                                    }`}
                                onDragOver={(event) => {
                                    if (!draggingPanelId || draggingPanelId === panelId) return
                                    event.preventDefault()
                                    event.dataTransfer.dropEffect = 'move'
                                    setDragOverPanelId(panelId)
                                }}
                                onDragLeave={(event) => {
                                    if (event.currentTarget.contains(event.relatedTarget as Node)) return
                                    setDragOverPanelId((prev) => (prev === panelId ? null : prev))
                                }}
                                onDrop={(event) => {
                                    event.preventDefault()
                                    if (draggingPanelId) movePanel(draggingPanelId, panelId)
                                    setDragOverPanelId(null)
                                    setDraggingPanelId(null)
                                }}
                            >
                                <TelemetryPanel
                                    panelId={panelId}
                                    sensors={sensors}
                                    title={`${panelTitleSeed} #${index + 1}`}
                                    onRemove={() => removePanel(panelId)}
                                    onSizeChange={(size) => handlePanelSizeChange(panelId, size)}
                                    dragHandleProps={{
                                        draggable: true,
                                        onDragStart: (event) => {
                                            setDraggingPanelId(panelId)
                                            event.dataTransfer.effectAllowed = 'move'
                                            event.dataTransfer.setData('text/plain', panelId)
                                        },
                                        onDragEnd: () => {
                                            setDraggingPanelId(null)
                                            setDragOverPanelId(null)
                                        },
                                    }}
                                />
                            </div>
                        )
                    })}
                </div>
            ) : (
                <div className="telemetry-view__history card">
                    <div className="telemetry-view__history-controls">
                        <div className="telemetry-view__history-sensors">
                            <MaterialSelect
                                id="telemetry_history_sensors"
                                value=""
                                label="Сенсоры"
                                onChange={(value, event) => {
                                    addHistorySensor(value)
                                    if (event?.currentTarget) {
                                        event.currentTarget.value = ''
                                    }
                                }}
                                disabled={availableHistorySensors.length === 0}
                            >
                                <option value="">Добавить сенсор</option>
                                {availableHistorySensors.map((sensor) => (
                                    <option key={sensor.id} value={sensor.id}>
                                        {sensor.name} ({sensor.type})
                                    </option>
                                ))}
                            </MaterialSelect>
                            <div className="telemetry-view__sensor-list">
                                {historySensorIds.length === 0 && (
                                    <span className="telemetry-view__hint">Сенсоры не выбраны</span>
                                )}
                                {historySensorIds.map((id) => {
                                    const sensor = historySensorsById.get(id)
                                    return (
                                        <span key={id} className="telemetry-view__sensor-pill">
                                            {sensor?.name || id}
                                            <button type="button" onClick={() => removeHistorySensor(id)} aria-label="Удалить сенсор">
                                                ×
                                            </button>
                                        </span>
                                    )
                                })}
                            </div>
                        </div>

                        <div className="telemetry-view__history-options">
                            <label>
                                <span>max points</span>
                                <input
                                    type="number"
                                    min={100}
                                    max={20000}
                                    value={historyMaxPoints}
                                    onChange={(e) => setHistoryMaxPoints(Number(e.target.value || HISTORY_MAX_POINTS_DEFAULT))}
                                />
                            </label>
                            <label className="telemetry-view__checkbox">
                                <input
                                    type="checkbox"
                                    checked={historyIncludeLate}
                                    onChange={(e) => setHistoryIncludeLate(e.target.checked)}
                                />
                                include late
                            </label>
                            <div className="telemetry-view__mode-toggle">
                                <label>
                                    <input
                                        type="radio"
                                        name="history-value-mode"
                                        checked={historyValueMode === 'physical'}
                                        onChange={() => setHistoryValueMode('physical')}
                                    />
                                    physical
                                </label>
                                <label>
                                    <input
                                        type="radio"
                                        name="history-value-mode"
                                        checked={historyValueMode === 'raw'}
                                        onChange={() => setHistoryValueMode('raw')}
                                    />
                                    raw
                                </label>
                            </div>
                            <button
                                type="button"
                                className="btn btn-primary btn-sm"
                                onClick={loadHistory}
                                disabled={historyLoading || !historyCaptureSessionId}
                            >
                                {historyLoading ? 'Загрузка...' : 'Загрузить'}
                            </button>
                        </div>
                    </div>

                    {historyError && <div className="telemetry-view__error">{historyError}</div>}

                    <div className="telemetry-view__history-chart">
                        <div ref={historyPlotRef} className="telemetry-view__plotly" />
                        {!historyHasData && !historyLoading && (
                            <div className="telemetry-view__empty">Нет данных — нажмите «Загрузить»</div>
                        )}
                    </div>
                </div>
            )}

            {isLiveMode && typeof document !== 'undefined' &&
                createPortal(
                    <FloatingActionButton
                        onClick={addPanel}
                        title="Добавить панель"
                        ariaLabel="Добавить панель"
                        disabled={!canAddPanel}
                    />,
                    document.body
                )}
        </div>
    )
}

export default TelemetryViewer
