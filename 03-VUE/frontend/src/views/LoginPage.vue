<template>
  <div class="login-container">
    <div class="login-box">
      <h1>登录</h1>
      <form @submit.prevent="handleLogin">
        <AppInput v-model="username" label="用户名" placeholder="请输入用户名" required />
        <AppInput v-model="password" label="密码" type="password" placeholder="请输入密码" required />
        <div v-if="errorMessage" class="error-message">{{ errorMessage }}</div>
        <AppButton :disabled="loading">{{ loading ? '登录中...' : '登录' }}</AppButton>
      </form>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref } from 'vue'
import { useRouter } from 'vue-router'
import { user } from '../stores/user'
import { login } from '../api/http'
import AppInput from '../components/AppInput.vue'
import AppButton from '../components/AppButton.vue'

const router = useRouter()
const username = ref('')
const password = ref('')
const errorMessage = ref('')
const loading = ref(false)

const handleLogin = async () => {
  errorMessage.value = ''
  loading.value = true
  try {
    const res = await login(username.value, password.value)
    if (res.success) {
      user.login(res.token, username.value)
      router.push('/')
    } else {
      errorMessage.value = res.message || '登录失败'
    }
  } catch (error) {
    errorMessage.value = (error as { response?: { data?: { message?: string } } })?.response?.data?.message || '登录失败，请检查用户名和密码'
  } finally {
    loading.value = false
  }
}
</script>

<style scoped>
.login-container { display: flex; justify-content: center; align-items: center; min-height: 100vh; background: linear-gradient(135deg, #1a1a2e 0%, #16213e 50%, #0f3460 100%); }
.login-box { background: rgba(255, 255, 255, 0.95); padding: 48px 40px; border-radius: 16px; box-shadow: 0 25px 50px -12px rgba(0, 0, 0, 0.4); width: 100%; max-width: 400px; }
.login-box h1 { text-align: center; margin-bottom: 32px; color: #1a1a2e; font-size: 28px; font-weight: 600; }
.login-box form { display: flex; flex-direction: column; gap: 4px; }
.error-message { margin-bottom: 16px; }
.login-box .btn { width: 100%; padding: 14px; font-size: 16px; margin-top: 8px; }
</style>
