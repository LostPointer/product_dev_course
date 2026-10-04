import { createHttpClient } from '@lostpointer/web-http'
import { getCsrfToken } from '../../utils/csrf'
import { getTraceId } from '../../utils/trace'
import { generateRequestId } from '../../utils/uuid'
import { AUTH_PROXY_URL } from './baseUrl'
import { installSessionRecovery } from './sessionRecovery'

/**
 * Projects is the first domain on the shared `@lostpointer/web-http` transport. The shared client
 * owns credentials, timeout, request IDs, CSRF and error normalization (`HttpError`); this adapter
 * adds the trace header and the application's session recovery, which must run before the final
 * normalization and therefore lives in `configure`.
 */
export const projectsClient = createHttpClient({
  baseURL: AUTH_PROXY_URL,
  timeoutMs: 30_000,
  requestId: generateRequestId,
  csrfToken: getCsrfToken,
  configure(instance) {
    instance.interceptors.request.use((config) => {
      config.headers.set('X-Trace-Id', getTraceId())
      return config
    })
    installSessionRecovery(instance)
  },
})
