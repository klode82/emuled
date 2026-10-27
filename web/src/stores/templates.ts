// Interface templates: the list (from the daemon), the one in use (from the /t/<id>/ address) and the choice, remembered ONLY in this browser (localStorage, key
// emuled.template; with no choice the daemon's entry page leads to "console"). See docs/TEMPLATES.md.
import { defineStore } from 'pinia'
import { ref } from 'vue'
import type { TemplateInfo, TemplateList } from '@/types'
import { errText, useToasts } from './toasts'
import { t } from '@/i18n'
import { useSession } from './session'
import { useUi } from './ui'

export const TEMPLATE_KEY = 'emuled.template'
export const DEFAULT_TEMPLATE = 'console'

// the id of the template the page was opened from (/t/<id>/...); null if it is not a template page (e.g. the dev server)
export function currentTemplateId(path: string = location.pathname): string | null {
  const m = /^\/t\/([a-z0-9][a-z0-9-]{1,31})\//.exec(path)
  return m ? m[1] : null
}
const store = (v: string | null) => { try { v === null ? localStorage.removeItem(TEMPLATE_KEY) : localStorage.setItem(TEMPLATE_KEY, v) } catch { /* storage not available */ } }

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
  // switch to the chosen template: remember the choice (the default, console, is "no choice") and reopen that template's page on the same screen
  function choose(id: string, go: (url: string) => void = (u) => location.assign(u)) {
    store(id === DEFAULT_TEMPLATE ? null : id)
    go(`/t/${id}/${location.hash}`)
  }
  // a non-built-in template is a program running in the page: ask for confirmation before using it
  async function confirmAndChoose(tpl: TemplateInfo) {
    if (tpl.reserved || await useUi().ask({ title: t('template.warnTitle'), text: t('template.warnText', { name: tpl.name }), confirm: t('template.use'), danger: true })) choose(tpl.id)
  }
  return { list, dirs, loaded, current, load, rescan, choose, confirmAndChoose }
})
