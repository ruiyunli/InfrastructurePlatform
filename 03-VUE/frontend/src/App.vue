<script setup lang="ts">
import { onMounted } from 'vue'
import { user } from './stores/user'

// 应用启动时检查登录状态（从 localStorage 恢复并验证 token）
onMounted(() => {
  user.checkAuth()
})
</script>

<template>
  <router-view />
</template>

<style>
* {
  margin: 0;
  padding: 0;
  box-sizing: border-box;
}

html, body {
  width: 100%;
  height: 100%;
  overflow-x: hidden;
}

body {
  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, 'Helvetica Neue', Arial, sans-serif;
}

#app {
  width: 100%;
  height: 100%;
}
</style>
