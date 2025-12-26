import { ReactNode } from 'react'
import './CreateRunModal.css'

interface ModalProps {
    isOpen: boolean
    onClose: () => void
    title: string
    children: ReactNode
    disabled?: boolean
    className?: string
}

function Modal({ isOpen, onClose, title, children, disabled = false, className = '' }: ModalProps) {
    if (!isOpen) {
        return null
    }

    const handleOverlayClick = () => {
        if (!disabled) {
            onClose()
        }
    }

    return (
        <div className="modal-overlay" onClick={handleOverlayClick}>
            <div className={`modal-content ${className}`} onClick={(e) => e.stopPropagation()}>
                <div className="modal-header">
                    <h2>{title}</h2>
                    <button
                        type="button"
                        className="modal-close"
                        onClick={onClose}
                        disabled={disabled}
                    >
                        ×
                    </button>
                </div>
                {children}
            </div>
        </div>
    )
}

export default Modal

