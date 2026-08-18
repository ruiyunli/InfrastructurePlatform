import { reactive } from 'vue'
import { verify } from '../api/http'

const state = reactive({
  token: localStorage.getItem('token') || '',
  username: localStorage.getItem('username') || ''
})

export const user = {
  get state() { return state },
  get isLoggedIn() { return !!state.token },
  get token() { return state.token },
  login(token: string, username: string) {
    state.token = token
    state.username = username
    localStorage.setItem('token', token)
    localStorage.setItem('username', username)
  },
  logout() {
    state.token = ''
    state.username = ''
    localStorage.removeItem('token')
    localStorage.removeItem('username')
  },
  async checkAuth() {
    if (!state.token) return false
    try { await verify(); return true }
    catch { this.logout(); return false }
  }
}
