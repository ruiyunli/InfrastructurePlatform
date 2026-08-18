<template>
  <div class="home-page" :style="{ backgroundImage: `url(${bingImageUrl})` }">
    <DefaultLayout>
      <div class="welcome-card">
        <h2>欢迎, {{ user.state.username }}</h2>
        <p>这是一个 03-VUE 应用模板。背景图由后端 /api/background 提供，图片文件放在后端 backgrounds/ 目录，支持内网部署。</p>
      </div>
    </DefaultLayout>
  </div>
</template>

<script setup lang="ts">
import { onMounted, ref } from 'vue'
import { user } from '../stores/user'
import DefaultLayout from '../layouts/DefaultLayout.vue'

const bingImageUrl = ref('')

// 从后端 /api/background 获取背景图（内网环境由后端供图，前端不直连外网）
onMounted(async () => {
  try {
    const response = await fetch(`${import.meta.env.BASE_URL}api/background`)
    if (response.ok) {
      const blob = await response.blob()
      bingImageUrl.value = URL.createObjectURL(blob)
    }
  } catch (error) {
    console.error('获取背景图失败:', error)
  }
})
</script>

<style scoped>
.home-page {
  min-height: 100vh;
  background-size: cover;
  background-position: center;
  background-color: var(--color-bg-alt);
}
.welcome-card {
  background: rgba(255, 255, 255, 0.92);
  padding: 32px;
  border-radius: var(--radius);
  box-shadow: var(--shadow);
  max-width: 560px;
}
h2 { margin-bottom: 12px; }
p { color: var(--color-text-muted); }
</style>
