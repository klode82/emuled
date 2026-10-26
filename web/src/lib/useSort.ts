// Ordinamento delle colonne di una tabella: clic sull'intestazione = ordine crescente, secondo clic = decrescente. Le colonne di testo partono dal crescente,
// quelle numeriche dal decrescente (il piu' grande in alto). Senza colonna scelta (key = '') le righe restano nell'ordine che arriva dal demone.
import { computed, ref, watch, type Ref } from 'vue'

export interface SortCol<T> { get: (row: T) => string | number; desc?: boolean }   // desc: la prima volta si ordina dal piu' grande
export interface Sorter { key: Ref<string>; dir: Ref<1 | -1>; toggle: (k: string) => void }

// Con `persist` (nome della tabella) l'ordine scelto si ricorda in questo browser, anche dopo il riavvio.
export const loadSort = (persist: string): { key?: string; dir?: 1 | -1 } => { try { return JSON.parse(localStorage.getItem('emule.sort.' + persist) ?? '{}') } catch { return {} } }
export const saveSort = (persist: string, key: string, dir: 1 | -1) => { try { localStorage.setItem('emule.sort.' + persist, JSON.stringify({ key, dir })) } catch { /* senza memoria del browser l'ordine non si ricorda */ } }

export function useSort<T>(rows: () => T[], cols: Record<string, SortCol<T>>, initial = '', initialDir: 1 | -1 = 1, persist = '') {
  const saved = persist ? loadSort(persist) : {}
  const ok = saved.key !== undefined && (saved.key === '' || saved.key in cols) && (saved.dir === 1 || saved.dir === -1)
  const key = ref(ok ? saved.key! : initial)
  const dir = ref<1 | -1>(ok ? saved.dir! : initialDir)
  if (persist) watch([key, dir], () => saveSort(persist, key.value, dir.value))
  const sorted = computed(() => {
    const c = cols[key.value]
    if (!c) return rows()
    const d = dir.value
    return [...rows()].sort((a, b) => {
      const x = c.get(a), y = c.get(b)
      const r = typeof x === 'string' && typeof y === 'string' ? x.localeCompare(y, undefined, { numeric: true, sensitivity: 'base' }) : x < y ? -1 : x > y ? 1 : 0
      return r * d
    })
  })
  function toggle(k: string) {
    if (key.value === k) dir.value = dir.value === 1 ? -1 : 1
    else { key.value = k; dir.value = cols[k]?.desc ? -1 : 1 }
  }
  return { sorted, sorter: { key, dir, toggle } as Sorter }
}
