import { defineStore } from 'pinia'
import { computed, ref, watch } from 'vue'
import type { Download, Priority, ServerEvent } from '@/types'
import { statusInfo } from '@/lib/labels'
import { loadSort, saveSort } from '@/lib/useSort'
import { t } from '@/i18n'
import { errText, useToasts } from './toasts'
import { useSession } from './session'

export type Filter = 'all' | 'active' | 'waiting' | 'paused' | 'done'
export type SortKey = 'name' | 'size' | 'percent' | 'speed' | 'eta' | 'sources' | 'status'

export const FILTERS: { key: Filter }[] = [{ key: 'all' }, { key: 'active' }, { key: 'waiting' }, { key: 'paused' }, { key: 'done' }]
const matches = (d: Download, f: Filter) => {
  if (f === 'all') return true
  const k = statusInfo(d).key
  return f === 'active' ? k === 'active' : f === 'waiting' ? k === 'waiting' || k === 'busy' : f === 'paused' ? k === 'paused' : k === 'done'
}

export const useDownloads = defineStore('downloads', () => {
  const items = ref<Record<string, Download>>({})
  const loaded = ref(false)
  const filter = ref<Filter>('all')
  const category = ref<number | null>(null)
  const query = ref('')
  const savedSort = loadSort('transfers')
  const sortKey = ref<SortKey>((savedSort.key as SortKey | undefined) ?? 'name')
  const sortDir = ref<1 | -1>(savedSort.dir === -1 ? -1 : 1)
  watch([sortKey, sortDir], () => saveSort('transfers', sortKey.value, sortDir.value))
  const selected = ref<Set<string>>(new Set())
  const detail = ref<string | null>(null)
  const toasts = useToasts()
  const api = () => useSession().getApi()

  const all = computed(() => Object.values(items.value))
  const counts = computed(() => Object.fromEntries(FILTERS.map((f) => [f.key, all.value.filter((d) => matches(d, f.key)).length])) as Record<Filter, number>)
  const list = computed(() => {
    const q = query.value.trim().toLowerCase()
    const k = sortKey.value, dir = sortDir.value
    const val = (d: Download): string | number => (k === 'status' ? t(statusInfo(d).labelKey) : k === 'name' ? d.name.toLowerCase() : k === 'eta' ? (d.speed > 0 && d.completed < d.size ? (d.size - d.completed) / d.speed : Infinity) : d[k])
    return all.value
      .filter((d) => matches(d, filter.value) && (category.value === null || d.category === category.value) && (!q || d.name.toLowerCase().includes(q)))
      .sort((a, b) => { const x = val(a), y = val(b); return (x < y ? -1 : x > y ? 1 : a.hash < b.hash ? -1 : 1) * dir })
  })
  const detailItem = computed(() => (detail.value ? items.value[detail.value] ?? null : null))
  const selectedItems = computed(() => [...selected.value].map((h) => items.value[h]).filter(Boolean) as Download[])

  async function load() { const a = await api().downloads(); items.value = Object.fromEntries(a.map((d) => [d.hash, d])); loaded.value = true; prune() }
  function prune() {
    for (const h of [...selected.value]) if (!items.value[h]) selected.value.delete(h)
    if (detail.value && !items.value[detail.value]) detail.value = null
  }
  function apply(e: ServerEvent) {
    if (e.type === 'download.added' || e.type === 'download.updated') items.value[e.data.hash] = e.data
    else if (e.type === 'download.removed') { delete items.value[e.data.hash]; prune() }
  }
  function setSort(k: SortKey) { if (sortKey.value === k) sortDir.value = sortDir.value === 1 ? -1 : 1; else { sortKey.value = k; sortDir.value = k === 'name' || k === 'status' || k === 'eta' ? 1 : -1 } }
  function toggle(hash: string) { selected.value.has(hash) ? selected.value.delete(hash) : selected.value.add(hash) }
  function selectAll(on: boolean) { selected.value = on ? new Set(list.value.map((d) => d.hash)) : new Set() }

  // le azioni mandano la richiesta e aggiornano l'elemento con la risposta; gli eventi del WebSocket fanno il resto
  async function act(hashes: string[], what: 'pause' | 'resume' | 'stop' | 'remove', done: string) {
    const results = await Promise.allSettled(hashes.map(async (h) => {
      const r = await api()[what](h)
      if (what === 'remove') delete items.value[h]; else items.value[h] = r
    }))
    const failed = results.filter((r) => r.status === 'rejected') as PromiseRejectedResult[]
    prune()
    if (failed.length) toasts.error(t('toast.someFailed', { failed: failed.length, total: hashes.length, reason: errText(failed[0].reason) }))
    else toasts.ok(hashes.length > 1 ? t('toast.manyFiles', { what: done, n: hashes.length }) : done)
  }
  const pause = (h: string[]) => act(h, 'pause', t('toast.paused'))
  const resume = (h: string[]) => act(h, 'resume', t('toast.resumed'))
  const stop = (h: string[]) => act(h, 'stop', t('toast.stopped'))
  const remove = (h: string[]) => act(h, 'remove', t(h.every((x) => items.value[x]?.status === 'complete') ? 'toast.removedFromList' : 'toast.removed'))

  async function setPriority(hash: string, p: Priority) {
    try { items.value[hash] = await api().patchDownload(hash, { priority: p }) } catch (e) { toasts.error(errText(e)) }
  }
  async function setCategory(hash: string, c: number) {
    try { items.value[hash] = await api().patchDownload(hash, { category: c }) } catch (e) { toasts.error(errText(e)) }
  }
  // sposta piu' download in una categoria (0 = nessuna)
  async function setCategoryMany(hashes: string[], c: number) {
    let ok = 0
    for (const h of hashes) { try { items.value[h] = await api().patchDownload(h, { category: c }); ok++ } catch (e) { toasts.error(errText(e)); break } }
    if (ok) toasts.ok(t('toast.moved', ok))
  }
  // aggiunge uno o piu' link; ritorna quanti sono stati aggiunti
  async function addLinks(links: string[], cat = 0): Promise<number> {
    let ok = 0; const errs: string[] = []
    for (const link of links) {
      try { const d = await api().addDownload({ link, category: cat }); items.value[d.hash] = d; ok++ } catch (e) { errs.push(errText(e)) }
    }
    if (ok) toasts.ok(t('toast.added', ok))
    if (errs.length) toasts.error(errs[0] + (errs.length > 1 ? ' ' + t('toast.moreErrors', { n: errs.length - 1 }) : ''))
    return ok
  }
  // scarica piu' risultati di ricerca insieme (quelli gia' in lista si saltano); ritorna quanti ne ha aggiunti
  async function addFoundMany(fs: { hash: string; name: string; size: number }[], cat = 0): Promise<number> {
    let ok = 0; const errs: string[] = []
    for (const f of fs) {
      if (items.value[f.hash]) continue
      try { const d = await api().addDownload({ ...f, category: cat }); items.value[d.hash] = d; ok++ } catch (e) { errs.push(errText(e)) }
    }
    if (ok) toasts.ok(t('toast.added', ok))
    if (errs.length) toasts.error(errs[0] + (errs.length > 1 ? ' ' + t('toast.moreErrors', { n: errs.length - 1 }) : ''))
    return ok
  }
  async function addFound(f: { hash: string; name: string; size: number }, cat = 0): Promise<boolean> {
    try { const d = await api().addDownload({ ...f, category: cat }); items.value[d.hash] = d; toasts.ok(t('toast.added', 1)); return true } catch (e) { toasts.error(errText(e)); return false }
  }
  // "Cerca fonti adesso": il demone azzera le attese e chiede subito al server e a Kad; il messaggio dice cosa e' partito davvero
  async function findSources(hash: string) {
    try {
      const r = await api().findSources(hash)
      toasts.ok(r.paused ? t('toast.sourcesPaused') : !r.server && !r.kad ? t('toast.sourcesOffline') : r.server && r.wait_s >= 5 ? t('toast.sourcesQueued', { s: r.wait_s }) : t('toast.sourcesAsked'))
    } catch (e) { toasts.error(errText(e)) }
  }
  // "Cerca fonti" su tutti i download che stanno scaricando o aspettano (non completati, non fermi): un solo messaggio
  async function findAllSources() {
    const todo = all.value.filter((d) => !d.stopped && d.status !== 'paused' && d.status !== 'complete')
    if (!todo.length) { toasts.info(t('toast.sourcesNone')); return }
    try {
      const rs = await Promise.all(todo.map((d) => api().findSources(d.hash)))
      if (!rs[0].server && !rs[0].kad) toasts.ok(t('toast.sourcesOffline'))
      else toasts.ok(t('toast.sourcesAll', { n: todo.length, s: rs[0].server ? Math.max(...rs.map((r) => r.wait_s)) : 0 }))
    } catch (e) { toasts.error(errText(e)) }
  }
  // "Togli": un completato esce dalla lista (il file resta su disco); "Svuota": tutti i completati
  async function clearCompleted() {
    try { const r = await api().clearCompleted(); await load(); toasts.ok(t('toast.cleared', r.removed)) } catch (e) { toasts.error(errText(e)) }
  }
  return { items, loaded, filter, category, query, sortKey, sortDir, selected, detail, all, counts, list, detailItem, selectedItems, load, apply, setSort, toggle, selectAll, pause, resume, stop, remove, setCategoryMany, addFoundMany, findSources, findAllSources, clearCompleted, setPriority, setCategory, addLinks, addFound }
})
