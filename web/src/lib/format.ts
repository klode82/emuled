// Formati dei numeri, delle dimensioni e dei tempi. Seguono la lingua dell'interfaccia (virgola o punto decimale).
// Le dimensioni sono in base 1024 con le sigle KB/MB/GB: come eMule, cosi' i numeri coincidono con quelli del client Windows.
import { lang, t } from '@/i18n'

const U = ['B', 'KB', 'MB', 'GB', 'TB']
const nf = (digits: number) => new Intl.NumberFormat(lang(), { minimumFractionDigits: digits, maximumFractionDigits: digits })
// minKb: la dimensione minima mostrata e' il KB (un file piccolo si vede 0,29 KB, non 300 B)
export function bytes(n: number, digits = 2, minKb = false): string {
  if (!Number.isFinite(n) || n < 0) return '—'
  let i = 0
  if (minKb) { n /= 1024; i = 1 }
  while (n >= 1024 && i < U.length - 1) { n /= 1024; i++ }
  return `${i === 0 ? Math.round(n) : nf(digits).format(n)} ${U[i]}`
}
export function speed(n: number): string { return n > 0 ? `${bytes(n, 1)}/s` : '—' }
export function percent(n: number): string { return `${nf(n >= 100 || Number.isInteger(n) ? 0 : 1).format(n >= 100 ? 100 : Math.floor(n * 10) / 10)}%` }
export function duration(seconds: number): string {
  if (!Number.isFinite(seconds) || seconds < 0) return '—'
  const s = Math.round(seconds), d = Math.floor(s / 86400), h = Math.floor((s % 86400) / 3600), m = Math.floor((s % 3600) / 60)
  if (d) return t('time.dh', { d, h })
  if (h) return t('time.hm', { h, m })
  if (m) return t('time.m', { m })
  return t('time.s', { s })
}
// tempo rimasto: serve una velocita' positiva e qualcosa ancora da scaricare
export function eta(size: number, completed: number, bps: number): string {
  if (bps <= 0 || completed >= size) return '—'
  return duration((size - completed) / bps)
}
export function number(n: number): string { return new Intl.NumberFormat(lang()).format(n) }
export function dateTime(unixSeconds: number): string { return new Intl.DateTimeFormat(lang(), { dateStyle: 'medium', timeStyle: 'short' }).format(new Date(unixSeconds * 1000)) }
export function ed2kLink(name: string, size: number, hash: string): string {
  return `ed2k://|file|${encodeURIComponent(name)}|${size}|${hash}|/`
}
// ricava i link ed2k://|file|...|/ da un testo incollato (uno per riga o separati da spazi)
export function extractLinks(text: string): string[] {
  return text.match(/ed2k:\/\/\|file\|[^|]*\|\d+\|[0-9A-Fa-f]{32}\|[^\s]*/g) ?? []
}
export function looksLikeLink(text: string): boolean { return /^\s*ed2k:\/\/\|/i.test(text) }
