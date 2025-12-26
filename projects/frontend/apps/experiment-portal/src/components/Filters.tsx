import { ReactNode } from 'react'
import './Filters.css'

interface FilterField {
  id: string
  label: string
  type: 'text' | 'select'
  value: string
  onChange: (value: string) => void
  placeholder?: string
  options?: Array<{ value: string; label: string }>
}

interface FiltersProps {
  fields: FilterField[]
  onReset?: () => void
  resetLabel?: string
}

function Filters({ fields, onReset, resetLabel = 'Сбросить' }: FiltersProps) {
  return (
    <div className="filters card">
      <div className="filters-grid">
        {fields.map((field) => (
          <div key={field.id} className="form-group">
            <label htmlFor={field.id}>{field.label}</label>
            {field.type === 'text' ? (
              <input
                id={field.id}
                type="text"
                placeholder={field.placeholder}
                value={field.value}
                onChange={(e) => field.onChange(e.target.value)}
              />
            ) : (
              <select
                id={field.id}
                value={field.value}
                onChange={(e) => field.onChange(e.target.value)}
              >
                {field.options?.map((option) => (
                  <option key={option.value} value={option.value}>
                    {option.label}
                  </option>
                ))}
              </select>
            )}
          </div>
        ))}
        {onReset && (
          <div className="form-group">
            <label>&nbsp;</label>
            <button
              type="button"
              className="btn btn-secondary"
              onClick={onReset}
            >
              {resetLabel}
            </button>
          </div>
        )}
      </div>
    </div>
  )
}

export default Filters

