// Preferenze dell'interfaccia (tema, modello) salvate nel browser, finestra "Aggiungi" e conferme
import { defineStore } from 'pinia'
import { ref, watch } from 'vue'

export type Theme = 'system' | 'light' | 'dark'
export interface ConfirmItem { label: string; info?: string }
export interface ConfirmReq { title: string; text: string; confirm: string; danger?: boolean; list?: ConfirmItem[]; resolve: (ok: boolean) => void }

const get = (k: string, d: string) => { try { return localStorage.getItem(k) ?? d } catch { return d } }
const set = (k: string, v: string) => { try { localStorage.setItem(k, v) } catch { /* archivio non disponibile */ } }

export const useUi = defineStore('ui', () => {
  const theme = ref<Theme>(get('emuled.theme', 'system') as Theme)
  const addOpen = ref(false)
  const addText = ref('')
  const confirmReq = ref<ConfirmReq | null>(null)

  function applyTheme() {
    if (theme.value === 'system') delete document.documentElement.dataset.theme
    else document.documentElement.dataset.theme = theme.value
  }
  watch(theme, (v) => { set('emuled.theme', v); applyTheme() }, { immediate: true })

  function openAdd(text = '') { addText.value = text; addOpen.value = true }
  function ask(r: Omit<ConfirmReq, 'resolve'>): Promise<boolean> {
    return new Promise((resolve) => { confirmReq.value = { ...r, resolve: (ok) => { confirmReq.value = null; resolve(ok) } } })
  }
  return { theme, addOpen, addText, confirmReq, openAdd, ask }
})
