import { createRouter, createWebHistory } from 'vue-router'
import { user } from '../stores/user'

const routes = [
  { path: '/', name: 'Home', component: () => import('../views/HomePage.vue') },
  { path: '/login', name: 'Login', component: () => import('../views/LoginPage.vue') }
]

const router = createRouter({
  history: createWebHistory(import.meta.env.BASE_URL),
  routes
})

router.beforeEach((to) => {
  if (to.path !== '/login' && !user.isLoggedIn) return '/login'
  if (to.path === '/login' && user.isLoggedIn) return '/'
  return true
})

export default router
