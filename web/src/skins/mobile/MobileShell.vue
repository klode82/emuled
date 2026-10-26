<!-- Modello D "Mobile first": casella di ricerca grande in cima, barra in basso e pulsante + sul telefono, barra di icone a sinistra sullo schermo largo -->
<script setup lang="ts">
import { computed, ref } from 'vue'
import { useI18n } from 'vue-i18n'
import { RouterLink, RouterView, useRoute } from 'vue-router'
import { SECTIONS } from '@/router'
import { useSession } from '@/stores/session'
import { useUi } from '@/stores/ui'
import { useShellBar } from '@/lib/shell'
import Icon from '@/components/Icon.vue'

const { t } = useI18n()
const session = useSession(), ui = useUi(), route = useRoute()
const { q, go, onPaste } = useShellBar()
const more = ref(false)
const linkBad = computed(() => session.link !== 'open')
const SEG = [{ path: '/transfers', key: 'nav.transfersDownload' }, { path: '/transfers/uploads', key: 'nav.transfersUpload' }, { path: '/search', key: 'nav.search' }, { path: '/shared', key: 'nav.shared' }] as const
</script>
<template>
  <div class="relative flex h-full overflow-hidden bg-bg">
    <aside class="hidden w-20 shrink-0 flex-col items-center gap-2 border-r border-line bg-surface py-4 lg:flex" :aria-label="t('nav.sections')">
      <img src="/assets/images/emuled_logo_transparent.png" alt="" class="mb-3 size-10 object-contain" />
      <RouterLink v-for="s in SECTIONS" :key="s.name" :to="s.path" :title="t('nav.' + s.name)" :aria-label="t('nav.' + s.name)" class="grid size-11 place-items-center rounded-2xl text-mute hover:bg-sunk" active-class="!bg-soft !text-accent"><Icon :name="s.icon" class="!size-5" /></RouterLink>
      <button class="mt-auto grid size-11 place-items-center rounded-2xl text-mute hover:bg-sunk hover:text-fg" :title="t('nav.logout')" :aria-label="t('nav.logout')" @click="session.logout()"><Icon name="logout" class="!size-5" /></button>
    </aside>
    <div class="flex min-w-0 flex-1 flex-col">
      <div v-if="linkBad" class="flex shrink-0 items-center gap-2 border-b border-line bg-soft px-4 py-1.5 text-xs text-warn" role="status"><Icon name="wifi" />{{ t('shell.linkDown') }}</div>
      <div class="flex shrink-0 flex-col gap-3 px-4 pb-2 pt-4 md:px-6">
        <label class="relative"><span class="sr-only">{{ t('shell.searchLabel') }}</span>
          <span class="pointer-events-none absolute left-4 top-1/2 -translate-y-1/2 text-mute"><Icon name="search" class="!size-5" /></span>
          <input v-model="q" class="w-full rounded-full border border-line bg-surface py-3.5 pl-12 pr-4 text-[15px] shadow-sm placeholder:text-mute" :placeholder="t('skin.mobile.search')" @keydown.enter.prevent="go" @paste="onPaste" /></label>
        <nav class="flex gap-1 rounded-full bg-sunk p-1 text-sm font-semibold" :aria-label="t('nav.sections')">
          <RouterLink v-for="x in SEG" :key="x.path" :to="x.path" class="flex-1 rounded-full py-2 text-center text-mute" exact-active-class="!bg-surface !text-fg shadow-sm">{{ t(x.key) }}</RouterLink>
        </nav>
      </div>
      <main class="relative min-h-0 flex-1 overflow-y-auto" :aria-label="String(route.name ?? '')"><RouterView /></main>
      <nav class="grid shrink-0 grid-cols-5 border-t border-line bg-surface lg:hidden" :aria-label="t('nav.sections')">
        <RouterLink v-for="s in SECTIONS.slice(0, 4)" :key="s.name" :to="s.path" class="flex flex-col items-center gap-0.5 py-2.5 text-[11px] font-semibold text-mute" active-class="!text-accent"><Icon :name="s.icon" class="!size-5" />{{ t('nav.' + s.name + 'Short') }}</RouterLink>
        <button class="flex flex-col items-center gap-0.5 py-2.5 text-[11px] font-semibold text-mute" :class="{ '!text-accent': more || ['kad', 'log', 'stats', 'prefs'].includes(String(route.name)) }" :aria-expanded="more" @click="more = !more"><Icon name="gear" class="!size-5" />{{ t('nav.more') }}</button>
      </nav>
    </div>
    <button class="absolute bottom-20 right-4 z-10 grid size-14 place-items-center rounded-full bg-accent text-onaccent shadow-lg lg:hidden" :aria-label="t('common.add')" @click="ui.openAdd()"><Icon name="plus" class="!size-6" /></button>
    <div v-if="more" class="fixed inset-0 z-30 lg:hidden" @click="more = false">
      <div class="absolute inset-x-2 bottom-16 rounded-2xl border border-line bg-surface p-2 shadow-xl" @click.stop>
        <RouterLink v-for="s in SECTIONS.slice(4)" :key="s.name" :to="s.path" class="flex items-center gap-3 rounded-xl px-3 py-3 text-sm hover:bg-sunk" @click="more = false"><Icon :name="s.icon" class="!size-5 text-mute" />{{ t('nav.' + s.name) }}</RouterLink>
        <button class="flex w-full items-center gap-3 rounded-xl px-3 py-3 text-left text-sm hover:bg-sunk" @click="more = false; session.logout()"><Icon name="logout" class="!size-5 text-mute" />{{ t('nav.logout') }}</button>
      </div>
    </div>
  </div>
</template>
