import { createPinia, setActivePinia } from 'pinia'
import { useDownloads } from '@/stores/downloads'
import { statusInfo } from '@/lib/labels'
import type { Download } from '@/types'
import { setLang } from '@/i18n'

const d = (o: Partial<Download>): Download => ({ hash: 'H' + (o.name ?? 'x'), name: 'x', size: 100, completed: 0, percent: 0, status: 'ready', stopped: false, sources: 0, sources_transferring: 0, sources_other_files: 0, best_queue_rank: 0, speed: 0, priority: 'normal', category: 0, part_file: '001.part.met', ...o })

describe('downloads store', () => {
  beforeAll(() => setLang('it'))
  beforeEach(() => setActivePinia(createPinia()))
  it('applica gli eventi del WebSocket', () => {
    const s = useDownloads()
    s.apply({ type: 'download.added', data: d({ name: 'a' }) }); s.apply({ type: 'download.added', data: d({ name: 'b' }) })
    expect(s.all).toHaveLength(2)
    s.apply({ type: 'download.updated', data: d({ name: 'a', percent: 50 }) }); expect(s.items['Ha'].percent).toBe(50)
    s.detail = 'Ha'; s.selected.add('Ha')
    s.apply({ type: 'download.removed', data: { hash: 'Ha', name: 'a' } })
    expect(s.all).toHaveLength(1); expect(s.detail).toBeNull(); expect(s.selected.size).toBe(0)   // selection and detail do not stay on a vanished file
  })
  it('filtri, conteggi, ricerca per nome e ordinamento', () => {
    const s = useDownloads()
    for (const x of [d({ name: 'Zeta', speed: 5, sources: 2 }), d({ name: 'alfa', status: 'paused' }), d({ name: 'Beta', status: 'complete', percent: 100 }), d({ name: 'gamma' })]) s.apply({ type: 'download.added', data: x })
    expect(s.counts).toMatchObject({ all: 4, active: 1, waiting: 1, paused: 1, done: 1 })
    s.filter = 'paused'; expect(s.list.map((x) => x.name)).toEqual(['alfa'])
    s.filter = 'all'; expect(s.list.map((x) => x.name)).toEqual(['alfa', 'Beta', 'gamma', 'Zeta'])   // case-insensitive
    s.setSort('name'); expect(s.list[0].name).toBe('Zeta')                                            // second click: reverse order
    s.query = 'ET'; expect(s.list.map((x) => x.name).sort()).toEqual(['Beta', 'Zeta'])
    s.query = ''; s.category = 3; expect(s.list).toHaveLength(0)
  })
  it('stato leggibile', () => {
    expect(statusInfo(d({ speed: 10 })).labelKey).toBe('status.downloading')
    expect(statusInfo(d({ sources: 3 })).labelKey).toBe('status.waitingFree')
    expect(statusInfo(d({})).labelKey).toBe('status.waitingSources')
    expect(statusInfo(d({ status: 'paused', stopped: true })).labelKey).toBe('status.stopped')
    expect(statusInfo(d({ status: 'insufficient_disk_space' })).tone).toBe('bad')
  })
})
