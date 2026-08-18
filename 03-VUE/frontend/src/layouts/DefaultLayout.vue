<template>
  <div class="layout">
    <header class="header">
      <div class="logo">
        <img :src="logo" alt="logo" />
        <span>{{ appName }}</span>
      </div>
      <div v-if="user.isLoggedIn" class="user-area">
        <span>{{ user.state.username }}</span>
        <AppButton variant="ghost" @click="handleLogout">
          <svg class="icon" role="presentation" aria-hidden="true">
            <use :href="iconLogout"></use>
          </svg>
          注销
        </AppButton>
      </div>
      <AppButton v-else variant="ghost" @click="$router.push('/login')">登录</AppButton>
    </header>
    <main class="main">
      <slot />
    </main>
  </div>
</template>

<script setup lang="ts">
import { useRouter } from 'vue-router'
import logo from '../assets/logo.svg'
import { user } from '../stores/user'
import { logout } from '../api/http'
import AppButton from '../components/AppButton.vue'

const router = useRouter()
const appName = 'LoginApp'  // 新应用可改
// public 静态文件带 base 前缀（dev 下 /LoginApp/icons.svg，生产同），不能用模板内 import.meta.env
const iconLogout = `${import.meta.env.BASE_URL}icons.svg#icon-logout`

const handleLogout = async () => {
  try { await logout() } catch (e) { /* ignore */ }
  user.logout()
  router.push('/login')
}
</script>

<style scoped>
.layout { min-height: 100vh; display: flex; flex-direction: column; }
.header { padding: 12px 24px; display: flex; justify-content: space-between; align-items: center; background: white; border-bottom: 1px solid var(--color-border); }
.logo { display: flex; align-items: center; gap: 8px; font-size: 16px; font-weight: 500; }
.logo img { width: 24px; height: 24px; }
.icon { width: 16px; height: 16px; }
.user-area { display: flex; align-items: center; gap: 12px; }
.main { flex: 1; padding: 24px; max-width: 1200px; width: 100%; margin: 0 auto; }
</style>
