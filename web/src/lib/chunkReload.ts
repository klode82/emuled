// Dopo un aggiornamento dell'interfaccia i file delle schermate (che si caricano al primo uso) cambiano nome: una pagina rimasta aperta prova a caricare quelli vecchi e
// la schermata "non si apre". Se succede, si ricarica la pagina (location.assign con solo "#/percorso" cambierebbe il frammento senza ricaricare nulla; una volta sola, per non girare in tondo se il demone e' spento).
import type { Router } from 'vue-router'

export const isChunkError = (e: unknown): boolean =>
  /Failed to fetch dynamically imported module|Importing a module script failed|error loading dynamically imported module|Unable to preload CSS/i.test(e instanceof Error ? e.message : String(e))

const KEY = 'emuled.reloaded'
export function installChunkReload(router: Router, reload: (url: string) => void = (u) => { history.replaceState(null, '', u); location.reload() }): void {
  router.onError((e, to) => {
    if (!isChunkError(e)) return
    try { if (sessionStorage.getItem(KEY) === to.fullPath) return; sessionStorage.setItem(KEY, to.fullPath) } catch { /* senza archivio si ricarica comunque */ }
    reload(router.resolve(to.fullPath).href)
  })
  router.afterEach(() => { try { sessionStorage.removeItem(KEY) } catch { /* ok */ } })
}
