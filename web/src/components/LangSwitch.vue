<script setup lang="ts">
import { useI18n } from 'vue-i18n'
import { LANGS, setLang, type Lang } from '@/i18n'
const { t, locale } = useI18n()
</script>
<template>
  <label class="inline-flex items-center gap-2 text-sm text-mute"><span>{{ t('lang.label') }}</span>
    <select class="rounded-lg border border-line bg-bg px-2.5 py-1.5 text-sm text-fg" :value="locale" @change="setLang(($event.target as HTMLSelectElement).value as Lang)">
      <option v-for="l in LANGS" :key="l.code" :value="l.code">{{ l.name }}</option></select></label>
</template>
