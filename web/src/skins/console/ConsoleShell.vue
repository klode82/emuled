<!-- Modello A "Console": barra laterale (icone sotto 1024 px, barra in basso sul telefono), casella cerca/link in alto, barra di stato in fondo -->
<script setup lang="ts">
import { computed, ref } from 'vue'
import { useI18n } from 'vue-i18n'
import { RouterLink, RouterView, useRoute } from 'vue-router'
import { useUi } from '@/stores/ui'
import { SECTIONS } from '@/router'
import { useShellBar } from '@/lib/shell'
import { useSession } from '@/stores/session'
import { useShared } from '@/stores/shared'
import { number, speed } from '@/lib/format'
import Icon from '@/components/Icon.vue'

const { t } = useI18n()
const session = useSession(), route = useRoute()
const ui = useUi()
const { q, st, ed2k, kad, go, onPaste } = useShellBar()
const more = ref(false)
const shared = useShared()
const ups = computed(() => shared.uploads.uploading.length)
const linkBad = computed(() => session.link !== 'open')
</script>

<template>
  <div class="relative flex h-full flex-col overflow-hidden bg-bg md:flex-row">
    <aside class="hidden shrink-0 flex-col gap-1 border-r border-line bg-surface p-2 md:flex md:w-16 lg:w-56" :aria-label="t('nav.sections')">
      <div class="flex items-center gap-2 px-2 py-3">
        <img src="/assets/images/emuled_logo_transparent.png" alt="" class="size-8 shrink-0 object-contain" />
        <span class="hidden text-sm font-semibold lg:inline">emuled</span>
      </div>
      <template v-for="s in SECTIONS" :key="s.name">
        <RouterLink :to="s.path" :title="t('nav.' + s.name)" class="flex items-center gap-3 rounded-lg px-3 py-2 text-sm text-mute hover:bg-sunk hover:text-fg" :class="s.name === 'transfers' && route.path.startsWith('/transfers/') ? '!text-accent font-medium' : ''" active-class="!bg-soft !text-accent font-medium">
          <Icon :name="s.icon" class="!size-5" /><span class="hidden lg:inline">{{ t('nav.' + s.name) }}</span>
        </RouterLink>
        <!-- sottomenu di Trasferimenti: solo con la barra larga (sotto 1024 px ci sono le schede in cima alla pagina) -->
        <div v-if="s.name === 'transfers' && route.path.startsWith('/transfers')" class="ml-5 hidden flex-col gap-0.5 border-l border-line pl-2 lg:flex">
          <RouterLink to="/transfers" class="flex items-center gap-2 rounded-lg px-3 py-1.5 text-xs text-mute hover:bg-sunk hover:text-fg" exact-active-class="!bg-soft !text-accent font-medium"><Icon name="dl" class="!size-4" />{{ t('nav.transfersDownload') }}</RouterLink>
          <RouterLink to="/transfers/uploads" class="flex items-center gap-2 rounded-lg px-3 py-1.5 text-xs text-mute hover:bg-sunk hover:text-fg" exact-active-class="!bg-soft !text-accent font-medium"><Icon name="up" class="!size-4" />{{ t('nav.transfersUpload') }}<span v-if="ups" class="ml-auto tabular-nums text-[9px]">{{ ups }}</span></RouterLink>
        </div>
      </template>
      <div class="mt-auto flex flex-col gap-1">
        <p v-if="st" class="hidden px-3 py-1 text-xs text-mute lg:block">{{ st.nick }} · v{{ st.version.replace(/^.*?(\d)/, '$1') }}</p>
        <button class="flex items-center gap-3 rounded-lg px-3 py-2 text-sm text-mute hover:bg-sunk hover:text-fg" @click="session.logout()" :title="t('nav.logout')"><Icon name="logout" class="!size-5" /><span class="hidden lg:inline">{{ t('nav.logout') }}</span></button>
      </div>
    </aside>

    <div class="flex min-h-0 min-w-0 flex-1 flex-col">
      <div v-if="linkBad" class="flex items-center gap-2 border-b border-line bg-soft px-4 py-1.5 text-xs text-warn" role="status">
        <Icon name="wifi" />{{ t('shell.linkDown') }}
      </div>
      <header class="flex flex-wrap items-center gap-3 border-b border-line bg-surface px-4 py-3">
        <label class="relative min-w-0 flex-1 basis-56">
          <span class="sr-only">{{ t('shell.searchLabel') }}</span>
          <span class="pointer-events-none absolute left-3 top-1/2 -translate-y-1/2 text-mute"><Icon name="search" /></span>
          <input v-model="q" class="w-full rounded-lg border border-line bg-bg py-2 pl-9 pr-3 text-sm placeholder:text-mute" :placeholder="t('shell.searchPlaceholder')" @keydown.enter.prevent="go" @paste="onPaste" />
        </label>
        <button class="inline-flex items-center gap-1.5 rounded-lg bg-accent px-3.5 py-2 text-sm font-medium text-onaccent hover:opacity-90" @click="ui.openAdd()"><Icon name="plus" />{{ t('common.add') }}</button>
        <div class="hidden items-center gap-2 text-xs lg:flex">
          <span class="rounded-full bg-soft px-2.5 py-1 font-medium" :class="ed2k.tone">{{ ed2k.text }}</span>
          <span class="rounded-full bg-soft px-2.5 py-1 font-medium" :class="kad.tone">{{ kad.text }}</span>
        </div>
      </header>

      <main class="relative min-h-0 flex-1 overflow-y-auto" :aria-label="String(route.name ?? '')"><RouterView /></main>

      <footer class="flex flex-wrap items-center justify-between gap-x-4 gap-y-1 border-t border-line bg-surface px-4 py-2 text-xs text-mute tabular-nums">
        <span class="flex items-center gap-1"><Icon name="dl" class="!size-3.5" />{{ speed(st?.transfers.download_speed ?? 0) }}</span>
        <span class="flex items-center gap-1"><Icon name="up" class="!size-3.5" />{{ speed(st?.transfers.upload_speed ?? 0) }}</span>
        <span class="lg:hidden"><span :class="ed2k.tone">ed2k {{ st?.ed2k.connected ? '●' : '○' }}</span> · <span :class="kad.tone">Kad {{ st?.kad.connected ? '●' : '○' }}</span></span>
        <span class="hidden sm:inline">{{ t('shell.ports', { tcp: st?.ports.tcp ?? '—', udp: st?.ports.udp ?? '—' }) }}</span>
        <span>{{ t('shell.counters', { d: number(st?.transfers.downloads ?? 0), u: number(st?.transfers.uploading ?? 0) }) }}</span>
      </footer>
      <nav class="grid grid-cols-5 border-t border-line bg-surface md:hidden" :aria-label="t('nav.sections')">
        <RouterLink v-for="s in SECTIONS.slice(0, 4)" :key="s.name" :to="s.path" class="flex flex-col items-center gap-0.5 py-2 text-[11px] text-mute" active-class="!text-accent font-medium">
          <Icon :name="s.icon" class="!size-5" />{{ t('nav.' + s.name + 'Short') }}
        </RouterLink>
        <button class="flex flex-col items-center gap-0.5 py-2 text-[11px] text-mute" :class="{ '!text-accent font-medium': more || ['kad', 'stats', 'prefs'].includes(String(route.name)) }" :aria-expanded="more" @click="more = !more"><Icon name="gear" class="!size-5" />{{ t('nav.more') }}</button>
      </nav>
      <div v-if="more" class="fixed inset-0 z-30 md:hidden" @click="more = false">
        <div class="absolute inset-x-2 bottom-14 rounded-xl border border-line bg-surface p-2 shadow-xl" @click.stop>
          <RouterLink v-for="s in SECTIONS.slice(4)" :key="s.name" :to="s.path" class="flex items-center gap-3 rounded-lg px-3 py-3 text-sm hover:bg-sunk" @click="more = false"><Icon :name="s.icon" class="!size-5 text-mute" />{{ t('nav.' + s.name) }}</RouterLink>
          <button class="flex w-full items-center gap-3 rounded-lg px-3 py-3 text-left text-sm hover:bg-sunk" @click="more = false; session.logout()"><Icon name="logout" class="!size-5 text-mute" />{{ t('nav.logout') }}</button>
        </div>
      </div>
    </div>
  </div>
</template>
