import { defineStore } from 'pinia'
import { ref } from 'vue'
import type { Server } from '@/types'
import { errText, useToasts } from './toasts'
import { t } from '@/i18n'
import type { ServerPriority } from '@/types'
import { useSession } from './session'
import { useCore } from './core'

export const useServers = defineStore('servers', () => {
  const list = ref<Server[]>([])
  const loading = ref(false)
  const api = () => useSession().getApi()
  async function load() { loading.value = true; try { list.value = await api().servers() } catch (e) { useToasts().error(errText(e)) } finally { loading.value = false } }
  async function connect(address?: string) {
    try { await api().connectServer(address); useToasts().info(address ? t('servers.connectingTo', { address }) : t('servers.searching')); setTimeout(() => void useCore().refresh(), 1500) } catch (e) { useToasts().error(errText(e)) }
  }
  // "Riconnetti": scollega e ricollega allo stesso server, che rifa' anche la richiesta di fonti
  async function reconnect(address: string) {
    try { await api().disconnectServer(); await connect(address) } catch (e) { useToasts().error(errText(e)) }
  }
  async function disconnect() { try { await api().disconnectServer(); await useCore().refresh(); useToasts().ok(t('servers.disconnected')) } catch (e) { useToasts().error(errText(e)) } }
  async function add(b: { address: string; name?: string; priority?: ServerPriority; static?: boolean }): Promise<boolean> {
    try { await api().addServer(b); await load(); useToasts().ok(t('servers.added')); return true } catch (e) { useToasts().error(errText(e)); return false }
  }
  async function edit(addr: string, b: { priority?: ServerPriority; static?: boolean; name?: string }) {
    try { await api().patchServer(addr, b); await load() } catch (e) { useToasts().error(errText(e)) }
  }
  async function remove(addr: string) {
    try { await api().removeServer(addr); await load(); useToasts().ok(t('servers.removed')) } catch (e) { useToasts().error(errText(e)) }
  }
  // aggiorna la lista dei server da un indirizzo web (server.met): parte il download, poi si guarda lo stato finche' non finisce
  const urls = ref<string[]>([]); const updating = ref(false)
  async function loadUrls() { try { urls.value = await api().serverUrls() } catch { /* senza elenco si scrive a mano */ } }
  async function updateFromUrl(url: string) {
    updating.value = true
    try {
      let r = await api().updateServers(url.trim())
      for (let i = 0; i < 90 && r.state === 'running'; i++) { await new Promise((ok) => setTimeout(ok, 1000)); r = await api().serverUpdateState() }
      if (r.state === 'done') { await load(); await loadUrls(); useToasts().ok(t('servers.updated', { n: r.added ?? 0, total: r.total ?? 0 })) }
      else useToasts().error(r.state === 'failed' ? t('servers.updateFailed', { why: r.error ?? '' }) : t('servers.updateSlow'))
    } catch (e) { useToasts().error(errText(e)) } finally { updating.value = false }
  }
  return { urls, updating, loadUrls, updateFromUrl, reconnect, list, loading, load, connect, disconnect, add, edit, remove }
})
