// Sessione: token, collegamento live (WebSocket) e caricamento iniziale degli altri store. Se il WebSocket cade si ricarica tutto alla riconnessione
// e, finche' e' giu', si interroga l'API ogni 5 secondi.
import { defineStore } from 'pinia'
import { ref } from 'vue'
import { Api, ApiError, authLogin, authSetup } from '@/lib/api'
import { errText } from './toasts'
import { EventsClient, type LinkState } from '@/lib/events'
import type { ServerEvent } from '@/types'
import { t } from '@/i18n'
import { useCore } from './core'
import { useDownloads } from './downloads'
import { useSearch } from './search'
import { useLog } from './log'

const KEY = 'emuled.token'
const read = () => { try { return localStorage.getItem(KEY) ?? '' } catch { return '' } }
const write = (v: string) => { try { v ? localStorage.setItem(KEY, v) : localStorage.removeItem(KEY) } catch { /* archivio non disponibile */ } }

export const useSession = defineStore('session', () => {
  const token = ref(read())
  const ready = ref(false)
  const link = ref<LinkState>('closed')
  const error = ref('')
  const busy = ref(false)
  let api: Api | null = null
  let events: EventsClient | null = null
  let poll: ReturnType<typeof setInterval> | null = null

  function getApi(): Api { if (!api) throw new Error(t('errors.notConnected')); return api }

  function dispatch(e: ServerEvent) {
    useCore().apply(e); useDownloads().apply(e); useSearch().apply(e); useLog().apply(e)
  }
  async function refreshAll() {
    await Promise.allSettled([useCore().refresh(), useDownloads().load(), useLog().sync()])
  }
  function onLink(s: LinkState) {
    link.value = s
    if (s === 'open') { void refreshAll(); stopPoll() } else if (ready.value) startPoll()
  }
  function startPoll() { if (!poll) poll = setInterval(() => void refreshAll(), 5000) }
  function stopPoll() { if (poll) { clearInterval(poll); poll = null } }

  async function login(raw: string, quiet = false): Promise<boolean> {		// quiet: tentativo con il token salvato, non scritto dall'utente
    busy.value = true; error.value = ''
    const candidate = new Api(raw.trim())
    try {
      await candidate.status()
    } catch (e) {
      busy.value = false
      if (quiet && e instanceof ApiError && e.status === 401) { token.value = ''; write(''); return false }		// token di una vecchia installazione o scaduto: si dimentica, senza dare un errore che l'utente non ha causato
      error.value = e instanceof ApiError && e.status === 401 ? t('login.invalidToken') : e instanceof Error ? e.message : String(e)
      return false
    }
    api = candidate; api.onUnauthorized = () => logout(t('login.sessionExpired'))
    token.value = raw.trim(); write(token.value); ready.value = true; busy.value = false
    events?.stop(); events = new EventsClient(token.value, dispatch, onLink); events.start()
    await refreshAll()
    return true
  }
  // Accesso con nome utente e password (o primo account con il codice di impostazione): il demone risponde con un gettone di sessione, che da qui in poi
  // si usa come il token del file (stessa chiave nel browser, stesso Bearer: il contratto dei template non cambia).
  async function loginUser(username: string, password: string): Promise<boolean> {
    busy.value = true; error.value = ''
    try { const r = await authLogin(username.trim(), password); busy.value = false; return await login(r.token) }
    catch (e) { busy.value = false; error.value = errText(e); return false }
  }
  async function setupAccount(code: string, username: string, password: string): Promise<boolean> {
    busy.value = true; error.value = ''
    try { const r = await authSetup(code.trim(), username.trim(), password); busy.value = false; return await login(r.token) }
    catch (e) { busy.value = false; error.value = errText(e); return false }
  }
  async function resume() { if (token.value && !ready.value) await login(token.value, true) }
  // Riavvia il demone e aspetta che torni; poi ricarica la pagina. Ritorna '' se e' andato a buon fine, altrimenti il messaggio d'errore.
  async function restartDaemon(): Promise<string> {
    const a = getApi()
    try {
      const before = (await a.status()).uptime_s
      await a.systemRestart()
      const t0 = Date.now(); let sawDown = false
      await new Promise((r) => setTimeout(r, 1500))
      while (Date.now() - t0 < 90000) {
        try { const s = await a.status(); if (sawDown || s.uptime_s < before) { location.reload(); return '' } }
        catch { sawDown = true }		// sta ripartendo
        await new Promise((r) => setTimeout(r, 1000))
      }
      return t('prefs.daemon.restartFailed')
    } catch (e) { return e instanceof Error ? e.message : String(e) }
  }
  function logout(msg = '') {
    events?.stop(); events = null; stopPoll(); api = null; ready.value = false; link.value = 'closed'; token.value = ''; write(''); error.value = msg
  }
  return { token, ready, link, error, busy, getApi, login, loginUser, setupAccount, resume, logout, restartDaemon }
})
