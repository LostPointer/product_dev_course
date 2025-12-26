import { ReactNode } from 'react'
import './FormGroup.css'

interface FormGroupProps {
  label: string | ReactNode
  htmlFor?: string
  required?: boolean
  hint?: string
  children: ReactNode
  error?: string
}

function FormGroup({
  label,
  htmlFor,
  required = false,
  hint,
  children,
  error,
}: FormGroupProps) {
  return (
    <div className="form-group">
      <label htmlFor={htmlFor}>
        {label}
        {required && <span className="required">*</span>}
      </label>
      {children}
      {hint && <small className="form-hint">{hint}</small>}
      {error && <div className="form-error">{error}</div>}
    </div>
  )
}

export default FormGroup

