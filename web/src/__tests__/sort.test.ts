import { useSort } from '@/lib/useSort'

interface R { name: string; size: number }
const rows: R[] = [{ name: 'file10.pdf', size: 50 }, { name: 'File2.pdf', size: 900 }, { name: 'alfa.pdf', size: 7 }]
const cols = { name: { get: (r: R) => r.name }, size: { get: (r: R) => r.size, desc: true } }
const names = (s: { sorted: { value: R[] } }) => s.sorted.value.map((r) => r.name)

describe('useSort', () => {
  it('senza colonna scelta lascia l\'ordine del demone', () => { expect(names(useSort(() => rows, cols))).toEqual(['file10.pdf', 'File2.pdf', 'alfa.pdf']) })
  it('testo: crescente al primo clic, decrescente al secondo; senza distinguere maiuscole e con i numeri "naturali" (2 prima di 10)', () => {
    const s = useSort(() => rows, cols)
    s.sorter.toggle('name'); expect(names(s)).toEqual(['alfa.pdf', 'File2.pdf', 'file10.pdf'])
    s.sorter.toggle('name'); expect(names(s)).toEqual(['file10.pdf', 'File2.pdf', 'alfa.pdf'])
  })
  it('numeri: il piu\' grande in alto al primo clic; cambiando colonna si riparte dalla direzione giusta', () => {
    const s = useSort(() => rows, cols)
    s.sorter.toggle('size'); expect(s.sorted.value.map((r) => r.size)).toEqual([900, 50, 7])
    s.sorter.toggle('size'); expect(s.sorted.value.map((r) => r.size)).toEqual([7, 50, 900])
    s.sorter.toggle('name'); expect(s.sorter.dir.value).toBe(1)
  })
  it('non modifica l\'elenco originale e segue i dati che cambiano', () => {
    const live = [...rows]; const s = useSort(() => live, cols, 'size', -1)
    expect(names(s)[0]).toBe('File2.pdf'); expect(live[0].name).toBe('file10.pdf')
  })
  it('con un nome da ricordare, l\'ordine scelto torna alla prossima apertura; una colonna sparita si ignora', async () => {
    localStorage.clear()
    const a = useSort(() => rows, cols, '', 1, 'prova')
    a.sorter.toggle('size'); await Promise.resolve()
    const b = useSort(() => rows, cols, '', 1, 'prova')
    expect([b.sorter.key.value, b.sorter.dir.value]).toEqual(['size', -1])
    localStorage.setItem('emule.sort.prova', JSON.stringify({ key: 'sparita', dir: 1 }))
    expect(useSort(() => rows, cols, 'name', 1, 'prova').sorter.key.value).toBe('name')
  })
})
