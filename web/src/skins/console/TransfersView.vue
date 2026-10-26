<!-- Modello A: schermata Trasferimenti. Tabella ordinabile (lg), righe a scheda (telefono), selezione multipla, dettaglio a lato -->
<script setup lang="ts">
import { computed, onMounted, ref } from 'vue'
import { useI18n } from 'vue-i18n'
import { FILTERS, useDownloads, type SortKey } from '@/stores/downloads'
import { usePrefs } from '@/stores/prefs'
import { useUi } from '@/stores/ui'
import { bytes, eta, percent, speed } from '@/lib/format'
import { statusInfo, toneClass } from '@/lib/labels'
import Icon from '@/components/Icon.vue'
import ProgressBar from '@/components/ProgressBar.vue'
import EmptyState from '@/components/EmptyState.vue'
import DownloadDetail from '@/components/DownloadDetail.vue'
import TransfersTabs from '@/components/TransfersTabs.vue'
import StatusIcon from '@/components/StatusIcon.vue'
import CategoryDialog from '@/components/CategoryDialog.vue'
import type { Category } from '@/types'

const { t } = useI18n()
const dl = useDownloads(), prefs = usePrefs(), ui = useUi()
onMounted(() => { void prefs.loadCategories() })

const COLS: { key: SortKey; right: boolean }[] = [
  { key: 'name', right: false }, { key: 'size', right: true }, { key: 'percent', right: false }, { key: 'speed', right: true }, { key: 'eta', right: true }, { key: 'sources', right: true }, { key: 'status', right: false },
]
// con il dettaglio aperto la tabella e' stretta: restano nome, avanzamento e stato
const compact = computed(() => dl.detail !== null)
const GRID = computed(() => (compact.value ? 'lg:grid-cols-[1.75rem_minmax(0,1fr)_7rem_3.5rem]' : 'lg:grid-cols-[1.75rem_minmax(0,1fr)_6rem_9rem_6rem_5.5rem_4rem_3.5rem]'))
const allOn = computed(() => dl.list.length > 0 && dl.list.every((d) => dl.selected.has(d.hash)))
const sel = computed(() => dl.selectedItems)
const anyPaused = computed(() => sel.value.some((d) => statusInfo(d).key === 'paused'))
const anyRunning = computed(() => sel.value.some((d) => ['active', 'waiting'].includes(statusInfo(d).key)))
const allDone = computed(() => sel.value.length > 0 && sel.value.every((d) => statusInfo(d).key === 'done'))
const anyDone = computed(() => dl.counts.done > 0)
const catName = (id: number) => prefs.categories.find((c) => c.id === id)?.title

// ---- menu "Opzioni categorie": sposta i file scelti, azioni su tutti i file in vista, gestione delle categorie (come il menu delle schede categoria di eMule)
const menu = ref(false)
const dlg = ref<{ open: boolean; category: Category | null; move: boolean }>({ open: false, category: null, move: false })
const CAT_PRIO = [4, 0, 1, 2, 3, 5]
const real = computed(() => prefs.categories.filter((c) => c.id > 0))
const viewCat = computed(() => (dl.category === null ? null : prefs.categories.find((c) => c.id === dl.category) ?? null))
const inView = computed(() => dl.list.filter((d) => statusInfo(d).key !== 'done'))
const hashes = (l: { hash: string }[]) => l.map((d) => d.hash)
function closeAnd(fn: () => void | Promise<unknown>) { menu.value = false; void fn() }
function openDlg(category: Category | null, move = false) { menu.value = false; dlg.value = { open: true, category, move } }
function onSaved(id: number) { if (dlg.value.move) void dl.setCategoryMany(hashes(sel.value), id) }
function resumeNext() { const d = inView.value.filter((x) => statusInfo(x).key === 'paused').sort((a, b) => a.name.localeCompare(b.name))[0]; if (d) void dl.resume([d.hash]) }
const listItem = (d: { name: string; size: number; percent: number }) => ({ label: d.name, info: `${bytes(d.size, 2, true)} · ${percent(d.percent)}` })
async function cancelView() {
  const n = inView.value.length
  if (!n) return
  if (await ui.ask({ title: t('transfers.cancelTitle', n), text: t('transfers.cancelText'), confirm: t('common.cancelDownload'), danger: true, list: inView.value.map(listItem) })) await dl.remove(hashes(inView.value))
}
async function delCat() {
  const c = viewCat.value
  if (!c) return
  if (await ui.ask({ title: t('prefs.catDeleteTitle'), text: t('prefs.catDeleteText', { name: c.title }), confirm: t('common.remove'), danger: true })) { await prefs.removeCategory(c.id); dl.category = null }
}

async function cancelSel() {
  const n = sel.value.length
  if (!n) return
  if (allDone.value) { await dl.remove(sel.value.map((d) => d.hash)); return }	// togliere dalla lista dei completati non cancella nulla: niente conferma
  const ok = await ui.ask({ title: t('transfers.cancelTitle', n), text: t('transfers.cancelText'), confirm: t('common.cancelDownload'), danger: true, list: sel.value.map(listItem) })
  if (ok) await dl.remove(sel.value.map((d) => d.hash))
}
function key(e: KeyboardEvent) {
  const el = e.target as HTMLElement
  if (['INPUT', 'SELECT', 'TEXTAREA'].includes(el.tagName)) return
  if (e.key === 'Delete' && sel.value.length) { e.preventDefault(); void cancelSel() }
  if ((e.ctrlKey || e.metaKey) && e.key === 'a') { e.preventDefault(); dl.selectAll(true) }
  if (e.key === 'Escape') { dl.detail ? (dl.detail = null) : dl.selectAll(false) }
}
function pick(hash: string, e: MouseEvent) {
  if ((e.target as HTMLElement).closest('input,button,a')) return
  dl.detail = dl.detail === hash ? null : hash
}
</script>

<template>
  <div class="flex h-full min-h-0" tabindex="-1" @keydown="key">
    <div class="flex min-h-0 min-w-0 flex-1 flex-col">
      <TransfersTabs />
      <div class="flex flex-wrap items-center gap-x-2 gap-y-2 border-b border-line bg-surface px-4 py-2">
        <div class="flex items-center gap-1 overflow-x-auto text-xs" role="tablist" :aria-label="t('transfers.filter')">
          <button v-for="f in FILTERS" :key="f.key" role="tab" :aria-selected="dl.filter === f.key" class="whitespace-nowrap rounded-md px-3 py-1.5" :class="dl.filter === f.key ? 'bg-sunk font-medium' : 'text-mute hover:bg-sunk'" @click="dl.filter = f.key">
            {{ t('filters.' + f.key) }} <span class="tabular-nums text-mute">{{ dl.counts[f.key] }}</span></button>
        </div>
        <select v-if="prefs.categories.length > 1" class="rounded-md border border-line bg-bg px-2 py-1.5 text-xs" :value="dl.category ?? ''" :aria-label="t('common.category')" @change="dl.category = ($event.target as HTMLSelectElement).value === '' ? null : Number(($event.target as HTMLSelectElement).value)">
          <option value="">{{ t('transfers.allCategories') }}</option><option v-for="c in prefs.categories.filter((x) => x.id > 0)" :key="c.id" :value="c.id">{{ c.title }}</option></select>
        <div class="relative">
          <button class="inline-flex items-center gap-1 whitespace-nowrap rounded-md border border-line px-2.5 py-1.5 text-xs hover:bg-sunk" aria-haspopup="menu" :aria-expanded="menu" @click="menu = !menu">{{ t('catmenu.title') }}<Icon name="chevron" class="!size-3 rotate-90" /></button>
          <template v-if="menu">
            <div class="fixed inset-0 z-20" @click="menu = false"></div>
            <div class="absolute left-0 z-30 mt-1 max-h-[70vh] w-72 overflow-y-auto rounded-xl border border-line bg-surface py-1 text-xs shadow-xl" role="menu">
              <p class="px-3 pb-1 pt-2 text-[10px] font-semibold uppercase tracking-wide text-mute">{{ t('catmenu.moveSelected', sel.length) }}</p>
              <button role="menuitem" class="block w-full px-3 py-1.5 text-left hover:bg-sunk disabled:opacity-40" :disabled="!sel.length" @click="closeAnd(() => dl.setCategoryMany(hashes(sel), 0))">{{ t('catmenu.noCategory') }}</button>
              <button v-for="c in real" :key="c.id" role="menuitem" class="block w-full truncate px-3 py-1.5 text-left hover:bg-sunk disabled:opacity-40" :disabled="!sel.length" @click="closeAnd(() => dl.setCategoryMany(hashes(sel), c.id))">{{ c.title }}</button>
              <button role="menuitem" class="block w-full px-3 py-1.5 text-left text-accent hover:bg-sunk disabled:opacity-40" :disabled="!sel.length" @click="openDlg(null, true)">{{ t('catmenu.newAndMove') }}</button>
              <p class="mt-1 border-t border-line px-3 pb-1 pt-3 text-[10px] font-semibold uppercase tracking-wide text-mute">{{ viewCat ? t('catmenu.inCategory', { name: viewCat.title, n: inView.length }) : t('catmenu.inView', inView.length) }}</p>
              <button role="menuitem" class="block w-full px-3 py-1.5 text-left hover:bg-sunk disabled:opacity-40" :disabled="!inView.length" @click="closeAnd(() => dl.pause(hashes(inView)))">{{ t('common.pause') }}</button>
              <button role="menuitem" class="block w-full px-3 py-1.5 text-left hover:bg-sunk disabled:opacity-40" :disabled="!inView.length" @click="closeAnd(() => dl.resume(hashes(inView)))">{{ t('common.resume') }}</button>
              <button role="menuitem" class="block w-full px-3 py-1.5 text-left hover:bg-sunk disabled:opacity-40" :disabled="!inView.length" @click="closeAnd(resumeNext)">{{ t('catmenu.resumeNext') }}</button>
              <button role="menuitem" class="block w-full px-3 py-1.5 text-left hover:bg-sunk disabled:opacity-40" :disabled="!inView.length" @click="closeAnd(() => dl.stop(hashes(inView)))">{{ t('catmenu.stop') }}</button>
              <button role="menuitem" class="block w-full px-3 py-1.5 text-left text-bad hover:bg-sunk disabled:opacity-40" :disabled="!inView.length" @click="closeAnd(cancelView)">{{ t('common.cancelDownload') }}…</button>
              <label v-if="viewCat" class="flex items-center justify-between gap-2 px-3 py-1.5">{{ t('catmenu.priority') }}
                <select class="rounded-md border border-line bg-bg px-2 py-1 text-xs" :value="viewCat.priority" @change="prefs.editCategory(viewCat.id, { priority: Number(($event.target as HTMLSelectElement).value) })"><option v-for="x in CAT_PRIO" :key="x" :value="x">{{ t('prefs.catPrio.' + x) }}</option></select></label>
              <p class="mt-1 border-t border-line px-3 pb-1 pt-3 text-[10px] font-semibold uppercase tracking-wide text-mute">{{ t('catmenu.manage') }}</p>
              <button role="menuitem" class="block w-full px-3 py-1.5 text-left hover:bg-sunk" @click="openDlg(null)">{{ t('catmenu.new') }}</button>
              <button role="menuitem" class="block w-full px-3 py-1.5 text-left hover:bg-sunk disabled:opacity-40" :disabled="!viewCat" @click="openDlg(viewCat)">{{ t('catmenu.edit') }}</button>
              <button role="menuitem" class="block w-full px-3 py-1.5 text-left text-bad hover:bg-sunk disabled:opacity-40" :disabled="!viewCat" @click="closeAnd(delCat)">{{ t('catmenu.delete') }}</button>
            </div>
          </template>
        </div>
        <input v-model="dl.query" type="search" class="min-w-0 flex-1 basis-40 rounded-md border border-line bg-bg px-3 py-1.5 text-xs placeholder:text-mute lg:max-w-xs" :placeholder="t('transfers.filterByName')" :aria-label="t('transfers.filterByName')" />
        <div class="ml-auto flex items-center gap-1" role="toolbar" :aria-label="t('transfers.actions')">
          <span v-if="sel.length" class="pr-1 text-xs text-mute tabular-nums">{{ t('transfers.selected', sel.length) }}</span>
          <button class="rounded-md p-2 text-mute hover:bg-sunk hover:text-fg" :title="t('common.pause')" :aria-label="t('common.pause')" :disabled="!sel.length || !anyRunning" @click="dl.pause(sel.map((d) => d.hash))"><Icon name="pause" /></button>
          <button class="rounded-md p-2 text-mute hover:bg-sunk hover:text-fg" :title="t('common.resume')" :aria-label="t('common.resume')" :disabled="!sel.length || !anyPaused" @click="dl.resume(sel.map((d) => d.hash))"><Icon name="play" /></button>
          <button class="rounded-md p-2 text-mute hover:bg-sunk hover:text-bad" :title="allDone ? t('transfers.removeFromList') : t('transfers.cancelHint')" :aria-label="allDone ? t('transfers.removeFromList') : t('common.cancelDownload')" :disabled="!sel.length" @click="cancelSel"><Icon :name="allDone ? 'x' : 'trash'" /></button>
          <button class="ml-1 inline-flex items-center gap-1 whitespace-nowrap rounded-md border border-line px-2.5 py-1.5 text-xs hover:bg-sunk" :title="t('transfers.findAllHint')" @click="dl.findAllSources()"><Icon name="refresh" class="!size-3" />{{ t('transfers.findAll') }}</button>
          <button v-if="anyDone" class="ml-1 whitespace-nowrap rounded-md border border-line px-2.5 py-1.5 text-xs hover:bg-sunk" :title="t('transfers.clearCompletedHint')" @click="dl.clearCompleted()">{{ t('transfers.clearCompleted') }}</button>
        </div>
      </div>

      <div class="hidden items-center gap-4 border-b border-line bg-sunk px-4 py-2 text-xs font-medium uppercase tracking-wide text-mute lg:grid" :class="GRID" role="row">
        <input type="checkbox" :checked="allOn" :aria-label="t('transfers.selectAll')" @change="dl.selectAll(($event.target as HTMLInputElement).checked)" />
        <button v-for="c in COLS.filter((x) => !compact || ['name', 'percent', 'status'].includes(x.key))" :key="c.key" class="flex items-center gap-1 uppercase tracking-wide hover:text-fg" :class="c.right ? 'justify-end' : c.key === 'status' ? 'justify-center' : ''" :aria-sort="dl.sortKey === c.key ? (dl.sortDir === 1 ? 'ascending' : 'descending') : 'none'" @click="dl.setSort(c.key)">
          {{ t('cols.' + c.key) }}<Icon v-if="dl.sortKey === c.key" :name="dl.sortDir === 1 ? 'sortup' : 'sortdown'" class="!size-3.5" /></button>
      </div>

      <div v-if="!dl.loaded" class="p-8 text-center text-xs text-mute" role="status">{{ t('transfers.loading') }}</div>
      <EmptyState v-else-if="!dl.all.length" icon="dl" :title="t('transfers.emptyTitle')" :text="t('transfers.emptyText')">
        <button class="inline-flex items-center gap-1.5 rounded-lg bg-accent px-4 py-2 text-xs font-medium text-onaccent" @click="ui.openAdd()"><Icon name="plus" />{{ t('transfers.addLink') }}</button>
      </EmptyState>
      <EmptyState v-else-if="!dl.list.length" icon="search" :title="t('transfers.noMatchTitle')" :text="t('transfers.noMatchText')">
        <button class="rounded-lg border border-line px-4 py-2 text-xs hover:bg-sunk" @click="dl.filter = 'all'; dl.category = null; dl.query = ''">{{ t('transfers.showAll') }}</button>
      </EmptyState>
      <ul v-else class="min-h-0 flex-1 overflow-y-auto bg-surface" role="list">
        <li v-for="d in dl.list" :key="d.hash" class="flex cursor-pointer flex-col gap-2 border-b border-line px-4 py-3 hover:bg-sunk lg:grid lg:items-center lg:gap-4" :class="[GRID, dl.detail === d.hash ? '!bg-soft' : '', dl.selected.has(d.hash) ? 'bg-sunk' : '']" @click="pick(d.hash, $event)">
          <input type="checkbox" class="hidden lg:block" :checked="dl.selected.has(d.hash)" :aria-label="t('transfers.select', { name: d.name })" @change="dl.toggle(d.hash)" />
          <div class="flex min-w-0 items-start gap-3">
            <input type="checkbox" class="mt-1 lg:hidden" :checked="dl.selected.has(d.hash)" :aria-label="t('transfers.select', { name: d.name })" @change="dl.toggle(d.hash)" />
            <div class="min-w-0"><p class="line-clamp-2 break-words text-xs font-medium leading-snug" :title="d.name">{{ d.name }}</p><p class="text-xs text-mute tabular-nums"><span :class="compact ? '' : 'lg:hidden'">{{ bytes(d.size, 2, true) }}</span><template v-if="d.category && catName(d.category)"><span :class="compact ? '' : 'lg:hidden'"> · </span>{{ catName(d.category) }}</template></p></div>
          </div>
          <p class="hidden text-right text-xs tabular-nums" :class="compact ? '' : 'lg:block'">{{ bytes(d.size, 2, true) }}</p>
          <div class="flex items-center gap-2"><div class="flex-1"><ProgressBar :value="d.percent" :tone="statusInfo(d).key === 'done' ? 'ok' : 'accent'" :label="d.name" /></div><span class="w-14 text-right text-xs text-mute tabular-nums">{{ percent(d.percent) }}</span></div>
          <div class="flex items-center justify-between gap-3 text-xs lg:contents">
            <p class="tabular-nums lg:text-right" :class="compact ? 'lg:hidden' : ''"><span class="text-xs text-mute lg:hidden">↓ </span>{{ speed(d.speed) }}</p>
            <p class="tabular-nums lg:text-right" :class="compact ? 'lg:hidden' : ''"><span class="text-xs text-mute lg:hidden">{{ t('cols.eta') }} </span>{{ eta(d.size, d.completed, d.speed) }}</p>
            <p class="text-mute tabular-nums lg:text-right" :class="compact ? 'lg:hidden' : ''"><span class="text-xs lg:hidden">{{ t('cols.sources') }} </span>{{ d.sources_transferring }}/{{ d.sources }}<span v-if="d.best_queue_rank" class="ml-1 text-xs text-mute" :title="t('cols.bestRankHint')">#{{ d.best_queue_rank }}</span><span v-if="d.sources_other_files" class="ml-1 text-xs text-accent" :title="t('cols.otherFilesHint', { n: d.sources_other_files })">+{{ d.sources_other_files }}</span></p>
            <p class="flex items-center gap-2 lg:justify-center"><StatusIcon :download="d" /><span class="text-xs font-medium lg:hidden" :class="toneClass(statusInfo(d).tone)">{{ t(statusInfo(d).labelKey) }}</span></p>
          </div>
        </li>
      </ul>
    </div>

    <CategoryDialog :open="dlg.open" :category="dlg.category" @close="dlg.open = false" @saved="onSaved" />
    <aside v-if="dl.detail" class="fixed inset-0 z-20 bg-surface lg:static lg:inset-auto lg:z-auto lg:w-2/5 lg:shrink-0 lg:border-l lg:border-line" :aria-label="t('detail.aria')"><DownloadDetail :hash="dl.detail" @close="dl.detail = null" /></aside>
  </div>
</template>
