import { useEffect, useMemo, useRef, useState } from 'react'
import { createPortal } from 'react-dom'
import { useQuery } from '@tanstack/react-query'
import { experimentsApi, projectsApi, runsApi, sensorsApi } from '../api/client'
import { EmptyState, Error as ErrorComponent, FloatingActionButton, Loading, MaterialSelect } from '../components/common'
import TelemetryPanel from '../components/TelemetryPanel'
import { setActiveProjectId } from '../utils/activeProject'
import { generateUUID } from '../utils/uuid'
import './TelemetryViewer.css'

type TelemetryViewerState = {
    projectId: string
    experimentId: string
    runId: string
    filtersOpen: boolean
}

function TelemetryViewer() {
    const [projectId, setProjectId] = useState<string>('')
    const [experimentId, setExperimentId] = useState<string>('')
    const [runId, setRunId] = useState<string>('')
    const [panelIds, setPanelIds] = useState<string[]>([])
    const [filtersOpen, setFiltersOpen] = useState(true)
    const [draggingPanelId, setDraggingPanelId] = useState<string | null>(null)
    const [dragOverPanelId, setDragOverPanelId] = useState<string | null>(null)
    const panelsLoadedRef = useRef(false)
    const viewerStateLoadedRef = useRef(false)
    const [panelSizes, setPanelSizes] = useState<Record<string, { width: number; height: number }>>({})
    const panelsWrapRef = useRef<HTMLDivElement | null>(null)
    const [panelsWrapWidth, setPanelsWrapWidth] = useState(0)

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

    const sensors = sensorsData?.sensors || []
    const experiments = experimentsData?.experiments || []
    const runs = runsData?.runs || []
    const hasProjects = !!projectsData?.projects?.length
    const canAddPanel =
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

            {typeof document !== 'undefined' &&
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
