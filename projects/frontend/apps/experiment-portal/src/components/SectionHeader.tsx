import { ReactNode } from 'react'
import './SectionHeader.css'

interface SectionHeaderProps {
  title: string
  action?: ReactNode
}

function SectionHeader({ title, action }: SectionHeaderProps) {
  return (
    <div className="section-header">
      <h3>{title}</h3>
      {action && <div className="section-header-action">{action}</div>}
    </div>
  )
}

export default SectionHeader

