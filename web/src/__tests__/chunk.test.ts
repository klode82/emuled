import { createRouter, createMemoryHistory } from 'vue-router'
import { installChunkReload, isChunkError } from '@/lib/chunkReload'

describe('ricarica dopo un aggiornamento', () => {
  it('riconosce l\'errore dei file delle schermate che non esistono piu\' (Chrome, Firefox, Safari)', () => {
    expect(isChunkError(new TypeError('Failed to fetch dynamically imported module: http://x/assets/KadView-abc.js'))).toBe(true)
    expect(isChunkError(new Error('error loading dynamically imported module'))).toBe(true)
    expect(isChunkError(new Error('Importing a module script failed.'))).toBe(true)
    expect(isChunkError(new Error('altro errore'))).toBe(false)
  })
  it('ricarica la pagina una volta sola per lo stesso indirizzo, e riparte da capo dopo una navigazione riuscita', async () => {
    sessionStorage.clear()
    let fail = true
    const router = createRouter({ history: createMemoryHistory(), routes: [{ path: '/', component: { template: '<i/>' } }, { path: '/kad', component: () => (fail ? Promise.reject(new TypeError('Failed to fetch dynamically imported module')) : Promise.resolve({ template: '<i/>' })) }] })
    const urls: string[] = []
    installChunkReload(router, (u) => urls.push(u))
    await router.push('/').catch(() => {})
    await router.push('/kad').catch(() => {}); expect(urls).toEqual(['/kad'])
    await router.push('/kad').catch(() => {}); expect(urls).toEqual(['/kad'])			// ancora rotto: niente giro in tondo
    fail = false; await router.push('/').catch(() => {}); await router.push('/kad').catch(() => {}); expect(urls).toEqual(['/kad'])
  })
})
