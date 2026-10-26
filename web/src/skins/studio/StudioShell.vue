<!-- Modello C "Studio": sezioni a schede in alto con la casella comandi (Ctrl+K), elenco + dettaglio, barra di stato a caratteri fissi in fondo -->
<script setup lang="ts">
import { computed, onMounted, onUnmounted, ref } from 'vue'
import { useI18n } from 'vue-i18n'
import { RouterLink, RouterView, useRoute } from 'vue-router'
import { SECTIONS } from '@/router'
import { useSession } from '@/stores/session'
import { useUi } from '@/stores/ui'
import { useShellBar } from '@/lib/shell'
import { number, speed } from '@/lib/format'
import Icon from '@/components/Icon.vue'

const { t } = useI18n()
const session = useSession(), ui = useUi(), route = useRoute()
const { q, st, ed2k, kad, go, onPaste } = useShellBar()
const box = ref<HTMLInputElement | null>(null)
const linkBad = computed(() => session.link !== 'open')
function key(e: KeyboardEvent) { if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === 'k') { e.preventDefault(); box.value?.focus(); box.value?.select() } }
onMounted(() => window.addEventListener('keydown', key))
onUnmounted(() => window.removeEventListener('keydown', key))
</script>
<template>
  <div class="flex h-full flex-col overflow-hidden bg-bg">
    <div v-if="linkBad" class="flex shrink-0 items-center gap-2 border-b border-line bg-soft px-4 py-1.5 text-xs text-warn" role="status"><Icon name="wifi" />{{ t('shell.linkDown') }}</div>
    <header class="flex shrink-0 flex-wrap items-center gap-x-4 gap-y-2 border-b border-line bg-surface px-4 py-2">
      <img src="/assets/images/emuled_logo_transparent.png" alt="" class="size-7 shrink-0 object-contain" />
      <nav class="flex min-w-0 basis-[calc(100%-3.5rem)] gap-1 overflow-x-auto text-sm sm:flex-1 sm:basis-0" :aria-label="t('nav.sections')">
        <RouterLink v-for="s in SECTIONS" :key="s.name" :to="s.path" class="whitespace-nowrap border-b-2 border-transparent px-3 py-2 text-mute hover:text-fg" active-class="!border-accent font-semibold !text-fg">{{ t('nav.' + s.name) }}</RouterLink>
      </nav>
      <label class="relative flex min-w-0 flex-1 basis-32 items-center sm:w-80 sm:flex-none">
        <span class="sr-only">{{ t('shell.searchLabel') }}</span>
        <span class="pointer-events-none absolute left-3 text-mute"><Icon name="search" class="!size-3.5" /></span>
        <input ref="box" v-model="q" class="w-full rounded-md border border-line bg-bg py-1.5 pl-8 pr-14 text-sm placeholder:text-mute hover:border-accent" :placeholder="t('skin.studio.search')" @keydown.enter.prevent="go" @paste="onPaste" />
        <kbd class="pointer-events-none absolute right-2 rounded border border-line px-1.5 font-mono text-[10px] text-mute">Ctrl K</kbd>
      </label>
      <button class="inline-flex items-center gap-1.5 rounded-md bg-accent px-3 py-1.5 text-sm font-semibold text-onaccent hover:opacity-90" @click="ui.openAdd()"><Icon name="plus" />{{ t('common.add') }}</button>
      <button class="rounded-md p-2 text-mute hover:bg-sunk hover:text-fg" :title="t('nav.logout')" :aria-label="t('nav.logout')" @click="session.logout()"><Icon name="logout" /></button>
    </header>
    <main class="relative min-h-0 flex-1 overflow-y-auto" :aria-label="String(route.name ?? '')"><RouterView /></main>
    <footer class="flex shrink-0 flex-wrap items-center gap-x-4 gap-y-1 border-t border-line bg-surface px-4 py-1.5 font-mono text-[11px] text-mute tabular-nums">
      <span :class="ed2k.tone">● {{ ed2k.text }}</span><span :class="kad.tone">● {{ kad.text }}</span>
      <span class="hidden sm:inline">{{ t('shell.ports', { tcp: st?.ports.tcp ?? '—', udp: st?.ports.udp ?? '—' }) }}</span>
      <span class="ml-auto">↓ {{ speed(st?.transfers.download_speed ?? 0) }}</span><span>↑ {{ speed(st?.transfers.upload_speed ?? 0) }}</span>
      <span class="hidden md:inline">{{ t('shell.counters', { d: number(st?.transfers.downloads ?? 0), u: number(st?.transfers.uploading ?? 0) }) }}</span>
    </footer>
  </div>
</template>
