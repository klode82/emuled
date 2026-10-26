<script setup lang="ts">
import Icon from './Icon.vue'
defineProps<{ icon: string; title: string; text?: string }>()
</script>
<template>
  <div class="flex flex-col items-center gap-2 px-6 py-14 text-center">
    <span class="grid size-12 place-items-center rounded-full bg-sunk text-mute"><Icon :name="icon" class="size-6" /></span>
    <p class="text-sm font-medium">{{ title }}</p>
    <p v-if="text" class="max-w-sm text-sm text-mute">{{ text }}</p>
    <div class="pt-2"><slot /></div>
  </div>
</template>
