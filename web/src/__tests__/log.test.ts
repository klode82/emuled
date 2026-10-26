import { createPinia, setActivePinia } from 'pinia'
import { useLog } from '@/stores/log'
import type { LogLine } from '@/types'

const line = (id: number, level: LogLine['level'] = 'info', source: LogLine['source'] = 'general', text = 'riga ' + id): LogLine => ({ id, time: 1700000000000 + id, level, source, text })

describe('log store', () => {
  beforeEach(() => setActivePinia(createPinia()))
  it('gli eventi si ignorano finche\' il registro non e\' stato caricato, poi si accodano senza ripetizioni', () => {
    const s = useLog()
    s.apply({ type: 'log', data: { lines: [line(1)], last_id: 1 } }); expect(s.lines).toHaveLength(0)
    s.loaded = true
    s.apply({ type: 'log', data: { lines: [line(1), line(2)], last_id: 2 } }); expect(s.lines.map((l) => l.id)).toEqual([1, 2]); expect(s.lastId).toBe(2)
    s.apply({ type: 'log', data: { lines: [line(2), line(3)], last_id: 3 } }); expect(s.lines.map((l) => l.id)).toEqual([1, 2, 3])		// la 2 era gia' arrivata
    s.apply({ type: 'log', data: { lines: [], last_id: 3 } }); expect(s.lines).toHaveLength(3)
  })
  it('tiene al massimo 3000 righe (le piu\' vecchie vanno via)', () => {
    const s = useLog(); s.loaded = true
    s.apply({ type: 'log', data: { lines: Array.from({ length: 3500 }, (_, i) => line(i + 1)), last_id: 3500 } })
    expect(s.lines).toHaveLength(3000); expect(s.lines[0].id).toBe(501); expect(s.lastId).toBe(3500)
  })
  it('filtri: livello minimo, fonte e testo (senza distinguere maiuscole)', () => {
    const s = useLog(); s.loaded = true
    s.apply({ type: 'log', data: { lines: [line(1, 'debug'), line(2, 'info', 'general', 'Collegato al Server'), line(3, 'warning', 'server', 'avviso'), line(4, 'error', 'api', 'Errore grave')], last_id: 4 } })
    expect(s.shown).toHaveLength(4); expect(s.hasDebug).toBe(true)
    s.level = 'info'; expect(s.shown.map((l) => l.id)).toEqual([2, 3, 4])
    s.level = 'warning'; expect(s.shown.map((l) => l.id)).toEqual([3, 4])
    s.level = 'all'; s.source = 'server'; expect(s.shown.map((l) => l.id)).toEqual([3])
    s.source = 'all'; s.query = 'SERVER'; expect(s.shown.map((l) => l.id)).toEqual([2])
  })
})
