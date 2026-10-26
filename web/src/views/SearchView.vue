<script setup lang="ts">
import { computed, onMounted, ref, watch } from 'vue'
import { useSort } from '@/lib/useSort'
import SortTh from '@/components/SortTh.vue'
import { useI18n } from 'vue-i18n'
import { useSearch } from '@/stores/search'
import { useDownloads } from '@/stores/downloads'
import { useCore } from '@/stores/core'
import { usePrefs } from '@/stores/prefs'
import { bytes, number } from '@/lib/format'
import type { SearchScope } from '@/types'
import Icon from '@/components/Icon.vue'
import EmptyState from '@/components/EmptyState.vue'

const { t } = useI18n()
const s = useSearch(), dl = useDownloads(), core = useCore(), prefs = usePrefs()
const advanced = ref(false)
const SCOPES: SearchScope[] = ['global', 'server', 'all', 'kad']
onMounted(() => { void s.load(); void prefs.loadCategories() })
// scelta multipla: spunta i file da scaricare insieme (quelli gia' in lista non si possono scegliere)
const chosen = ref<Set<string>>(new Set()), bulkCat = ref(0), bulkBusy = ref(false)
const pickable = computed(() => results.value.filter((r) => !dl.items[r.hash]))
const allChosen = computed(() => pickable.value.length > 0 && pickable.value.every((r) => chosen.value.has(r.hash)))
const chosenRows = computed(() => pickable.value.filter((r) => chosen.value.has(r.hash)))
function toggleChosen(h: string) { const n = new Set(chosen.value); if (n.has(h)) n.delete(h); else n.add(h); chosen.value = n }
function chooseAll(on: boolean) { chosen.value = on ? new Set(pickable.value.map((r) => r.hash)) : new Set() }
watch(() => s.current?.id, () => { chosen.value = new Set() })
async function downloadChosen() {
  bulkBusy.value = true
  const n = await dl.addFoundMany(chosenRows.value, bulkCat.value)
  bulkBusy.value = false
  if (n) chosen.value = new Set()
}
const TYPES = ['', 'Audio', 'Video', 'Image', 'Doc', 'Pro', 'Arc', 'Iso']
const running = computed(() => s.current?.state === 'running')
const results = computed(() => s.current?.results ?? [])
const minMb = computed({ get: () => (s.form.min_size ? s.form.min_size / 1048576 : ''), set: (v: number | '') => { s.form.min_size = v ? Math.round(Number(v) * 1048576) : undefined } })
const offline = computed(() => { const k = !core.status?.kad.connected, e = !core.status?.ed2k.connected; return s.form.scope === 'kad' ? k : s.form.scope === 'global' ? k && e : e })
// percentuale di fonti complete (hanno tutto il file): la rete Kad non lo dice, quindi per le ricerche su Kad la colonna e' vuota
const pct = (r: { sources: number; complete_sources: number }) => (r.sources ? Math.round((100 * r.complete_sources) / r.sources) : 0)
const knowsComplete = computed(() => s.current?.scope !== 'kad')
const scopeShort = (x: SearchScope) => t('search.scope.' + x)
const { sorted, sorter } = useSort(() => results.value, {
  name: { get: (r) => r.name }, size: { get: (r) => r.size, desc: true }, sources: { get: (r) => r.sources, desc: true }, complete: { get: (r) => (knowsComplete.value ? pct(r) : 0), desc: true }, type: { get: (r) => r.type || '' },
}, 'sources', -1, 'search')
const have = (hash: string) => !!dl.items[hash]
</script>
<template>
  <div class="flex flex-col gap-4 p-4">
    <h1 class="text-sm font-semibold">{{ t('nav.search') }}</h1>
    <form class="flex flex-col gap-3 rounded-xl border border-line bg-surface p-4" @submit.prevent="s.start()">
      <div class="flex flex-wrap gap-2">
        <label class="min-w-0 flex-1 basis-64"><span class="sr-only">{{ t('search.what') }}</span>
          <input v-model="s.form.query" class="w-full rounded-lg border border-line bg-bg px-3 py-2.5 text-xs placeholder:text-mute" :placeholder="t('search.placeholder')" autofocus /></label>
        <label><span class="sr-only">{{ t('search.where') }}</span>
          <select v-model="s.form.scope" class="rounded-lg border border-line bg-bg px-3 py-2.5 text-xs"><option v-for="x in SCOPES" :key="x" :value="x">{{ t('search.scope.' + x) }}</option></select></label>
        <button class="inline-flex items-center gap-1.5 rounded-lg bg-accent px-3 py-2 text-xs font-medium text-onaccent" :disabled="!s.form.query.trim() || s.starting || running"><Icon name="search" />{{ t('search.go') }}</button>
        <button v-if="running" type="button" class="rounded-lg border border-line px-3 py-2 text-xs hover:bg-sunk" @click="s.cancel()">{{ t('search.stop') }}</button>
      </div>
      <button type="button" class="flex w-fit items-center gap-1 text-[9px] text-mute hover:text-fg" :aria-expanded="advanced" @click="advanced = !advanced"><Icon name="chevron" class="!size-3.5 transition-transform" :class="advanced ? 'rotate-90' : ''" />{{ t('search.filters') }}</button>
      <div v-if="advanced" class="grid gap-3 sm:grid-cols-2 lg:grid-cols-4">
        <label class="flex flex-col gap-1 text-[9px] text-mute">{{ t('search.type') }}<select v-model="s.form.type" class="rounded-lg border border-line bg-bg px-2.5 py-2 text-xs text-fg"><option v-for="x in TYPES" :key="x" :value="x">{{ t('search.types.' + (x || 'any')) }}</option></select></label>
        <label class="flex flex-col gap-1 text-[9px] text-mute">{{ t('search.ext') }}<input v-model="s.form.ext" class="rounded-lg border border-line bg-bg px-2.5 py-2 text-xs text-fg" placeholder="pdf" /></label>
        <label class="flex flex-col gap-1 text-[9px] text-mute">{{ t('search.minSize') }}<input v-model.number="minMb" type="number" min="0" class="rounded-lg border border-line bg-bg px-2.5 py-2 text-xs text-fg" /></label>
        <label class="flex flex-col gap-1 text-[9px] text-mute">{{ t('search.minSources') }}<input v-model.number="s.form.min_sources" type="number" min="0" class="rounded-lg border border-line bg-bg px-2.5 py-2 text-xs text-fg" /></label>
      </div>
      <p v-if="offline" class="text-[9px] text-warn" role="status">{{ s.form.scope === 'kad' ? t('search.kadOffline') : t('search.ed2kOffline') }}</p>
    </form>

    <div v-if="s.list.length" class="flex flex-wrap gap-1.5" role="tablist" :aria-label="t('search.history')" :title="t('search.historyHint')">
      <span v-for="x in s.list" :key="x.id" class="inline-flex max-w-[16rem] items-center rounded-full border text-xs" :class="s.current?.id === x.id ? 'border-accent bg-soft text-accent' : 'border-line text-mute hover:bg-sunk'">
        <button role="tab" :aria-selected="s.current?.id === x.id" class="min-w-0 truncate py-1 pl-3 pr-1" :title="scopeShort(x.scope)" @click="s.select(x.id)">{{ x.query }} <span class="tabular-nums">· {{ number(x.result_count) }}</span></button>
        <button class="rounded-full px-2 py-1 hover:text-fg" :aria-label="t('search.closeSearch', { q: x.query })" @click="s.remove(x.id)">×</button></span>
    </div>
    <div v-if="s.current" class="flex flex-wrap items-center gap-3">
    <button class="inline-flex items-center gap-1.5 rounded-lg border border-line px-3 py-1.5 text-xs hover:bg-soft hover:text-accent disabled:opacity-50" :disabled="running" :title="t('search.refreshHint')" @click="s.refresh()"><Icon name="refresh" class="!size-3.5" />{{ t('search.refresh') }}</button>
    <p class="text-xs text-mute tabular-nums" role="status">
      <template v-if="running">{{ t('search.running', { q: s.current.query, n: number(s.current.result_count), s: s.current.elapsed_s }) }}<template v-if="s.current.servers_asked !== undefined"> · {{ t('search.serversAsked', s.current.servers_asked) }}</template></template>
      <template v-else>{{ t(s.current.state === 'cancelled' ? 'search.cancelled' : 'search.finished', { n: number(results.length), q: s.current.query }) }}</template>
    </p>
    </div>
    <EmptyState v-if="!s.current" icon="search" :title="t('search.emptyTitle')" :text="t('search.emptyText')" />
    <EmptyState v-else-if="!running && !results.length" icon="search" :title="t('search.noResultsTitle')" :text="t('search.noResultsText')" />
    <div v-else-if="results.length" class="flex flex-col gap-2">
    <div v-if="chosenRows.length" class="flex flex-wrap items-center gap-3 rounded-xl border border-line bg-soft px-4 py-2 text-xs" role="status">
      <span class="tabular-nums">{{ t('search.chosen', chosenRows.length) }}</span>
      <label class="flex items-center gap-2 text-mute">{{ t('common.category') }}
        <select v-model.number="bulkCat" class="rounded-md border border-line bg-bg px-2 py-1 text-xs text-fg"><option :value="0">{{ t('catmenu.noCategory') }}</option><option v-for="c in prefs.categories.filter((x) => x.id > 0)" :key="c.id" :value="c.id">{{ c.title }}</option></select></label>
      <button class="inline-flex items-center gap-1.5 rounded-lg bg-accent px-3 py-1.5 text-xs font-medium text-onaccent disabled:opacity-50" :disabled="bulkBusy" @click="downloadChosen"><Icon name="dl" class="!size-3.5" />{{ t('search.downloadChosen', chosenRows.length) }}</button>
      <button class="rounded-lg border border-line px-3 py-1.5 text-xs hover:bg-surface" @click="chooseAll(false)">{{ t('search.clearChoice') }}</button>
    </div>
    <div class="overflow-x-auto rounded-xl border border-line bg-surface">
      <table class="w-full min-w-[36rem] text-left text-xs">
        <thead class="bg-sunk text-[9px] uppercase tracking-wide text-mute"><tr><th class="w-8 px-3 py-2"><input type="checkbox" :checked="allChosen" :disabled="!pickable.length" :aria-label="t('search.chooseAll')" @change="chooseAll(($event.target as HTMLInputElement).checked)" /></th><SortTh :label="t('cols.name')" col="name" :sorter="sorter" /><SortTh :label="t('cols.size')" col="size" :sorter="sorter" right /><SortTh :label="t('cols.sources')" col="sources" :sorter="sorter" right /><SortTh :label="t('cols.complete')" col="complete" :sorter="sorter" right /><SortTh :label="t('cols.type')" col="type" :sorter="sorter" /><th class="px-4 py-2"><span class="sr-only">{{ t('common.actions') }}</span></th></tr></thead>
        <tbody>
          <tr v-for="r in sorted" :key="r.hash" class="border-t border-line hover:bg-sunk">
            <td class="px-3 py-2"><input v-if="!have(r.hash)" type="checkbox" :checked="chosen.has(r.hash)" :aria-label="t('search.choose', { name: r.name })" @change="toggleChosen(r.hash)" /></td>
            <td class="w-[58%] px-4 py-2"><p class="line-clamp-2 break-words font-medium leading-snug" :title="r.name">{{ r.name }}</p></td>
            <td class="px-3 py-2 text-right tabular-nums">{{ bytes(r.size) }}</td>
            <td class="px-3 py-2 text-right tabular-nums">{{ number(r.sources) }}</td>
            <td class="px-3 py-2 text-right tabular-nums" :title="knowsComplete ? t('search.completeTitle', { c: number(r.complete_sources), n: number(r.sources) }) : t('search.completeKad')"
              :class="!knowsComplete ? 'text-mute' : pct(r) >= 50 ? 'font-medium text-ok' : pct(r) > 0 ? 'text-warn' : 'text-mute'">{{ knowsComplete ? pct(r) + '%' : '—' }}</td>
            <td class="px-3 py-2 text-mute">{{ r.type || '—' }}</td>
            <td class="px-3 py-2 text-right"><span v-if="have(r.hash)" class="inline-flex items-center gap-2"><span class="text-[9px] text-ok">{{ t('search.already') }}</span>
              <button class="inline-flex items-center gap-1 rounded-lg border border-line px-2 py-1 text-[9px] hover:bg-soft hover:text-accent" :title="t('search.findSourcesHint')" @click="dl.findSources(r.hash)"><Icon name="refresh" class="!size-3" />{{ t('search.findSources') }}</button></span>
              <button v-else class="inline-flex items-center gap-1.5 rounded-lg border border-line px-3 py-1.5 text-[9px] hover:bg-soft hover:text-accent" @click="dl.addFound(r)"><Icon name="dl" class="!size-3.5" />{{ t('search.download') }}</button></td>
          </tr>
        </tbody>
      </table>
    </div>
    </div>
  </div>
</template>
