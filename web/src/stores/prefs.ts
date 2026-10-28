import { defineStore } from 'pinia'
import { computed, ref } from 'vue'
import type { Category, Directories, Preferences } from '@/types'
import { errText, useToasts } from './toasts'
import { t } from '@/i18n'
import { useSession } from './session'

export const usePrefs = defineStore('prefs', () => {
  const saved = ref<Preferences>({})
  const draft = ref<Preferences>({})
  const categories = ref<Category[]>([])
  const saving = ref(false)
  const restart = ref<string[]>([])
  const persisted = ref<boolean | null>(null)
  const dirs = ref<Directories | null>(null)
  const dirsSaved = ref<boolean | null>(null)
  const api = () => useSession().getApi()

  const changes = computed(() => Object.fromEntries(Object.entries(draft.value).filter(([k, v]) => v !== saved.value[k])) as Preferences)
  const dirty = computed(() => Object.keys(changes.value).length > 0)

  async function load() {
    try {
      saved.value = await api().preferences(); draft.value = { ...saved.value }; categories.value = await api().categories()
    } catch (e) { useToasts().error(errText(e)) }
  }
  function reset() { draft.value = { ...saved.value } }
  async function save() { await patch(changes.value) }
  // sends only these keys; the other unsaved changes of the page stay in the draft
  async function patch(b: Preferences) {
    saving.value = true
    try {
      const r = await api().patchPreferences(b)
      const keep = Object.fromEntries(Object.entries(changes.value).filter(([k]) => !(k in b)))
      saved.value = r.preferences; draft.value = { ...r.preferences, ...keep }; persisted.value = r.saved
      restart.value = [...new Set([...restart.value, ...r.restart_required])]
      useToasts().ok(t(r.saved ? 'prefs.saved' : 'prefs.appliedNotSaved'))
    } catch (e) { useToasts().error(errText(e)) } finally { saving.value = false }
  }
  async function loadDirs() { try { dirs.value = await api().directories() } catch (e) { useToasts().error(errText(e)) } }
  // returns the daemon error (a message) or '' on success: the screen shows it next to the field
  async function saveDirs(b: Partial<Directories>): Promise<string> {
    try { const r = await api().putDirectories(b); dirs.value = r.directories; dirsSaved.value = r.saved; useToasts().ok(t(r.saved ? 'dirs.saved' : 'dirs.appliedNotSaved')); return '' } catch (e) { return errText(e) }
  }
  async function loadCategories() { try { categories.value = await api().categories() } catch (e) { useToasts().error(errText(e)) } }
  async function addCategory(c: Partial<Category>) { await createCategory(c) }
  // return the category id (null on failure): used to move the chosen files into it right away
  async function createCategory(c: Partial<Category>): Promise<number | null> { try { const r = await api().addCategory(c); await loadCategories(); useToasts().ok(t('prefs.categoryCreated')); return r.id } catch (e) { useToasts().error(errText(e)); return null } }
  async function editCategory(id: number, c: Partial<Category>): Promise<number | null> { try { await api().patchCategory(id, c); await loadCategories(); useToasts().ok(t('prefs.categorySaved')); return id } catch (e) { useToasts().error(errText(e)); return null } }
  async function removeCategory(id: number) { try { await api().removeCategory(id); await loadCategories(); useToasts().ok(t('prefs.categoryDeleted')) } catch (e) { useToasts().error(errText(e)) } }
  return { dirs, dirsSaved, loadDirs, saveDirs, saved, draft, categories, saving, restart, persisted, changes, dirty, load, reset, save, patch, loadCategories, addCategory, createCategory, editCategory, removeCategory }
})
