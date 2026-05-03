<template>
  <div class="home-page" :style="{ backgroundImage: `url(${bingImageUrl})` }">
    <div class="header">
      <div v-if="userStore.isLoggedIn" class="user-info">
        <span class="username">{{ userStore.username }}</span>
        <button @click="handleLogout" class="logout-btn">注销</button>
      </div>
      <button v-else @click="goToLogin" class="login-btn">登录</button>
    </div>

    <div class="content">
      <div class="search-container">
        <input
          v-model="searchQuery"
          type="text"
          placeholder="搜索..."
          class="search-box"
          @click="showEgg"
        />
        <div v-if="showEggMessage" class="egg-message">
          🎉 这是一个彩蛋！你找到了隐藏的惊喜！
        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, onMounted } from 'vue'
import { useRouter } from 'vue-router'
import { useUserStore } from '../stores/userStore'
import { logout } from '../api/auth'

const router = useRouter()
const userStore = useUserStore()

const bingImageUrl = ref('')
const searchQuery = ref('')
const showEggMessage = ref(false)

// 获取必应每日图片
const fetchBingImage = async () => {
  try {
    const response = await fetch('https://www.bing.com/HPImageArchive.aspx?format=js&idx=0&n=1&mkt=zh-CN')
    const data = await response.json()
    if (data.images && data.images[0]) {
      bingImageUrl.value = `https://www.bing.com${data.images[0].url}`
    }
  } catch (error) {
    console.error('获取必应图片失败:', error)
    // 使用默认背景
    bingImageUrl.value = 'https://images.unsplash.com/photo-1506905925346-21bd4a458e7a?w=1920'
  }
}

// 点击搜索框显示彩蛋
const showEgg = () => {
  showEggMessage.value = true
  setTimeout(() => {
    showEggMessage.value = false
  }, 3000)
}

// 跳转到登录页
const goToLogin = () => {
  router.push('/login')
}

// 处理注销
const handleLogout = async () => {
  try {
    await logout()
  } catch (error) {
    console.error('注销失败:', error)
  }
  userStore.logout()
  router.push('/login')
}

onMounted(() => {
  fetchBingImage()
})
</script>

<style scoped>
.home-page {
  min-height: 100vh;
  background-size: cover;
  background-position: center;
  background-repeat: no-repeat;
  display: flex;
  flex-direction: column;
}

.header {
  padding: 20px;
  display: flex;
  justify-content: flex-end;
  align-items: center;
}

.user-info {
  display: flex;
  align-items: center;
  gap: 15px;
}

.username {
  color: white;
  font-size: 16px;
  font-weight: 500;
  text-shadow: 1px 1px 2px rgba(0, 0, 0, 0.5);
}

.logout-btn,
.login-btn {
  padding: 8px 20px;
  background: white;
  color: #333;
  border: none;
  border-radius: 4px;
  font-size: 14px;
  cursor: pointer;
  transition: all 0.3s;
}

.logout-btn:hover,
.login-btn:hover {
  background: #f0f0f0;
  transform: translateY(-1px);
}

.content {
  flex: 1;
  display: flex;
  justify-content: center;
  align-items: center;
  padding: 20px;
}

.search-container {
  width: 100%;
  max-width: 600px;
  position: relative;
}

.search-box {
  width: 100%;
  padding: 16px 20px;
  font-size: 18px;
  border: none;
  border-radius: 24px;
  background: white;
  box-shadow: 0 2px 8px rgba(0, 0, 0, 0.2);
  cursor: pointer;
  transition: all 0.3s;
}

.search-box:hover {
  box-shadow: 0 4px 12px rgba(0, 0, 0, 0.3);
}

.search-box:focus {
  outline: none;
  box-shadow: 0 6px 16px rgba(0, 0, 0, 0.4);
}

.egg-message {
  position: absolute;
  top: 100%;
  left: 50%;
  transform: translateX(-50%);
  margin-top: 20px;
  padding: 15px 30px;
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
  color: white;
  border-radius: 10px;
  font-size: 16px;
  animation: fadeIn 0.3s;
  white-space: nowrap;
}

@keyframes fadeIn {
  from {
    opacity: 0;
    transform: translateX(-50%) translateY(-10px);
  }
  to {
    opacity: 1;
    transform: translateX(-50%) translateY(0);
  }
}
</style>