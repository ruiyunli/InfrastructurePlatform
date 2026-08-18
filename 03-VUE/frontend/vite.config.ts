import { defineConfig, loadEnv } from 'vite'
import vue from '@vitejs/plugin-vue'

export default defineConfig(({ mode }) => {
  const env = loadEnv(mode, process.cwd(), '')
  const base = env.VITE_BASE || '/LoginApp/'
  // 前端 API baseURL 为 `${base}api`（见 http.ts），dev 下需把该前缀代理到后端
  const apiPrefix = `${base.replace(/\/+$/, '')}/api`
  return {
    base,
    plugins: [vue()],
    server: {
      proxy: {
        '/api': {
          target: 'http://localhost:8080',
          changeOrigin: true
        },
        [apiPrefix]: {
          target: 'http://localhost:8080',
          changeOrigin: true,
          rewrite: path => path.replace(apiPrefix, '/api')
        }
      }
    }
  }
})
