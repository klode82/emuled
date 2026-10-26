import { defineStore } from 'pinia'
import { ref } from 'vue'
import type { Priority, Shared, Uploads } from '@/types'
import { errText, useToasts } from './toasts'
import { t } from '@/i18n'
import { useSession } from './session'

export const useShared = defineStore('shared', () => {
  const list = ref<Shared[]>([])
  const uploads = ref<Uploads>({ uploading: [], waiting: [], speed: 0 })
  const loading = ref(false)
  const api = () => useSession().getApi()
  async function load() {
    loading.value = true
    try { [list.value, uploads.value] = await Promise.all([api().shared(), api().uploads()]) } catch (e) { useToasts().error(errText(e)) } finally { loading.value = false }
  }
  async function loadUploads() { try { uploads.value = await api().uploads() } catch { /* si riprova al giro dopo */ } }
  async function reload() {
    try { const r = await api().reloadShared(); useToasts().ok(t('shared.reloaded', { n: r.count, hashing: r.hashing })); await load() } catch (e) { useToasts().error(errText(e)) }
  }
  async function setPriority(h: string, p: Priority) {
    try { const s = await api().setSharedPriority(h, p); list.value = list.value.map((x) => (x.hash === h ? s : x)) } catch (e) { useToasts().error(errText(e)) }
  }
  return { list, uploads, loading, load, loadUploads, reload, setPriority }
})
