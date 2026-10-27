import { defineStore } from 'pinia'
import { ref } from 'vue'
import { ApiError } from '@/lib/api'
import { t, te } from '@/i18n'

export interface Toast { id: number; kind: 'ok' | 'error' | 'info'; text: string }
export const useToasts = defineStore('toasts', () => {
  const items = ref<Toast[]>([])
  let n = 0
  function push(kind: Toast['kind'], text: string, ms = kind === 'error' ? 7000 : 3500) {
    const id = ++n
    items.value.push({ id, kind, text })
    setTimeout(() => dismiss(id), ms)
  }
  const dismiss = (id: number) => { items.value = items.value.filter((t) => t.id !== id) }
  return { items, push, dismiss, ok: (t: string) => push('ok', t), error: (t: string) => push('error', t), info: (t: string) => push('info', t) }
})
// readable message from any error (API messages are written by the daemon, in Italian)
// the API error has a stable code: if a translation exists use it (in the chosen language), otherwise the daemon's Italian text
export const errText = (e: unknown) => (e instanceof ApiError && e.code && te(`apiErrors.${e.code}`) ? t(`apiErrors.${e.code}`) : e instanceof Error ? e.message : String(e))
