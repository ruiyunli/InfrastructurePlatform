import axios from 'axios'

// 创建axios实例
const api = axios.create({
  baseURL: 'http://localhost:8080'
})

// 请求拦截器：自动添加Authorization header
api.interceptors.request.use(
  config => {
    const token = localStorage.getItem('token')
    if (token) {
      config.headers.Authorization = `Bearer ${token}`
    }
    return config
  },
  error => {
    return Promise.reject(error)
  }
)

// 响应拦截器：捕获401，自动跳转登录页
api.interceptors.response.use(
  response => {
    return response
  },
  error => {
    if (error.response && error.response.status === 401) {
      // 清除本地存储
      localStorage.removeItem('token')
      localStorage.removeItem('username')
      // 跳转到登录页
      window.location.href = '/login'
    }
    return Promise.reject(error)
  }
)

// 登录接口
export const login = async (username, password) => {
  const response = await api.post('/api/login', { username, password })
  return response.data
}

// 注销接口
export const logout = async () => {
  const response = await api.post('/api/logout')
  return response.data
}

// 验证token接口
export const verify = async () => {
  const response = await api.get('/api/verify')
  return response.data
}

export default api
