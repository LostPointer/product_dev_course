import { ReactNode } from 'react'
import './FormActions.css'

interface FormActionsProps {
  onCancel?: () => void
  cancelLabel?: string
  submitLabel: string
  isSubmitting?: boolean
  submitDisabled?: boolean
  submitButtonType?: 'button' | 'submit'
  additionalActions?: ReactNode
}

function FormActions({
  onCancel,
  cancelLabel = 'Отмена',
  submitLabel,
  isSubmitting = false,
  submitDisabled = false,
  submitButtonType = 'submit',
  additionalActions,
}: FormActionsProps) {
  return (
    <div className="form-actions">
      {onCancel && (
        <button type="button" className="btn btn-secondary" onClick={onCancel}>
          {cancelLabel}
        </button>
      )}
      {additionalActions}
      <button
        type={submitButtonType}
        className="btn btn-primary"
        disabled={isSubmitting || submitDisabled}
      >
        {isSubmitting ? 'Сохранение...' : submitLabel}
      </button>
    </div>
  )
}

export default FormActions

