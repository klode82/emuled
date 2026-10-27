// After an interface update the screens' files (loaded on first use) change name: a page left open tries to load the old ones and
// the screen "does not open". If that happens, the page is reloaded (location.assign with only "#/path" would change the fragment without reloading; only once, to avoid looping if the daemon is off).
import type { Router } from 'vue-router'

export const isChunkError = (e: unknown): boolean =>
  /Failed to fetch dynamically imported module|Importing a module script failed|error loading dynamically imported module|Unable to preload CSS/i.test(e instanceof Error ? e.message : String(e))

const KEY = 'emuled.reloaded'
export function installChunkReload(router: Router, reload: (url: string) => void = (u) => { history.replaceState(null, '', u); location.reload() }): void {
  router.onError((e, to) => {
    if (!isChunkError(e)) return
    try { if (sessionStorage.getItem(KEY) === to.fullPath) return; sessionStorage.setItem(KEY, to.fullPath) } catch { /* without storage it reloads anyway */ }
    reload(router.resolve(to.fullPath).href)
  })
  router.afterEach(() => { try { sessionStorage.removeItem(KEY) } catch { /* ok */ } })
}
