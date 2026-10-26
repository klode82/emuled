// I template d'interfaccia: l'elenco (dal demone), quello in uso (dall'indirizzo /t/<id>/) e la scelta, che si ricorda SOLO in questo browser (localStorage, chiave
// emuled.template; senza scelta la pagina d'ingresso del demone porta a "console"). Vedi docs/TEMPLATES.md.
import { defineStore } from 'pinia'
import { ref } from 'vue'
import type { TemplateInfo, TemplateList } from '@/types'
import { errText, useToasts } from './toasts'
import { t } from '@/i18n'
import { useSession } from './session'
import { useUi } from './ui'

export const TEMPLATE_KEY = 'emuled.template'
export const DEFAULT_TEMPLATE = 'console'

// l'id del template da cui e' stata aperta la pagina (/t/<id>/...); null se non e' una pagina di template (per esempio il server di sviluppo)
export function currentTemplateId(path: string = location.pathname): string | null {
  const m = /^\/t\/([a-z0-9][a-z0-9-]{1,31})\//.exec(path)
  return m ? m[1] : null
}
const store = (v: string | null) => { try { v === null ? localStorage.removeItem(TEMPLATE_KEY) : localStorage.setItem(TEMPLATE_KEY, v) } catch { /* archivio non disponibile */ } }

export const useTemplates = defineStore('templates', () => {
  const list = ref<TemplateInfo[]>([])
  const dirs = ref<TemplateList['dirs']>({})
  const loaded = ref(false)
  const current = currentTemplateId()
  const api = () => useSession().getApi()
  async function load() {
    try { const r = await api().templates(); list.value = r.templates; dirs.value = r.dirs; loaded.value = true } catch (e) { useToasts().error(errText(e)) }
  }
  async function rescan() {
    try { const r = await api().rescanTemplates(); list.value = r.templates; dirs.value = r.dirs; loaded.value = true; useToasts().ok(t('template.rescanned', { n: r.templates.filter((x) => x.valid).length })) } catch (e) { useToasts().error(errText(e)) }
  }
  // passa al template scelto: ricorda la scelta (il predefinito, console, e' "nessuna scelta") e riapre la pagina di quel template nella stessa schermata
  function choose(id: string, go: (url: string) => void = (u) => location.assign(u)) {
    store(id === DEFAULT_TEMPLATE ? null : id)
    go(`/t/${id}/${location.hash}`)
  }
  // un template che non e' di serie e' un programma che gira nella pagina: prima di usarlo si chiede conferma
  async function confirmAndChoose(tpl: TemplateInfo) {
    if (tpl.reserved || await useUi().ask({ title: t('template.warnTitle'), text: t('template.warnText', { name: tpl.name }), confirm: t('template.use'), danger: true })) choose(tpl.id)
  }
  return { list, dirs, loaded, current, load, rescan, choose, confirmAndChoose }
})
