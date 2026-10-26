<!-- Modello B "Cruscotto": barra in alto con le sezioni come pillole; il riepilogo a tessere sta nella schermata Trasferimenti -->
<script setup lang="ts">
import { computed } from 'vue'
import { useI18n } from 'vue-i18n'
import { RouterLink, RouterView, useRoute } from 'vue-router'
import { SECTIONS } from '@/router'
import { useSession } from '@/stores/session'
import { useUi } from '@/stores/ui'
import Icon from '@/components/Icon.vue'
import logo from '@/assets/emuled_logo_transparent.png'

const { t } = useI18n()
const session = useSession(), ui = useUi(), route = useRoute()
const linkBad = computed(() => session.link !== 'open')
</script>
<template>
  <div class="flex h-full flex-col overflow-hidden bg-bg">
    <div v-if="linkBad" class="flex shrink-0 items-center gap-2 border-b border-line bg-soft px-4 py-1.5 text-xs text-warn" role="status"><Icon name="wifi" />{{ t('shell.linkDown') }}</div>
    <header class="shrink-0 border-b border-line bg-surface">
      <div class="flex items-center gap-3 px-4 py-3 md:px-6">
        <img :src="logo" alt="" class="size-8 shrink-0 object-contain" />
        <span class="hidden text-base font-extrabold tracking-tight text-accent sm:inline">emuled</span>
        <nav class="flex min-w-0 flex-1 gap-1 overflow-x-auto" :aria-label="t('nav.sections')">
          <RouterLink v-for="s in SECTIONS" :key="s.name" :to="s.path" :title="t('nav.' + s.name)" class="flex items-center gap-2 whitespace-nowrap rounded-full px-3.5 py-1.5 text-sm font-semibold text-mute hover:bg-sunk hover:text-fg" :class="s.name === 'transfers' && route.path.startsWith('/transfers/') ? '!bg-accent !text-onaccent' : ''" active-class="!bg-accent !text-onaccent">
            <Icon :name="s.icon" class="hidden sm:block" />{{ t('nav.' + s.name) }}</RouterLink>
        </nav>
        <button class="inline-flex items-center gap-1.5 rounded-full border border-line px-3.5 py-1.5 text-sm font-semibold hover:bg-sunk" :aria-label="t('skin.dashboard.pasteLink')" @click="ui.openAdd()"><Icon name="link" /><span class="hidden md:inline">{{ t('skin.dashboard.pasteLink') }}</span></button>
        <button class="rounded-full p-2 text-mute hover:bg-sunk hover:text-fg" :title="t('nav.logout')" :aria-label="t('nav.logout')" @click="session.logout()"><Icon name="logout" /></button>
      </div>
    </header>
    <main class="relative min-h-0 flex-1 overflow-y-auto" :aria-label="String(route.name ?? '')"><RouterView /></main>
  </div>
</template>
