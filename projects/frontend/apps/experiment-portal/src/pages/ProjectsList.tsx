import { useState } from 'react'
import { createPortal } from 'react-dom'
import { useQuery } from '@tanstack/react-query'
import { Link } from 'react-router-dom'
import { projectsApi } from '../api/client'
import { Loading, Error, EmptyState, PageHeader } from '../components/common'
import CreateProjectModal from '../components/CreateProjectModal'
import './ProjectsList.css'

function ProjectsList() {
    const [isCreateModalOpen, setIsCreateModalOpen] = useState(false)
    const { data, isLoading, error } = useQuery({
        queryKey: ['projects'],
        queryFn: () => projectsApi.list(),
    })

    return (
        <div className="projects-list">
            {isLoading && <Loading />}
            {error && <Error message="Ошибка загрузки проектов" />}

            {!isLoading && !error && (
                <>
                    <PageHeader
                        title="Проекты"
                        action={
                            <button
                                className="btn btn-primary"
                                onClick={() => setIsCreateModalOpen(true)}
                            >
                                Создать проект
                            </button>
                        }
                    />

                    {data && data.projects.length === 0 ? (
                        <EmptyState message="У вас пока нет проектов">
                            <button
                                className="btn btn-primary"
                                onClick={() => setIsCreateModalOpen(true)}
                            >
                                Создать первый проект
                            </button>
                        </EmptyState>
                    ) : (
                        <div className="projects-grid">
                            {data?.projects.map((project) => (
                                <Link
                                    key={project.id}
                                    to={`/projects/${project.id}`}
                                    className="project-card card"
                                >
                                    <div className="project-card-header">
                                        <h3>{project.name}</h3>
                                    </div>
                                    {project.description && (
                                        <p className="project-description">{project.description}</p>
                                    )}
                                    <div className="project-card-footer">
                                        <span className="project-meta">
                                            Создан: {new Date(project.created_at).toLocaleDateString('ru-RU')}
                                        </span>
                                    </div>
                                </Link>
                            ))}
                        </div>
                    )}
                </>
            )}

            <CreateProjectModal
                isOpen={isCreateModalOpen}
                onClose={() => setIsCreateModalOpen(false)}
            />

            {typeof document !== 'undefined' &&
                createPortal(
                    <button
                        className="fab"
                        onClick={() => setIsCreateModalOpen(true)}
                        title="Создать проект"
                        aria-label="Создать проект"
                    >
                        +
                    </button>,
                    document.body
                )}
        </div>
    )
}

export default ProjectsList

