import { HttpError } from '@lostpointer/web-http'

/**
 * Text for an error shown on screen. Server messages from the shared transport (`HttpError`) and
 * legacy Axios errors (`response.data.error|message`) are shown as is; transport failures (network,
 * timeout, abort, malformed body) and bodiless HTTP errors get the screen's own fallback.
 */
export function apiErrorMessage(error: unknown, fallback: string): string {
  if (error instanceof HttpError) {
    return error.kind === 'api' && error.message !== `HTTP ${error.status}` ? error.message : fallback
  }
  const data = (error as { response?: { data?: { error?: unknown; message?: unknown } } } | null)?.response?.data
  if (typeof data?.error === 'string' && data.error) return data.error
  if (typeof data?.message === 'string' && data.message) return data.message
  return fallback
}
