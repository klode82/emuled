<script setup lang="ts">
import { useI18n } from 'vue-i18n'
import { useToasts } from '@/stores/toasts'
import Icon from './Icon.vue'
const { t } = useI18n()
const toasts = useToasts()
</script>
<template>
  <div class="pointer-events-none fixed inset-x-0 top-3 z-50 flex flex-col items-center gap-2 px-4 md:inset-x-auto md:bottom-12 md:left-20 md:top-auto md:items-start md:px-0 lg:left-60" aria-live="polite">
    <div v-for="x in toasts.items" :key="x.id" class="pointer-events-none flex max-w-md items-start gap-2 rounded-lg border px-4 py-3 text-sm shadow-lg"
      :class="x.kind === 'error' ? 'border-bad bg-surface text-fg' : 'border-line bg-surface text-fg'" :role="x.kind === 'error' ? 'alert' : 'status'">
      <Icon :name="x.kind === 'error' ? 'warn' : x.kind === 'ok' ? 'check' : 'info'" class="mt-0.5" :class="x.kind === 'error' ? 'text-bad' : x.kind === 'ok' ? 'text-ok' : 'text-accent'" />
      <span class="min-w-0 flex-1 break-words">{{ x.text }}</span>
      <button class="pointer-events-auto text-mute hover:text-fg" :aria-label="t('common.closeMessage')" @click="toasts.dismiss(x.id)"><Icon name="x" /></button>
    </div>
  </div>
</template>
