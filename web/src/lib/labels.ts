// Etichette dell'API (stati, priorita'): qui solo le chiavi di traduzione e i toni; il testo sta in i18n/*.ts.
import type { Download, Priority } from '@/types'

export type Tone = 'ok' | 'warn' | 'bad' | 'mute'
export const PRIORITIES: Priority[] = ['auto', 'very_low', 'low', 'normal', 'high', 'release']
export const priorityKey = (p: Priority) => `priority.${p}`

export function statusInfo(d: Download): { labelKey: string; tone: Tone; key: 'active' | 'waiting' | 'paused' | 'done' | 'error' | 'busy' } {
  switch (d.status) {
    case 'complete': return { labelKey: 'status.complete', tone: 'ok', key: 'done' }
    case 'completing': return { labelKey: 'status.completing', tone: 'ok', key: 'busy' }
    case 'hashing': case 'waiting_for_hash': return { labelKey: 'status.hashing', tone: 'mute', key: 'busy' }
    case 'error': return { labelKey: 'status.error', tone: 'bad', key: 'error' }
    case 'insufficient_disk_space': return { labelKey: 'status.diskFull', tone: 'bad', key: 'error' }
    case 'paused': return { labelKey: d.stopped ? 'status.stopped' : 'status.paused', tone: 'mute', key: 'paused' }
  }
  if (d.stopped) return { labelKey: 'status.stopped', tone: 'mute', key: 'paused' }
  if (d.speed > 0) return { labelKey: 'status.downloading', tone: 'ok', key: 'active' }
  if (d.sources > 0) return { labelKey: 'status.waitingFree', tone: 'warn', key: 'waiting' }
  return { labelKey: 'status.waitingSources', tone: 'warn', key: 'waiting' }
}
export const toneClass = (t: Tone) => (t === 'ok' ? 'text-ok' : t === 'warn' ? 'text-warn' : t === 'bad' ? 'text-bad' : 'text-mute')
