import { ReactNode } from 'react'
import { usePermissions } from '../hooks/usePermissions'

interface PermissionGateProps {
  permission?: string
  anyOf?: string[]
  fallback?: ReactNode
  children: ReactNode
}

export function PermissionGate({
  permission,
  anyOf,
  fallback = null,
  children,
}: PermissionGateProps) {
  const { hasPermission, hasAnyPermission } = usePermissions()

  let allowed = false
  if (permission) {
    allowed = hasPermission(permission)
  } else if (anyOf && anyOf.length > 0) {
    allowed = hasAnyPermission(...anyOf)
  } else {
    allowed = true
  }

  return allowed ? <>{children}</> : <>{fallback}</>
}
