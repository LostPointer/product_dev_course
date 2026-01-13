import { defineConfig, loadEnv } from 'vite'
import react from '@vitejs/plugin-react'

// https://vitejs.dev/config/
export default defineConfig(({ mode }) => {
  const env = loadEnv(mode, process.cwd(), '')
  const authProxyUrl = env.VITE_AUTH_PROXY_URL || 'http://localhost:8080'

  return {
    plugins: [react()],
    server: {
      host: '0.0.0.0', // Доступ извне контейнера
      port: 3000,
      proxy: {
        // Experiment-service API and Telemetry ingest (through auth-proxy)
        '/api': {
          target: authProxyUrl,
          changeOrigin: true,
        },
        // Auth-service projects API (through auth-proxy)
        '/projects': {
          target: authProxyUrl,
          changeOrigin: true,
        },
      },
    },
    test: {
      globals: true,
      environment: 'jsdom',
      setupFiles: './src/setupTests.ts',
      css: true,
    },
  }
})

