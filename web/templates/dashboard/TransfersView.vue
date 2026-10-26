<!-- Modello B: schermata Trasferimenti. Tessere di riepilogo (velocita', server, Kad) e una scheda per ogni download; il dettaglio si apre a lato -->
<script setup lang="ts">
import { computed, onMounted } from 'vue'
import { useI18n } from 'vue-i18n'
import { FILTERS, useDownloads } from '@/stores/downloads'
import { useCore } from '@/stores/core'
import { usePrefs } from '@/stores/prefs'
import { useUi } from '@/stores/ui'
import { useTransferActions } from '@/lib/transferActions'
import { bytes, eta, number, speed } from '@/lib/format'
import { statusInfo, toneClass } from '@/lib/labels'
import Icon from '@/components/Icon.vue'
import Ring from '@/components/Ring.vue'
import Sparkline from '@/components/Sparkline.vue'
import EmptyState from '@/components/EmptyState.vue'
import DownloadDetail from '@/components/DownloadDetail.vue'
import TransfersSeg from '@/components/TransfersSeg.vue'

const { t } = useI18n()
const dl = useDownloads(), core = useCore(), prefs = usePrefs(), ui = useUi(), act = useTransferActions()
onMounted(() => { void prefs.loadCategories() })
const st = computed(() => core.status)
const catName = (id: number) => (id ? prefs.categories.find((c) => c.id === id)?.title : undefined)
const ed2kValue = computed(() => { const e = st.value?.ed2k; return !e ? '…' : e.connected ? (e.server?.name || t('shell.connected')) : t(e.connecting ? 'shell.connecting' : 'shell.disconnected') })
const ed2kSub = computed(() => { const e = st.value?.ed2k; return e?.connected ? [t(e.high_id ? 'shell.highId' : 'shell.lowId'), e.server?.users ? t('skin.dashboard.users', { n: number(e.server.users) }) : ''].filter(Boolean).join(' · ') : '' })
const kadValue = computed(() => { const k = st.value?.kad; return !k?.running ? t('shell.off') : t(k.connected ? 'shell.connected' : 'shell.connecting') })
const kadSub = computed(() => { const k = st.value?.kad; return k?.running ? [k.contacts ? t('skin.dashboard.contacts', { n: number(k.contacts) }) : '', k.udp_verified ? t('skin.dashboard.udpOpen') : ''].filter(Boolean).join(' · ') : '' })
function pick(hash: string, e: MouseEvent) {
  if ((e.target as HTMLElement).closest('button,a,input,select')) return
  dl.detail = dl.detail === hash ? null : hash
}
</script>
<template>
  <div class="flex flex-col gap-5 p-4 md:p-6" tabindex="-1" @keydown.esc="dl.detail = null">
    <section class="grid grid-cols-2 gap-3 lg:grid-cols-4" :aria-label="t('skin.dashboard.summary')">
      <div class="rounded-2xl border border-line bg-surface p-4">
        <p class="text-xs font-semibold uppercase tracking-wide text-mute">{{ t('skin.dashboard.download') }}</p>
        <p class="pt-1 text-2xl font-bold tracking-tight tabular-nums">{{ speed(st?.transfers.download_speed ?? 0) }}</p>
        <p class="text-xs text-mute">{{ t('skin.dashboard.activeFiles', dl.counts.active) }}</p><Sparkline :samples="core.samples" kind="down" />
      </div>
      <div class="rounded-2xl border border-line bg-surface p-4">
        <p class="text-xs font-semibold uppercase tracking-wide text-mute">{{ t('skin.dashboard.upload') }}</p>
        <p class="pt-1 text-2xl font-bold tracking-tight tabular-nums">{{ speed(st?.transfers.upload_speed ?? 0) }}</p>
        <p class="text-xs text-mute">{{ t('skin.dashboard.served', st?.transfers.uploading ?? 0) }}</p><Sparkline :samples="core.samples" kind="up" />
      </div>
      <div class="rounded-2xl border border-line bg-surface p-4">
        <p class="text-xs font-semibold uppercase tracking-wide text-mute">{{ t('skin.dashboard.ed2k') }}</p>
        <p class="truncate pt-1 text-2xl font-bold tracking-tight" :title="ed2kValue">{{ ed2kValue }}</p><p class="truncate text-xs text-mute">{{ ed2kSub || '—' }}</p>
      </div>
      <div class="rounded-2xl border border-line bg-surface p-4">
        <p class="text-xs font-semibold uppercase tracking-wide text-mute">Kad</p>
        <p class="truncate pt-1 text-2xl font-bold tracking-tight">{{ kadValue }}</p><p class="truncate text-xs text-mute">{{ kadSub || '—' }}</p>
      </div>
    </section>

    <section>
      <div class="flex flex-wrap items-center gap-x-3 gap-y-2 pb-3">
        <h2 class="text-base font-bold">{{ t('skin.dashboard.inProgress') }}</h2><TransfersSeg />
        <div class="ml-auto flex flex-wrap items-center gap-2">
          <button v-if="dl.counts.done" class="rounded-full border border-line px-3 py-1.5 text-xs font-semibold hover:bg-sunk" :title="t('transfers.clearCompletedHint')" @click="dl.clearCompleted()">{{ t('transfers.clearCompleted') }}</button>
          <button class="inline-flex items-center gap-1.5 rounded-full border border-line px-3 py-1.5 text-xs font-semibold hover:bg-sunk" :title="t('transfers.findAllHint')" @click="dl.findAllSources()"><Icon name="refresh" class="!size-3.5" />{{ t('transfers.findAll') }}</button>
          <button class="inline-flex items-center gap-1.5 rounded-full bg-accent px-4 py-2 text-sm font-bold text-onaccent hover:opacity-90" @click="ui.openAdd()"><Icon name="plus" />{{ t('common.add') }}</button>
        </div>
      </div>
      <div class="flex flex-wrap items-center gap-2 pb-3">
        <div class="flex items-center gap-1 overflow-x-auto text-xs" role="tablist" :aria-label="t('transfers.filter')">
          <button v-for="f in FILTERS" :key="f.key" role="tab" :aria-selected="dl.filter === f.key" class="whitespace-nowrap rounded-full px-3 py-1.5 font-semibold" :class="dl.filter === f.key ? 'bg-soft text-accent' : 'text-mute hover:bg-sunk'" @click="dl.filter = f.key">
            {{ t('filters.' + f.key) }} <span class="tabular-nums">{{ dl.counts[f.key] }}</span></button>
        </div>
        <select v-if="prefs.categories.length > 1" class="rounded-full border border-line bg-surface px-3 py-1.5 text-xs" :value="dl.category ?? ''" :aria-label="t('common.category')" @change="dl.category = ($event.target as HTMLSelectElement).value === '' ? null : Number(($event.target as HTMLSelectElement).value)">
          <option value="">{{ t('transfers.allCategories') }}</option><option v-for="c in prefs.categories.filter((x) => x.id > 0)" :key="c.id" :value="c.id">{{ c.title }}</option></select>
        <input v-model="dl.query" type="search" class="min-w-0 flex-1 basis-40 rounded-full border border-line bg-surface px-4 py-1.5 text-xs placeholder:text-mute md:max-w-xs" :placeholder="t('transfers.filterByName')" :aria-label="t('transfers.filterByName')" />
      </div>

      <p v-if="!dl.loaded" class="py-8 text-center text-xs text-mute" role="status">{{ t('transfers.loading') }}</p>
      <EmptyState v-else-if="!dl.all.length" icon="dl" :title="t('transfers.emptyTitle')" :text="t('transfers.emptyText')">
        <button class="inline-flex items-center gap-1.5 rounded-full bg-accent px-4 py-2 text-xs font-bold text-onaccent" @click="ui.openAdd()"><Icon name="plus" />{{ t('transfers.addLink') }}</button>
      </EmptyState>
      <EmptyState v-else-if="!dl.list.length" icon="search" :title="t('transfers.noMatchTitle')" :text="t('transfers.noMatchText')">
        <button class="rounded-full border border-line px-4 py-2 text-xs hover:bg-sunk" @click="dl.filter = 'all'; dl.category = null; dl.query = ''">{{ t('transfers.showAll') }}</button>
      </EmptyState>
      <div v-else class="grid gap-3 md:grid-cols-2 xl:grid-cols-3">
        <article v-for="d in dl.list" :key="d.hash" class="flex cursor-pointer flex-col gap-3 rounded-2xl border bg-surface p-4 hover:border-accent" :class="dl.detail === d.hash ? 'border-accent' : 'border-line'" @click="pick(d.hash, $event)">
          <div class="flex items-start gap-3">
            <Ring :value="d.percent" :size="52" :tone="statusInfo(d).key === 'done' ? 'ok' : 'accent'" />
            <div class="min-w-0 flex-1"><h3 class="line-clamp-2 break-words text-sm font-semibold leading-snug" :title="d.name">{{ d.name }}</h3>
              <p class="pt-0.5 text-xs text-mute tabular-nums">{{ bytes(d.size, 2, true) }}<template v-if="catName(d.category)"> · {{ catName(d.category) }}</template></p></div>
            <button class="rounded-lg p-1.5 text-mute hover:bg-sunk hover:text-fg" :title="t('skin.dashboard.details')" :aria-label="t('skin.dashboard.details')" @click="dl.detail = d.hash"><Icon name="info" /></button>
          </div>
          <div class="grid grid-cols-3 gap-2 rounded-xl bg-sunk p-2 text-center text-xs">
            <div><p class="font-semibold tabular-nums">{{ speed(d.speed) }}</p><p class="text-mute">{{ t('detail.speed') }}</p></div>
            <div><p class="font-semibold tabular-nums">{{ d.sources_transferring }}/{{ d.sources }}</p><p class="text-mute">{{ t('cols.sources') }}</p></div>
            <div><p class="font-semibold tabular-nums">{{ eta(d.size, d.completed, d.speed) }}</p><p class="text-mute">{{ t('cols.eta') }}</p></div>
          </div>
          <div class="flex items-center justify-between">
            <span class="rounded-full bg-sunk px-2.5 py-1 text-xs font-semibold" :class="toneClass(statusInfo(d).tone)">{{ t(statusInfo(d).labelKey) }}</span>
            <div class="flex gap-1">
              <button v-if="statusInfo(d).key !== 'done'" class="rounded-lg border border-line p-2 text-mute hover:text-fg" :title="act.paused(d) ? t('common.resume') : t('common.pause')" :aria-label="act.paused(d) ? t('common.resume') : t('common.pause')" @click="act.toggle(d)"><Icon :name="act.paused(d) ? 'play' : 'pause'" /></button>
              <button class="rounded-lg border border-line p-2 text-mute hover:text-bad" :title="statusInfo(d).key === 'done' ? t('transfers.removeFromList') : t('common.cancelDownload')" :aria-label="statusInfo(d).key === 'done' ? t('transfers.removeFromList') : t('common.cancelDownload')" @click="act.cancel([d])"><Icon :name="statusInfo(d).key === 'done' ? 'x' : 'trash'" /></button>
            </div>
          </div>
        </article>
      </div>
    </section>

    <template v-if="dl.detail">
      <div class="fixed inset-0 z-20 bg-black/40" @click="dl.detail = null"></div>
      <aside class="fixed inset-y-0 right-0 z-30 w-full max-w-md border-l border-line bg-surface shadow-2xl" :aria-label="t('detail.aria')"><DownloadDetail :hash="dl.detail" @close="dl.detail = null" /></aside>
    </template>
  </div>
</template>
