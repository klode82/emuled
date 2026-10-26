<!-- Download | Upload come due pulsanti uniti (modelli B, C e D, dove non c'e' il sottomenu della barra laterale) -->
<script setup lang="ts">
import { useI18n } from 'vue-i18n'
import { RouterLink } from 'vue-router'
import Icon from '@/components/Icon.vue'
const { t } = useI18n()
</script>
<template>
  <nav class="inline-flex gap-1 rounded-full bg-sunk p-1 text-xs font-semibold" :aria-label="t('nav.transfersSub')">
    <RouterLink to="/transfers" class="inline-flex items-center gap-1.5 rounded-full px-3 py-1.5 text-mute hover:text-fg" exact-active-class="!bg-surface !text-fg shadow-sm"><Icon name="dl" class="!size-3.5" />{{ t('nav.transfersDownload') }}</RouterLink>
    <RouterLink to="/transfers/uploads" class="inline-flex items-center gap-1.5 rounded-full px-3 py-1.5 text-mute hover:text-fg" exact-active-class="!bg-surface !text-fg shadow-sm"><Icon name="up" class="!size-3.5" />{{ t('nav.transfersUpload') }}</RouterLink>
  </nav>
</template>
