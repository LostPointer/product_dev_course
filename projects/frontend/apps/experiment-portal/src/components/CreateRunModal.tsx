import { useState } from 'react'
import { useMutation, useQueryClient } from '@tanstack/react-query'
import { useNavigate } from 'react-router-dom'
import { runsApi } from '../api/client'
import type { RunCreate } from '../types'
import Modal from './Modal'
import './CreateRunModal.css'

interface CreateRunModalProps {
    experimentId: string
    isOpen: boolean
    onClose: () => void
}

function CreateRunModal({ experimentId, isOpen, onClose }: CreateRunModalProps) {
    const navigate = useNavigate()
    const queryClient = useQueryClient()
    const [formData, setFormData] = useState<RunCreate>({
        name: '',
        params: {},
        notes: '',
        metadata: {},
    })
    const [parametersJson, setParametersJson] = useState('{}')
    const [metadataJson, setMetadataJson] = useState('{}')
    const [jsonError, setJsonError] = useState<string | null>(null)
    const [error, setError] = useState<string | null>(null)

    const createMutation = useMutation({
        mutationFn: (data: RunCreate) => runsApi.create(experimentId, data),
        onSuccess: (run) => {
            queryClient.invalidateQueries({ queryKey: ['runs', experimentId] })
            queryClient.invalidateQueries({ queryKey: ['experiment', experimentId] })
            onClose()
            navigate(`/runs/${run.id}`)
        },
        onError: (err: any) => {
            setError(err.response?.data?.error || 'Ошибка создания запуска')
        },
    })

    const handleSubmit = (e: React.FormEvent) => {
        e.preventDefault()
        setError(null)
        setJsonError(null)

        // Парсим JSON для params
        let params: Record<string, any> = {}
        if (parametersJson.trim()) {
            try {
                params = JSON.parse(parametersJson)
            } catch (err) {
                setJsonError('Ошибка в формате JSON для параметров')
                return
            }
        }

        // Парсим JSON для metadata
        let metadata: Record<string, any> = {}
        if (metadataJson.trim()) {
            try {
                metadata = JSON.parse(metadataJson)
            } catch (err) {
                setJsonError('Ошибка в формате JSON для метаданных')
                return
            }
        }

        createMutation.mutate({
            name: formData.name,
            params,
            notes: formData.notes || undefined,
            metadata: Object.keys(metadata).length > 0 ? metadata : undefined,
        })
    }

    const handleClose = () => {
        if (!createMutation.isPending) {
            setFormData({
                name: '',
                params: {},
                notes: '',
                metadata: {},
            })
            setParametersJson('{}')
            setMetadataJson('{}')
            setError(null)
            setJsonError(null)
            onClose()
        }
    }

    return (
        <Modal
            isOpen={isOpen}
            onClose={handleClose}
            title="Создать новый запуск"
            disabled={createMutation.isPending}
        >
            <form onSubmit={handleSubmit} className="modal-form">
                {error && <div className="error">{error}</div>}
                {jsonError && <div className="error">{jsonError}</div>}

                <div className="form-group">
                    <label htmlFor="run_name">
                        Название <span className="required">*</span>
                    </label>
                    <input
                        id="run_name"
                        type="text"
                        value={formData.name}
                        onChange={(e) => setFormData({ ...formData, name: e.target.value })}
                        required
                        placeholder="Например: Запуск #1"
                        disabled={createMutation.isPending}
                    />
                </div>

                <div className="form-group">
                    <label htmlFor="run_parameters">Параметры (JSON)</label>
                    <textarea
                        id="run_parameters"
                        value={parametersJson}
                        onChange={(e) => setParametersJson(e.target.value)}
                        placeholder='{"key": "value"}'
                        rows={6}
                        disabled={createMutation.isPending}
                    />
                    <small className="form-hint">
                        JSON объект с параметрами запуска. Оставьте пустым или используйте {'{}'} для пустого объекта.
                    </small>
                </div>

                <div className="form-group">
                    <label htmlFor="run_notes">Заметки</label>
                    <textarea
                        id="run_notes"
                        value={formData.notes}
                        onChange={(e) => setFormData({ ...formData, notes: e.target.value })}
                        placeholder="Дополнительная информация о запуске..."
                        rows={3}
                        disabled={createMutation.isPending}
                    />
                </div>

                <div className="form-group">
                    <label htmlFor="run_metadata">Метаданные (JSON)</label>
                    <textarea
                        id="run_metadata"
                        value={metadataJson}
                        onChange={(e) => setMetadataJson(e.target.value)}
                        placeholder='{"key": "value"}'
                        rows={6}
                        disabled={createMutation.isPending}
                    />
                    <small className="form-hint">
                        JSON объект с метаданными. Оставьте пустым или используйте {'{}'} для пустого объекта.
                    </small>
                </div>

                <div className="modal-actions">
                    <button
                        type="button"
                        className="btn btn-secondary"
                        onClick={handleClose}
                        disabled={createMutation.isPending}
                    >
                        Отмена
                    </button>
                    <button
                        type="submit"
                        className="btn btn-primary"
                        disabled={createMutation.isPending}
                    >
                        {createMutation.isPending ? 'Создание...' : 'Создать запуск'}
                    </button>
                </div>
            </form>
        </Modal>
    )
}

export default CreateRunModal

