import { useState } from 'react'
import { useQuery } from '@tanstack/react-query'
import { Link } from 'react-router-dom'
import { sensorsApi } from '../api/client'
import { format } from 'date-fns'
import type { Sensor } from '../types'
import StatusBadge from '../components/StatusBadge'
import Loading from '../components/Loading'
import Error from '../components/Error'
import EmptyState from '../components/EmptyState'
import Pagination from '../components/Pagination'
import PageHeader from '../components/PageHeader'
import InfoRow from '../components/InfoRow'
import Filters from '../components/Filters'
import { formatLastHeartbeat } from '../utils/formatLastHeartbeat'
import './SensorsList.css'

function SensorsList() {
    const [projectId, setProjectId] = useState<string>('')
    const [status, setStatus] = useState<string>('')
    const [page, setPage] = useState(1)
    const pageSize = 20

    const { data, isLoading, error } = useQuery({
        queryKey: ['sensors', projectId, status, page],
        queryFn: () =>
            sensorsApi.list({
                project_id: projectId || undefined,
                status: status || undefined,
                page,
                page_size: pageSize,
            }),
    })

    if (isLoading) {
        return <Loading />
    }

    if (error) {
        return <Error message="Ошибка загрузки датчиков" />
    }

    return (
        <div className="sensors-list">
            <PageHeader
                title="Датчики"
                action={
                    <Link to="/sensors/new" className="btn btn-primary">
                        Зарегистрировать датчик
                    </Link>
                }
            />

            <Filters
                fields={[
                    {
                        id: 'sensor_project_id',
                        label: 'Project ID',
                        type: 'text',
                        value: projectId,
                        onChange: (value) => {
                            setProjectId(value)
                            setPage(1)
                        },
                        placeholder: 'UUID проекта',
                    },
                    {
                        id: 'sensor_status',
                        label: 'Статус',
                        type: 'select',
                        value: status,
                        onChange: (value) => {
                            setStatus(value)
                            setPage(1)
                        },
                        options: [
                            { value: '', label: 'Все' },
                            { value: 'registering', label: 'Регистрация' },
                            { value: 'active', label: 'Активен' },
                            { value: 'inactive', label: 'Неактивен' },
                            { value: 'archived', label: 'Архивирован' },
                        ],
                    },
                ]}
                onReset={() => {
                    setProjectId('')
                    setStatus('')
                    setPage(1)
                }}
            />

            {data && (
                <>
                    <div className="sensors-grid">
                        {data.sensors.map((sensor: Sensor) => (
                            <Link
                                key={sensor.id}
                                to={`/sensors/${sensor.id}`}
                                className="sensor-card card"
                            >
                                <div className="card-header">
                                    <h3 className="card-title">{sensor.name}</h3>
                                    <StatusBadge status={sensor.status} variant="sensor" />
                                </div>

                                <div className="sensor-info">
                                    <InfoRow label="Тип" value={sensor.type} />
                                    <InfoRow
                                        label="Единицы"
                                        value={`${sensor.input_unit} → ${sensor.display_unit}`}
                                    />
                                    <InfoRow
                                        label="Последний heartbeat"
                                        value={formatLastHeartbeat(sensor.last_heartbeat)}
                                    />
                                    {sensor.token_preview && (
                                        <InfoRow
                                            label="Токен"
                                            value={`****${sensor.token_preview}`}
                                            mono
                                        />
                                    )}
                                </div>

                                <div className="sensor-meta">
                                    <small>
                                        Создан:{' '}
                                        {format(new Date(sensor.created_at), 'dd MMM yyyy HH:mm')}
                                    </small>
                                </div>
                            </Link>
                        ))}
                    </div>

                    {data.sensors.length === 0 && (
                        <EmptyState message="Датчики не найдены" />
                    )}

                    <Pagination
                        currentPage={page}
                        totalPages={Math.ceil(data.total / pageSize)}
                        onPageChange={setPage}
                    />
                </>
            )}
        </div>
    )
}

export default SensorsList

