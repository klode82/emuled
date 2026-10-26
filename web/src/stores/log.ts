// Registro degli eventi del demone: caricamento iniziale, aggiornamento in diretta (evento WebSocket "log") e riallineamento dopo una riconnessione
import { defineStore } from 'pinia'
import { computed, ref } from 'vue'
import type { LogLine, ServerEvent } from '@/types'
import { t } from '@/i18n'
import { errText, useToasts } from './toasts'
import { useSession } from './session'

const MAX = 3000
export type LevelFilter = 'all' | 'info' | 'warning' | 'error'
export type SourceFilter = 'all' | 'general' | 'server' | 'api'
const RANK = { debug: 0, info: 1, warning: 2, error: 3 } as const

export const useLog = defineStore('log', () => {
  const lines = ref<LogLine[]>([])
  const lastId = ref(0)
  const loaded = ref(false)
  const level = ref<LevelFilter>('all')
  const source = ref<SourceFilter>('all')
  const query = ref('')
  const follow = ref(true)
  const api = () => useSession().getApi()

  function append(more: LogLine[]) {
    const fresh = more.filter((l) => l.id > lastId.value)	// le righe gia' viste (evento e lettura insieme) non si ripetono
    if (!fresh.length) return
    lines.value.push(...fresh)
    if (lines.value.length > MAX) lines.value.splice(0, lines.value.length - MAX)
    lastId.value = fresh[fresh.length - 1].id
  }
  async function load() {
    try { const r = await api().log({ level: 'debug', limit: 1000 }); lines.value = r.lines; lastId.value = r.last_id; loaded.value = true } catch (e) { useToasts().error(errText(e)) }
  }
  // dopo una riconnessione: solo le righe nuove; se il buffer del demone ne ha perse (o e' stato svuotato da un altro) si rilegge tutto
  async function sync() {
    if (!loaded.value) return
    try {
      const r = await api().log({ after: lastId.value, level: 'debug', limit: 2000 })
      if (r.truncated || r.last_id < lastId.value) await load(); else append(r.lines)
    } catch { /* si riprova al giro dopo */ }
  }
  function apply(e: ServerEvent) { if (e.type === 'log' && loaded.value) append(e.data.lines) }
  async function clear() {
    try { await api().clearLog(); lines.value = []; useToasts().ok(t('log.cleared')) } catch (e) { useToasts().error(errText(e)) }
  }

  const shown = computed(() => {
    const q = query.value.trim().toLowerCase()
    const min = level.value === 'all' ? 0 : RANK[level.value]
    return lines.value.filter((l) => RANK[l.level] >= min && (source.value === 'all' || l.source === source.value) && (!q || l.text.toLowerCase().includes(q)))
  })
  const hasDebug = computed(() => lines.value.some((l) => l.level === 'debug'))
  return { lines, lastId, loaded, level, source, query, follow, shown, hasDebug, load, sync, apply, clear }
})
