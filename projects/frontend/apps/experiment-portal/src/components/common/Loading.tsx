import './Loading.scss'

interface LoadingProps {
    message?: string
    /** Показать спиннер над текстом (по умолчанию true) */
    showSpinner?: boolean
}

function Loading({ message = 'Загрузка...', showSpinner = true }: LoadingProps) {
    return (
        <div className="loading" role="status" aria-live="polite">
            {showSpinner && <div className="loading__spinner" aria-hidden />}
            <span className="loading__message">{message}</span>
        </div>
    )
}

export default Loading

