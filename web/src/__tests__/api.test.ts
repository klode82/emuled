import { Api, ApiError, authLogin, authSetup, authStatus } from '@/lib/api'
import { EventsClient } from '@/lib/events'
import { setLang } from '@/i18n'

const reply = (status: number, body: unknown) => vi.fn(async () => new Response(typeof body === 'string' ? body : JSON.stringify(body), { status }))

describe('Api', () => {
  beforeAll(() => setLang('it'))
  afterEach(() => vi.unstubAllGlobals())
  it('manda il token e restituisce il JSON', async () => {
    const f = reply(200, { pong: true }); vi.stubGlobal('fetch', f)
    expect(await new Api('T').get('/ping')).toEqual({ pong: true })
    expect((f.mock.calls[0] as unknown[])[1]).toMatchObject({ method: 'GET', headers: { Authorization: 'Bearer T' } })
  })
  it('errore dell\'API: messaggio del demone e codice', async () => {
    vi.stubGlobal('fetch', reply(409, { error: 'il file e\' gia\' in download' }))
    await expect(new Api('T').addDownload({ link: 'x' })).rejects.toMatchObject({ status: 409, message: 'il file e\' gia\' in download' })
  })
  it('401 avvisa la sessione', async () => {
    vi.stubGlobal('fetch', reply(401, { error: 'token' })); const a = new Api('T'); a.onUnauthorized = vi.fn()
    await expect(a.status()).rejects.toBeInstanceOf(ApiError); expect(a.onUnauthorized).toHaveBeenCalled()
  })
  it('demone spento: errore comprensibile', async () => {
    vi.stubGlobal('fetch', vi.fn(async () => { throw new TypeError('failed') }))
    await expect(new Api('T').status()).rejects.toMatchObject({ status: 0, message: expect.stringContaining('non risponde') })
  })
  it('PATCH delle preferenze manda solo il corpo dato', async () => {
    const f = reply(200, { preferences: {}, saved: false, restart_required: [] }); vi.stubGlobal('fetch', f)
    await new Api('T').patchPreferences({ nick: 'a' })
    expect((f.mock.calls[0] as unknown[])[1]).toMatchObject({ method: 'PATCH', body: '{"nick":"a"}' })
  })
})

class FakeWs { static all: FakeWs[] = []; onopen?: () => void; onclose?: () => void; onmessage?: (m: { data: string }) => void; onerror?: () => void; constructor(public url: string) { FakeWs.all.push(this) } close() { this.onclose?.() } }

describe('EventsClient', () => {
  beforeEach(() => { vi.useFakeTimers(); FakeWs.all = [] })
  afterEach(() => vi.useRealTimers())
  it('token nell\'indirizzo, eventi decodificati, messaggi rotti ignorati', () => {
    const got: unknown[] = []; const c = new EventsClient('a b', (e) => got.push(e), () => {}, (u) => new FakeWs(u) as unknown as WebSocket); c.start()
    expect(FakeWs.all[0].url).toContain('/api/v1/events?token=a%20b')
    FakeWs.all[0].onmessage?.({ data: '{"type":"stats","data":{"downloads":1}}' }); FakeWs.all[0].onmessage?.({ data: 'non json' })
    expect(got).toEqual([{ type: 'stats', data: { downloads: 1 } }])
  })
  it('si riconnette con attesa crescente e si ferma con stop()', () => {
    const states: string[] = []; const c = new EventsClient('t', () => {}, (s) => states.push(s), (u) => new FakeWs(u) as unknown as WebSocket); c.start()
    FakeWs.all[0].close(); vi.advanceTimersByTime(999); expect(FakeWs.all).toHaveLength(1); vi.advanceTimersByTime(1); expect(FakeWs.all).toHaveLength(2)
    FakeWs.all[1].close(); vi.advanceTimersByTime(1999); expect(FakeWs.all).toHaveLength(2); vi.advanceTimersByTime(1); expect(FakeWs.all).toHaveLength(3)
    c.stop(); vi.advanceTimersByTime(60000); expect(FakeWs.all).toHaveLength(3); expect(states).toContain('closed')
  })
  it('account: le chiamate di accesso non mandano il Bearer e portano il codice dell\'errore', async () => {
    const f = reply(200, { token: 's1.ab.cd', username: 'mario', expires_in: 100 }); vi.stubGlobal('fetch', f)
    expect(await authLogin('mario', 'segreta-123')).toMatchObject({ token: 's1.ab.cd' })
    const [url, init] = f.mock.calls[0] as unknown as [string, { method: string; headers: Record<string, string>; body: string }]
    expect(url).toBe('/api/v1/auth/login'); expect(init.method).toBe('POST'); expect(init.headers.Authorization).toBeUndefined()
    expect(JSON.parse(init.body)).toEqual({ username: 'mario', password: 'segreta-123' })
    vi.stubGlobal('fetch', reply(401, { error: 'nome utente o password errati', code: 'bad_credentials' }))
    await expect(authLogin('mario', 'x')).rejects.toMatchObject({ status: 401, code: 'bad_credentials' })
    vi.stubGlobal('fetch', reply(429, { error: 'troppi tentativi sbagliati: riprova fra un minuto', code: 'too_many_attempts' }))
    await expect(authLogin('mario', 'x')).rejects.toMatchObject({ status: 429, code: 'too_many_attempts' })
  })
  it('account: stato e primo account', async () => {
    vi.stubGlobal('fetch', reply(200, { configured: false, setup_required: true }))
    expect(await authStatus()).toEqual({ configured: false, setup_required: true })
    const f = reply(201, { token: 't', username: 'mario', expires_in: 1 }); vi.stubGlobal('fetch', f)
    await authSetup('CODICE', 'mario', 'segreta-123')
    expect(JSON.parse(((f.mock.calls[0] as unknown as [string, { body: string }])[1]).body)).toEqual({ setup_code: 'CODICE', username: 'mario', password: 'segreta-123' })
  })
  it('Wizard di primo avvio: stato, fine e controllo delle porte', async () => {
    let f = reply(200, { pending: true }); vi.stubGlobal('fetch', f)
    expect(await new Api('T').setupStatus()).toEqual({ pending: true })
    expect((f.mock.calls[0] as unknown as [string])[0]).toBe('/api/v1/setup')
    f = reply(200, { pending: false }); vi.stubGlobal('fetch', f)
    await new Api('T').setupDone(); expect(((f.mock.calls[0] as unknown as [string, { method: string }])[1]).method).toBe('POST')
    f = reply(200, { tcp: { port: 55662, valid: true, free: true, own: false } }); vi.stubGlobal('fetch', f)
    expect((await new Api('T').portsCheck(55662, 55672)).tcp?.free).toBe(true)
    expect((f.mock.calls[0] as unknown as [string])[0]).toBe('/api/v1/ports/check?tcp=55662&udp=55672')
  })
})
