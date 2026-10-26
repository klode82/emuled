import { readdirSync, readFileSync, statSync } from 'node:fs'
import { join } from 'node:path'
import itMessages from '@/i18n/it'
import enMessages from '@/i18n/en'
import { detectLang, setLang, t } from '@/i18n'

type Tree = { [k: string]: string | Tree }
const flat = (o: Tree, p = ''): Record<string, string> => Object.fromEntries(Object.entries(o).flatMap(([k, v]) => (typeof v === 'string' ? [[p + k, v]] : Object.entries(flat(v, p + k + '.')))))
const A = flat(itMessages as unknown as Tree), B = flat(enMessages as unknown as Tree)
const params = (s: string) => [...s.matchAll(/\{(\w+)\}/g)].map((m) => m[1]).sort().join(',')

describe('traduzioni', () => {
  it('italiano e inglese hanno le stesse chiavi', () => {
    expect(Object.keys(B).sort()).toEqual(Object.keys(A).sort())
  })
  it('nessun testo vuoto, stessi segnaposto e stesso numero di forme plurali', () => {
    for (const k of Object.keys(A)) {
      expect(A[k].trim(), k).not.toBe(''); expect(B[k].trim(), k).not.toBe('')
      expect(params(B[k]), `segnaposto di ${k}`).toBe(params(A[k]))
      expect(B[k].split(' | ').length, `forme plurali di ${k}`).toBe(A[k].split(' | ').length)
    }
  })
  it('ogni chiave usata nel codice esiste (t("...") e $t("..."), anche con prefissi dinamici)', () => {
    const walk = (d: string): string[] => readdirSync(d).flatMap((f) => { const p = join(d, f); return statSync(p).isDirectory() ? (f === '__tests__' || f === 'i18n' ? [] : walk(p)) : /\.(vue|ts)$/.test(f) ? [p] : [] })
    const files = walk(join(__dirname, '..'))
    const used = new Set<string>(), prefixes = new Set<string>()
    for (const f of files) {
      const src = readFileSync(f, 'utf8')
      for (const m of src.matchAll(/\b(?:t|te|tr)\(\s*(['`])([\w.${}]+?)\1/g)) { if (m[2].includes('${')) prefixes.add(m[2].split('${')[0]); else if (m[2].endsWith('.')) prefixes.add(m[2]); else used.add(m[2]) }
      for (const m of src.matchAll(/\b(?:t|te)\(\s*'([\w.]+\.)'\s*\+/g)) prefixes.add(m[1])
      for (const m of src.matchAll(/(?:labelKey|key):\s*'([a-z]+\.[A-Za-z_.]+)'/g)) used.add(m[1])
      if (src.includes('priority.${')) prefixes.add('priority.')
    }
    const missing = [...used].filter((k) => !(k in A))
    expect(missing).toEqual([])
    for (const p of prefixes) expect(Object.keys(A).some((k) => k.startsWith(p)), `prefisso ${p}`).toBe(true)
  })
  it('scelta della lingua: salvata, poi quella del browser, poi italiano', () => {
    expect(detectLang('en', ['it-IT'])).toBe('en'); expect(detectLang(null, ['fr-FR', 'en-GB'])).toBe('en'); expect(detectLang('xx', ['de'])).toBe('it'); expect(detectLang(null, [])).toBe('it')
  })
  it('plurali e segnaposto funzionano nelle due lingue', () => {
    setLang('it'); expect(t('toast.added', 1)).toBe('Download aggiunto'); expect(t('toast.added', 3)).toBe('3 download aggiunti'); expect(t('transfers.selected', 2)).toBe('2 selezionati')
    setLang('en'); expect(t('toast.added', 1)).toBe('Download added'); expect(t('toast.added', 3)).toBe('3 downloads added'); expect(t('shell.ports', { tcp: 1, udp: 2 })).toBe('TCP port 1 · UDP 2')
    setLang('it')
  })
})
