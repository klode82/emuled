import { defineStore } from 'pinia'
import { ref } from 'vue'
import type { NewSearch, Search, ServerEvent } from '@/types'
import { errText, useToasts } from './toasts'
import { useSession } from './session'

export const useSearch = defineStore('search', () => {
  const current = ref<Search | null>(null)
  const list = ref<Search[]>([])
  const form = ref<NewSearch>({ query: '', scope: 'global' })
  const starting = ref(false)
  let timer: ReturnType<typeof setInterval> | null = null
  const api = () => useSession().getApi()

  function stopPoll() { if (timer) { clearInterval(timer); timer = null } }
  async function poll() {
    const c = current.value
    if (!c) return stopPoll()
    try {
      current.value = await api().search(c.id)
      upsert(current.value)
      if (current.value.state !== 'running') stopPoll()
    } catch { stopPoll() }
  }
  function upsert(c: Search) {
    const { results: _r, ...head } = c
    const i = list.value.findIndex((x) => x.id === c.id)
    if (i >= 0) list.value[i] = head; else list.value.unshift(head)
  }
  // il demone ricorda le ultime ricerche: al primo ingresso si riprendono, la piu' recente resta aperta
  async function load() {
    try {
      list.value = await api().searches()
      if (!current.value && list.value.length) await select(list.value[0].id)
    } catch { /* senza elenco si parte da zero */ }
  }
  async function select(id: number) {
    stopPoll()
    try {
      current.value = await api().search(id)
      form.value = { ...form.value, query: current.value.query, scope: current.value.scope }		// il modulo mostra la ricerca della scheda
      if (current.value.state === 'running') { timer = setInterval(() => void poll(), 1000) }
    } catch (e) { useToasts().error(errText(e)) }
  }
  async function remove(id: number) {
    try { await api().removeSearch(id) } catch (e) { useToasts().error(errText(e)); return }
    list.value = list.value.filter((x) => x.id !== id)
    if (current.value?.id === id) { stopPoll(); current.value = null; if (list.value.length) await select(list.value[0].id) }
  }
  async function start(params?: NewSearch) {
    if (params) form.value = { ...params }
    const f = { ...form.value, query: form.value.query.trim() }
    if (!f.query) return
    // campi opzionali vuoti non si mandano
    for (const k of ['ext', 'type'] as const) if (!f[k]) delete f[k]
    for (const k of ['min_size', 'max_size', 'min_sources', 'min_complete'] as const) if (!f[k]) delete f[k]
    starting.value = true
    try {
      current.value = await api().startSearch(f)
      stopPoll(); upsert(current.value); timer = setInterval(() => void poll(), 1000); void poll()
    } catch (e) { useToasts().error(errText(e)) } finally { starting.value = false }
  }
  // "Aggiorna": rifa la stessa ricerca (parola, dove e filtri) nella stessa scheda
  async function refresh() {
    const c = current.value
    if (!c) return
    try {
      current.value = await api().refreshSearch(c.id)
      stopPoll(); upsert(current.value); timer = setInterval(() => void poll(), 1000); void poll()
    } catch (e) { useToasts().error(errText(e)) }
  }
  async function cancel() {
    if (!current.value) return
    try { current.value = await api().cancelSearch(current.value.id); stopPoll(); await poll() } catch (e) { useToasts().error(errText(e)) }
  }
  function apply(e: ServerEvent) {
    // l'evento porta solo lo stato: i risultati si leggono con il sondaggio
    if (e.type === 'search.results' && current.value && e.data.id === current.value.id) {
      current.value = { ...current.value, state: e.data.state, result_count: e.data.result_count, elapsed_s: e.data.elapsed_s }
      upsert(current.value)
    }
  }
  return { current, list, load, select, remove, refresh, form, starting, start, cancel, apply }
})
