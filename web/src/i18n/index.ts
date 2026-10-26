// Traduzioni: italiano (lingua di riferimento), inglese e le 12 altre piu' diffuse (es, zh, hi, ar, fr, pt, ru, de, ja, ko, tr, id). Per aggiungere una lingua: copiare it.ts, tradurre, registrarla in LANGS.
// La lingua si sceglie nelle Preferenze (o dalla schermata di accesso) e si ricorda nel browser; la prima volta si usa quella del browser.
import { createI18n } from 'vue-i18n'
import it from './it'
import en from './en'
import es from './es'
import zh from './zh'
import hi from './hi'
import ar from './ar'
import fr from './fr'
import pt from './pt'
import ru from './ru'
import de from './de'
import ja from './ja'
import ko from './ko'
import tr from './tr'
import id from './id'

export const LANGS = [{ code: 'it', name: 'Italiano' }, { code: 'en', name: 'English' },
  { code: 'es', name: 'Español' }, { code: 'zh', name: '中文 (简体)' }, { code: 'hi', name: 'हिन्दी' }, { code: 'ar', name: 'العربية' }, { code: 'fr', name: 'Français' }, { code: 'pt', name: 'Português' }, { code: 'ru', name: 'Русский' }, { code: 'de', name: 'Deutsch' }, { code: 'ja', name: '日本語' }, { code: 'ko', name: '한국어' }, { code: 'tr', name: 'Türkçe' }, { code: 'id', name: 'Bahasa Indonesia' }] as const
export type Lang = (typeof LANGS)[number]['code']
const KEY = 'emuled.lang'
const RTL = new Set<string>(['ar'])		// lingue scritte da destra a sinistra

export function detectLang(stored: string | null, browser: readonly string[]): Lang {
  const ok = (c: string): c is Lang => LANGS.some((l) => l.code === c)
  if (stored && ok(stored)) return stored
  for (const b of browser) { const c = b.toLowerCase().split('-')[0]; if (ok(c)) return c }
  return 'it'
}
const read = () => { try { return localStorage.getItem(KEY) } catch { return null } }

export const i18n = createI18n({
  legacy: false,
  locale: detectLang(read(), typeof navigator !== 'undefined' ? navigator.languages ?? [navigator.language] : []),
  fallbackLocale: 'it',
  messages: { it, en, es, zh, hi, ar, fr, pt, ru, de, ja, ko, tr, id },
})
export const te = (key: string) => i18n.global.te(key)		// esiste una traduzione per questa chiave?
export const lang = () => i18n.global.locale.value as Lang
export function setLang(l: Lang) {
  i18n.global.locale.value = l
  try { localStorage.setItem(KEY, l) } catch { /* archivio non disponibile */ }
  if (typeof document !== 'undefined') { document.documentElement.lang = l; document.documentElement.dir = RTL.has(l) ? 'rtl' : 'ltr' }
}
if (typeof document !== 'undefined') { document.documentElement.lang = lang(); document.documentElement.dir = RTL.has(lang()) ? 'rtl' : 'ltr' }
// t() fuori dai componenti (store, formati). Con un numero come secondo argomento sceglie la forma singolare/plurale ("uno | molti", {n}).
export const t = (key: string, arg?: Record<string, unknown> | number) => (typeof arg === 'number' ? i18n.global.t(key, arg) : i18n.global.t(key, arg ?? {})) as string
