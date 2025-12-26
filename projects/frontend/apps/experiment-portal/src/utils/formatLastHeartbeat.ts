import { format } from 'date-fns'

export function formatLastHeartbeat(heartbeat?: string | null): string {
  if (!heartbeat) return 'Никогда'
  const date = new Date(heartbeat)
  const now = new Date()
  const diffMs = now.getTime() - date.getTime()
  const diffMins = Math.floor(diffMs / 60000)

  if (diffMins < 1) return 'Только что'
  if (diffMins < 60) return `${diffMins} мин назад`
  if (diffMins < 1440) return `${Math.floor(diffMins / 60)} ч назад`
  return format(date, 'dd MMM yyyy HH:mm')
}

