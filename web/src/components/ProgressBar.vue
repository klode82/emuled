<script setup lang="ts">
defineProps<{ value: number; tone?: 'accent' | 'ok' | 'warn' | 'bad'; label?: string }>()
</script>
<template>
  <progress class="block h-1.5 w-full appearance-none overflow-hidden rounded-full border-0 bg-sunk motion-reduce:[&::-webkit-progress-value]:transition-none [&::-webkit-progress-bar]:bg-sunk [&::-webkit-progress-value]:rounded-full [&::-webkit-progress-value]:transition-[width] [&::-webkit-progress-value]:duration-500 [&::-moz-progress-bar]:rounded-full"
    :class="tone === 'ok' ? '[&::-webkit-progress-value]:bg-ok [&::-moz-progress-bar]:bg-ok' : tone === 'warn' ? '[&::-webkit-progress-value]:bg-warn [&::-moz-progress-bar]:bg-warn' : tone === 'bad' ? '[&::-webkit-progress-value]:bg-bad [&::-moz-progress-bar]:bg-bad' : '[&::-webkit-progress-value]:bg-accent [&::-moz-progress-bar]:bg-accent'"
    :value="Math.min(100, Math.max(0, value))" max="100" :aria-label="label" />
</template>
