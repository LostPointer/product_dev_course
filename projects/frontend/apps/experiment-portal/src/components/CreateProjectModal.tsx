import { useState } from 'react'
import { useNavigate } from 'react-router-dom'
import { useMutation, useQueryClient } from '@tanstack/react-query'
import { projectsApi } from '../api/client'
import type { ProjectCreate } from '../types'
import Modal from './Modal'
import './CreateRunModal.css'

interface CreateProjectModalProps {
    isOpen: boolean
    onClose: () => void
}

function CreateProjectModal({ isOpen, onClose }: CreateProjectModalProps) {
    const navigate = useNavigate()
    const queryClient = useQueryClient()
    const [formData, setFormData] = useState<ProjectCreate>({
        name: '',
        description: '',
    })
    const [error, setError] = useState<string | null>(null)

    const createMutation = useMutation({
        mutationFn: async (data: ProjectCreate) => {
            console.log('CreateProjectModal: Starting create...', data)
            try {
                const result = await projectsApi.create(data)
                console.log('CreateProjectModal: Create success:', result)
                return result
            } catch (err) {
                console.error('CreateProjectModal: Create error:', err)
                throw err
            }
        },
        onSuccess: (project) => {
            console.log('CreateProjectModal: onSuccess called', project)
            queryClient.invalidateQueries({ queryKey: ['projects'] })
            onClose()
            // Переходим на список проектов после создания
            navigate('/projects')
        },
        onError: (err: any) => {
            console.error('CreateProjectModal: onError called', err)
            const errorMessage = err.response?.data?.error || err.response?.data?.message || err.message || 'Ошибка создания проекта'
            setError(errorMessage)
        },
    })

    const handleSubmit = (e: React.FormEvent) => {
        e.preventDefault()
        setError(null)

        createMutation.mutate({
            name: formData.name,
            description: formData.description || undefined,
        })
    }

    const handleClose = () => {
        if (!createMutation.isPending) {
            setFormData({
                name: '',
                description: '',
            })
            setError(null)
            onClose()
        }
    }

    return (
        <Modal
            isOpen={isOpen}
            onClose={handleClose}
            title="Создать проект"
            disabled={createMutation.isPending}
        >
            <form onSubmit={handleSubmit} className="modal-form">
                {error && <div className="error">{error}</div>}

                <div className="form-group">
                    <label htmlFor="project_name">
                        Название <span className="required">*</span>
                    </label>
                    <input
                        id="project_name"
                        type="text"
                        value={formData.name}
                        onChange={(e) => setFormData({ ...formData, name: e.target.value })}
                        required
                        placeholder="Например: Аэродинамические испытания"
                        disabled={createMutation.isPending}
                    />
                </div>

                <div className="form-group">
                    <label htmlFor="project_description">Описание</label>
                    <textarea
                        id="project_description"
                        value={formData.description}
                        onChange={(e) =>
                            setFormData({ ...formData, description: e.target.value })
                        }
                        placeholder="Описание проекта..."
                        rows={4}
                        disabled={createMutation.isPending}
                    />
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
                        {createMutation.isPending ? 'Создание...' : 'Создать проект'}
                    </button>
                </div>
            </form>
        </Modal>
    )
}

export default CreateProjectModal

