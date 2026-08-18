import axios from 'axios'

// baseURL 跟随全局 base：/LoginApp/api，dev 下 /api 由 vite proxy 转发到 8080
const api = axios.create({
  baseURL: `${import.meta.env.BASE_URL}api`
})

// 请求拦截器：直接读 localStorage 取 token（避免与 user 模块循环依赖）
api.interceptors.request.use(config => {
  const token = localStorage.getItem('token')
  if (token) {
    config.headers.Authorization = `Bearer ${token}`
  }
  return config
})

// 响应拦截器：捕获 401，非登录接口则清空登录态并跳登录页
api.interceptors.response.use(
  r => r,
  error => {
    if (error.response?.status === 401) {
      const isLogin = error.config?.url?.includes('/api/login')
      if (!isLogin) {
        localStorage.removeItem('token')
        localStorage.removeItem('username')
        window.location.href = `${import.meta.env.BASE_URL}login`
      }
    }
    return Promise.reject(error)
  }
)

export const login = (username: string, password: string) => api.post('/login', { username, password }).then(r => r.data)
export const logout = () => api.post('/logout').then(r => r.data)
export const verify = () => api.get('/verify').then(r => r.data)

export default api
