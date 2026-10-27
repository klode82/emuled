// Sorting of a table's columns: click on the header = ascending order, second click = descending. Text columns start ascending,
// numeric ones descending (largest on top). With no column chosen (key = '') rows stay in the order they arrive from the daemon.
import { computed, ref, watch, type Ref } from 'vue'

export interface SortCol<T> { get: (row: T) => string | number; desc?: boolean }   // desc: the first time it sorts from the largest
export interface Sorter { key: Ref<string>; dir: Ref<1 | -1>; toggle: (k: string) => void }

// With `persist` (table name) the chosen order is remembered in this browser, even after restart.
export const loadSort = (persist: string): { key?: string; dir?: 1 | -1 } => { try { return JSON.parse(localStorage.getItem('emule.sort.' + persist) ?? '{}') } catch { return {} } }
export const saveSort = (persist: string, key: string, dir: 1 | -1) => { try { localStorage.setItem('emule.sort.' + persist, JSON.stringify({ key, dir })) } catch { /* without browser storage the order is not remembered */ } }

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
