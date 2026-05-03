import { defineStore } from 'pinia'
import { verify } from '../api/auth'

export const useUserStore = defineStore('user', {
  state: () => ({
    username: '',
    token: '',
    isLoggedIn: false
  }),

  actions: {
    // 登录：保存到store和localStorage
    login(token, username) {
      this.token = token
      this.username = username
      this.isLoggedIn = true

      localStorage.setItem('token', token)
      localStorage.setItem('username', username)
    },

    // 注销：清除store和localStorage
    logout() {
      this.token = ''
      this.username = ''
      this.isLoggedIn = false

      localStorage.removeItem('token')
      localStorage.removeItem('username')
    },

    // 从localStorage恢复状态并验证token
    async checkAuth() {
      const token = localStorage.getItem('token')
      const username = localStorage.getItem('username')

      if (!token || !username) {
        this.logout()
        return false
      }

      try {
        // 验证token是否有效
        await verify()

        // token有效，恢复状态
        this.token = token
        this.username = username
        this.isLoggedIn = true
        return true
      } catch (error) {
        // token无效，清除状态
        this.logout()
        return false
      }
    }
  }
})
