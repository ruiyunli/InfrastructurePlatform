import { createRouter, createWebHistory } from 'vue-router'
import { useUserStore } from '../stores/userStore'
import HomePage from '../views/HomePage.vue'
import LoginPage from '../views/LoginPage.vue'

const routes = [
  {
    path: '/',
    name: 'Home',
    component: HomePage
  },
  {
    path: '/login',
    name: 'Login',
    component: LoginPage
  }
]

const router = createRouter({
  history: createWebHistory(),
  routes
})

// 导航守卫：已登录访问/login自动跳转/
router.beforeEach((to, from) => {
  const userStore = useUserStore()

  if (to.path === '/login' && userStore.isLoggedIn) {
    return '/'
  }
  return true
})

export default router
