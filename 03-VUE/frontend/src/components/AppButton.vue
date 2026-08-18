<template>
  <button :class="['btn', `btn-${variant}`]" :disabled="disabled" @click="$emit('click')">
    <slot />
  </button>
</template>

<script setup lang="ts">
defineProps<{
  variant?: 'primary' | 'danger' | 'ghost'
  disabled?: boolean
}>()

defineEmits<{ click: [] }>()
</script>
