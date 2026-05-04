<template>
  <div class="home-page" :style="{ backgroundImage: `url(${bingImageUrl})` }">
    <div class="header">
      <div v-if="userStore.isLoggedIn" class="user-dropdown" @mouseenter="handleMouseEnter" @mouseleave="handleMouseLeave">
        <button class="user-btn">
          <span>{{ userStore.username }}</span>
          <span class="arrow">▼</span>
        </button>
        <div v-show="showDropdown" class="dropdown-menu" @mouseenter="handleMenuMouseEnter" @mouseleave="handleMenuMouseLeave">
          <button @click="handleLogout" class="dropdown-item">退出登录</button>
        </div>
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
          @keyup.enter="showEgg"
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
const showDropdown = ref(false)
let hideDropdownTimer = null

const handleMouseEnter = () => {
  clearTimeout(hideDropdownTimer)
  showDropdown.value = true
}

const handleMouseLeave = () => {
  hideDropdownTimer = setTimeout(() => {
    showDropdown.value = false
  }, 150)
}

const handleMenuMouseEnter = () => {
  clearTimeout(hideDropdownTimer)
  showDropdown.value = true
}

const handleMenuMouseLeave = () => {
  hideDropdownTimer = setTimeout(() => {
    showDropdown.value = false
  }, 150)
}

// 获取背景图片
const fetchBingImage = async () => {
  try {
    const response = await fetch('http://localhost:8080/api/background')
    if (response.ok) {
      const blob = await response.blob()
      bingImageUrl.value = URL.createObjectURL(blob)
    }
  } catch (error) {
    console.error('获取背景图失败:', error)
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

// 处理退出登录
const handleLogout = async () => {
  showDropdown.value = false
  try {
    await logout()
  } catch (error) {
    console.error('退出登录失败:', error)
  }
  userStore.logout()
}

onMounted(() => {
  fetchBingImage()
})
</script>

<style scoped>
.home-page {
  min-height: 100vh;
  height: 100vh;
  background-size: cover;
  background-position: center;
  background-repeat: no-repeat;
  display: flex;
  flex-direction: column;
  position: relative;
}

/* 暗色遮罩层 */
.home-page::before {
  content: '';
  position: absolute;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  background: rgba(0, 0, 0, 0.3);
  z-index: 1;
}

.header {
  padding: 24px 32px;
  display: flex;
  justify-content: flex-end;
  align-items: center;
  position: relative;
  z-index: 10;
}

.user-dropdown {
  position: relative;
}

.user-btn {
  display: flex;
  align-items: center;
  gap: 10px;
  padding: 10px 20px;
  background: rgba(255, 255, 255, 0.95);
  color: #1a1a2e;
  border: none;
  border-radius: 25px;
  font-size: 14px;
  font-weight: 500;
  cursor: pointer;
  transition: all 0.3s;
  box-shadow: 0 2px 10px rgba(0, 0, 0, 0.1);
}

.user-btn:hover {
  background: white;
  transform: translateY(-2px);
  box-shadow: 0 4px 15px rgba(0, 0, 0, 0.2);
}

.arrow {
  font-size: 10px;
  color: #666;
  transition: transform 0.3s;
}

.user-dropdown:hover .arrow {
  transform: rotate(180deg);
}

.dropdown-menu {
  position: absolute;
  top: calc(100% + 8px);
  right: 0;
  background: white;
  border-radius: 12px;
  box-shadow: 0 10px 30px rgba(0, 0, 0, 0.15);
  overflow: hidden;
  z-index: 100;
  min-width: 140px;
  opacity: 0;
  visibility: hidden;
  transform: translateY(-10px);
  transition: all 0.3s;
}

.user-dropdown:hover .dropdown-menu,
.dropdown-menu:hover {
  opacity: 1;
  visibility: visible;
  transform: translateY(0);
}

.dropdown-item {
  width: 100%;
  padding: 12px 20px;
  background: none;
  border: none;
  text-align: left;
  cursor: pointer;
  font-size: 14px;
  color: #333;
  transition: all 0.2s;
}

.dropdown-item:hover {
  background: linear-gradient(135deg, #ff6b6b 0%, #ee5a5a 100%);
  color: white;
}

.login-btn {
  padding: 10px 24px;
  background: rgba(255, 255, 255, 0.95);
  color: #1a1a2e;
  border: none;
  border-radius: 25px;
  font-size: 14px;
  font-weight: 500;
  cursor: pointer;
  transition: all 0.3s;
  box-shadow: 0 2px 10px rgba(0, 0, 0, 0.1);
}

.login-btn:hover {
  background: white;
  transform: translateY(-2px);
  box-shadow: 0 4px 15px rgba(0, 0, 0, 0.2);
}

.content {
  flex: 1;
  display: flex;
  justify-content: center;
  align-items: center;
  padding: 20px;
  position: relative;
  z-index: 10;
}

.search-container {
  width: 100%;
  max-width: 680px;
  position: relative;
}

.search-box {
  width: 100%;
  padding: 20px 28px;
  font-size: 18px;
  border: none;
  border-radius: 50px;
  background: rgba(255, 255, 255, 0.95);
  box-shadow: 0 4px 20px rgba(0, 0, 0, 0.15);
  cursor: pointer;
  transition: all 0.3s;
}

.search-box:hover {
  box-shadow: 0 6px 30px rgba(0, 0, 0, 0.25);
  transform: scale(1.01);
}

.search-box:focus {
  outline: none;
  box-shadow: 0 8px 40px rgba(0, 0, 0, 0.3);
  transform: scale(1.02);
}

.egg-message {
  position: absolute;
  top: 100%;
  left: 50%;
  transform: translateX(-50%);
  margin-top: 24px;
  padding: 18px 36px;
  background: linear-gradient(135deg, #4facfe 0%, #00f2fe 100%);
  color: white;
  border-radius: 16px;
  font-size: 16px;
  font-weight: 500;
  animation: fadeIn 0.3s;
  white-space: nowrap;
  box-shadow: 0 10px 30px rgba(79, 172, 254, 0.4);
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