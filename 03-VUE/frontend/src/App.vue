<script setup>
import { onMounted } from 'vue'
import { useUserStore } from './stores/userStore'

const userStore = useUserStore()

// 应用启动时检查登录状态
onMounted(() => {
  userStore.checkAuth()
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
