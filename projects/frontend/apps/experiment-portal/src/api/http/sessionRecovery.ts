import axios from 'axios'
import type { AxiosError, AxiosInstance, InternalAxiosRequestConfig } from 'axios'
import { maybeEmitHttpErrorToastFromAxiosError } from '../../utils/httpDebug'
import { AUTH_PROXY_URL } from './baseUrl'

type RecoverableConfig = InternalAxiosRequestConfig & {
  _retry?: boolean
  /** Requests with non-session auth (e.g. sensor tokens): a 401 is not an expired user session. */
  _skipAuthInterceptor?: boolean
}

/**
 * Application-owned session recovery shared by the legacy and the migrated clients:
 * one refresh through Auth Proxy and one retry on the same instance, then the login page.
 * Every other failure only emits the DEV debug toast.
 */
export function installSessionRecovery(instance: AxiosInstance): void {
  instance.interceptors.response.use(undefined, async (error: AxiosError) => {
    const original = error.config as RecoverableConfig | undefined
    if (error.response?.status === 401 && original && !original._retry && !original._skipAuthInterceptor) {
      original._retry = true
      try {
        await axios.post(`${AUTH_PROXY_URL}/auth/refresh`, {}, { withCredentials: true })
        return instance(original)
      } catch (refreshError) {
        maybeEmitHttpErrorToastFromAxiosError(refreshError)
        window.location.href = '/login'
        return Promise.reject(refreshError)
      }
    }
    maybeEmitHttpErrorToastFromAxiosError(error)
    return Promise.reject(error)
  })
}
