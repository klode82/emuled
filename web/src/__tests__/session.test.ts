import { createPinia, setActivePinia } from 'pinia'
import { useSession } from '@/stores/session'
import { setLang } from '@/i18n'

const reply = (status: number, body: unknown) => vi.fn(async () => new Response(JSON.stringify(body), { status }))

describe('sessione', () => {
  beforeAll(() => setLang('it'))
  beforeEach(() => { localStorage.clear(); setActivePinia(createPinia()) })
  afterEach(() => vi.unstubAllGlobals())
  it('un token vecchio rimasto nel browser si dimentica senza errori (primo avvio dopo una reinstallazione)', async () => {
    localStorage.setItem('emuled.token', 'token-di-una-vecchia-installazione')
    vi.stubGlobal('fetch', reply(401, { error: 'token' }))
    const s = useSession(); await s.resume()
    expect(s.error).toBe(''); expect(s.ready).toBe(false); expect(localStorage.getItem('emuled.token')).toBeNull()
  })
  it('un token scritto dall\'utente e sbagliato da l\'errore', async () => {
    vi.stubGlobal('fetch', reply(401, { error: 'token' }))
    const s = useSession(); expect(await s.login('sbagliato')).toBe(false)
    expect(s.error).toBe('Token non valido.')
  })
})
