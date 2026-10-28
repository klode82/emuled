import { describe, expect, it as test } from 'vitest'
import it from '@/i18n/it'
import en from '@/i18n/en'
import { PREFS, PREF_PAGES } from '@/lib/prefsMeta'

// every option of the Preferences pages has its text: label, choices, section and page title
describe('preferences', () => {
  test('ogni opzione ha etichetta, scelte, sezione e pagina in italiano e inglese', () => {
    for (const m of [it, en]) {
      const items = m.prefs.items as Record<string, { label: string; choices?: Record<number, string> }>
      const sections = m.prefs.sections as Record<string, Record<string, string>>
      for (const p of PREFS) {
        expect(items[p.key]?.label, p.key).toBeTruthy()
        for (const c of p.choices ?? []) expect(items[p.key].choices?.[c], `${p.key} ${c}`).toBeTruthy()
        if (p.section) expect(sections[p.page]?.[p.section], `${p.page}.${p.section}`).toBeTruthy()
      }
      for (const pg of PREF_PAGES) expect(m.prefs.pages[pg], pg).toBeTruthy()
    }
  })
  test('nessuna chiave ripetuta', () => expect(new Set(PREFS.map((p) => p.key)).size).toBe(PREFS.length))
})
