<!-- Download | Upload: sotto 1024 px la barra laterale e' a sole icone e non ha il sottomenu, quindi le due sezioni si cambiano da qui -->
<script setup lang="ts">
import { useI18n } from 'vue-i18n'
import { RouterLink } from 'vue-router'
import Icon from '@/components/Icon.vue'
const { t } = useI18n()
</script>
<template>
  <nav class="flex gap-1 border-b border-line bg-surface px-4 text-sm lg:hidden" :aria-label="t('nav.transfersSub')">
    <RouterLink to="/transfers" class="flex items-center gap-1.5 border-b-2 border-transparent px-4 py-2 text-mute hover:text-fg" exact-active-class="!border-accent !text-fg font-medium"><Icon name="dl" class="!size-4" />{{ t('nav.transfersDownload') }}</RouterLink>
    <RouterLink to="/transfers/uploads" class="flex items-center gap-1.5 border-b-2 border-transparent px-4 py-2 text-mute hover:text-fg" exact-active-class="!border-accent !text-fg font-medium"><Icon name="up" class="!size-4" />{{ t('nav.transfersUpload') }}</RouterLink>
  </nav>
</template>
