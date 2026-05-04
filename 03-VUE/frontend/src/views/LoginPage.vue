<template>
  <div class="login-container">
    <div class="back-btn" @click="handleCancel">
      <span class="back-icon">←</span>
      <span>返回</span>
    </div>
    <div class="login-box">
      <h1>登录</h1>
      <form @submit.prevent="handleLogin">
        <div class="form-group">
          <input
            v-model="username"
            type="text"
            placeholder="用户名"
            required
            @input="errorMessage = ''"
          />
        </div>
        <div class="form-group">
          <input
            v-model="password"
            type="password"
            placeholder="密码"
            required
            @input="errorMessage = ''"
          />
        </div>
        <div v-if="errorMessage" class="error-message">
          {{ errorMessage }}
        </div>
        <button type="submit" class="login-button">登录</button>
      </form>
    </div>
  </div>
</template>

<script setup>
import { ref } from 'vue'
import { useRouter } from 'vue-router'
import { useUserStore } from '../stores/userStore'
import { login } from '../api/auth'

const router = useRouter()
const userStore = useUserStore()

const username = ref('')
const password = ref('')
const errorMessage = ref('')

const handleLogin = async () => {
  errorMessage.value = ''

  try {
    const response = await login(username.value, password.value)

    if (response.success) {
      // 保存token和用户名到store
      userStore.login(response.token, username.value)
      // 跳转到主页
      router.push('/')
    } else {
      errorMessage.value = response.message || '登录失败'
    }
  } catch (error) {
    errorMessage.value = error.response?.data?.message || '登录失败，请检查用户名和密码'
  }
}

const handleCancel = () => {
  router.push('/')
}
</script>

<style scoped>
.login-container {
  display: flex;
  justify-content: center;
  align-items: center;
  min-height: 100vh;
  background: linear-gradient(135deg, #1a1a2e 0%, #16213e 50%, #0f3460 100%);
  position: relative;
  overflow: hidden;
}

/* 背景装饰 */
.login-container::before {
  content: '';
  position: absolute;
  top: -50%;
  left: -50%;
  width: 200%;
  height: 200%;
  background: radial-gradient(circle, rgba(79, 172, 254, 0.1) 0%, transparent 50%);
  animation: rotateBg 20s linear infinite;
}

.login-container::after {
  content: '';
  position: absolute;
  bottom: -30%;
  right: -30%;
  width: 100%;
  height: 100%;
  background: radial-gradient(circle, rgba(0, 206, 201, 0.08) 0%, transparent 50%);
  animation: pulseBg 8s ease-in-out infinite;
}

@keyframes rotateBg {
  from { transform: rotate(0deg); }
  to { transform: rotate(360deg); }
}

@keyframes pulseBg {
  0%, 100% { transform: scale(1); opacity: 0.5; }
  50% { transform: scale(1.1); opacity: 0.8; }
}

.back-btn {
  position: absolute;
  top: 24px;
  left: 24px;
  display: flex;
  align-items: center;
  gap: 8px;
  color: rgba(255, 255, 255, 0.7);
  font-size: 14px;
  cursor: pointer;
  padding: 10px 16px;
  border-radius: 8px;
  background: rgba(255, 255, 255, 0.1);
  border: 1px solid rgba(255, 255, 255, 0.1);
  transition: all 0.3s;
  z-index: 10;
}

.back-btn:hover {
  background: rgba(255, 255, 255, 0.15);
  color: white;
  transform: translateX(-4px);
}

.back-icon {
  font-size: 16px;
  transition: transform 0.3s;
}

.back-btn:hover .back-icon {
  transform: translateX(-4px);
}

.login-box {
  background: rgba(255, 255, 255, 0.95);
  padding: 48px 40px;
  border-radius: 16px;
  box-shadow: 0 25px 50px -12px rgba(0, 0, 0, 0.4);
  width: 100%;
  max-width: 400px;
  position: relative;
  z-index: 10;
  backdrop-filter: blur(10px);
}

.login-box::before {
  content: '';
  position: absolute;
  top: 0;
  left: 0;
  right: 0;
  height: 4px;
  background: linear-gradient(90deg, #4facfe 0%, #00f2fe 100%);
  border-radius: 16px 16px 0 0;
}

h1 {
  text-align: center;
  margin-bottom: 32px;
  color: #1a1a2e;
  font-size: 28px;
  font-weight: 600;
  letter-spacing: 2px;
}

.form-group {
  margin-bottom: 24px;
}

input {
  width: 100%;
  padding: 14px 16px;
  border: 2px solid #e8e8e8;
  border-radius: 10px;
  font-size: 15px;
  box-sizing: border-box;
  transition: all 0.3s;
  background: #f8f9fa;
}

input:focus {
  outline: none;
  border-color: #4facfe;
  background: white;
  box-shadow: 0 0 0 4px rgba(79, 172, 254, 0.15);
}

input::placeholder {
  color: #999;
}

.error-message {
  color: #e74c3c;
  font-size: 13px;
  margin-bottom: 20px;
  text-align: center;
  padding: 10px;
  background: rgba(231, 76, 60, 0.1);
  border-radius: 8px;
  border: 1px solid rgba(231, 76, 60, 0.2);
}

.login-button {
  width: 100%;
  padding: 14px;
  background: linear-gradient(135deg, #4facfe 0%, #00f2fe 100%);
  color: white;
  border: none;
  border-radius: 10px;
  font-size: 16px;
  font-weight: 600;
  cursor: pointer;
  transition: all 0.3s;
  letter-spacing: 1px;
  margin-top: 8px;
}

.login-button:hover {
  transform: translateY(-2px);
  box-shadow: 0 8px 20px rgba(79, 172, 254, 0.4);
}

.login-button:active {
  transform: translateY(0);
}
</style>
