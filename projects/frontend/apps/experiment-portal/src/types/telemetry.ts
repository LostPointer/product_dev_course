export type TelemetryStreamRecord = {
    id: number
    sensor_id: string
    project_id: string
    timestamp: string
    raw_value: number
    physical_value: number | null
    run_id: string | null
    capture_session_id: string | null
    meta: Record<string, any>
}

export type TelemetryQueryRecord = {
    id: number
    sensor_id: string
    project_id: string
    timestamp: string
    raw_value: number
    physical_value: number | null
    run_id: string | null
    capture_session_id: string | null
    meta: Record<string, any>
}
