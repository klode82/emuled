// emuled REST API client. The token is a Bearer; on 401 listeners are notified (the session goes back to the login screen).
import { t } from '@/i18n'
import type { ImportRule, ImportScan, FsList, PortsCheck, LogPage, Category, Directories, Download, Parts, Source, ServerPriority, NewSearch, Preferences, PrefsPatchResult, Search, Server, Shared, Status, Uploads, KadState, Priority, ServerUpdate, KadBootstrap, Details, Content, FileNames, FileComments, StreamLink, TemplateList, Blocks } from '@/types'

export class ApiError extends Error {
  constructor(public status: number, message: string, public code = '') { super(message) }
}

type Json = Record<string, unknown> | unknown[]
const base = '/api/v1'

// Interface account (username + password): these three calls are public, they do not send the Bearer.
export interface AuthStatus { configured: boolean; setup_required: boolean }
export interface AuthSession { token: string; username: string; expires_in: number }
async function authReq<T>(method: string, path: string, body?: Json): Promise<T> {
  let res: Response
  try {
    res = await fetch(base + path, { method, headers: body ? { 'Content-Type': 'application/json' } : {}, body: body ? JSON.stringify(body) : undefined })
  } catch {
    throw new ApiError(0, t('errors.noDaemon'))
  }
  const text = await res.text()
  let data: unknown = null
  if (text) { try { data = JSON.parse(text) } catch { /* non-JSON response */ } }
  if (!res.ok) { const d = data as { error?: string; code?: string } | null; throw new ApiError(res.status, d?.error ?? (text || `Errore ${res.status}`), d?.code ?? '') }
  return data as T
}
export const authStatus = () => authReq<AuthStatus>('GET', '/auth/status')
export const authLogin = (username: string, password: string) => authReq<AuthSession>('POST', '/auth/login', { username, password })
export const authSetup = (setup_code: string, username: string, password: string) => authReq<AuthSession>('POST', '/auth/setup', { setup_code, username, password })

export class Api {
  onUnauthorized: () => void = () => {}
  constructor(public token: string) {}

  private async req<T>(method: string, path: string, body?: Json): Promise<T> {
    let res: Response
    try {
      res = await fetch(base + path, {
        method,
        headers: { Authorization: `Bearer ${this.token}`, ...(body ? { 'Content-Type': 'application/json' } : {}) },
        body: body ? JSON.stringify(body) : undefined,
      })
    } catch {
      throw new ApiError(0, t('errors.noDaemon'))
    }
    const text = await res.text()
    let data: unknown = null
    if (text) { try { data = JSON.parse(text) } catch { /* non-JSON response: the text is used */ } }
    if (res.status === 401) this.onUnauthorized()
    if (!res.ok) { const d = data as { error?: string; code?: string } | null; throw new ApiError(res.status, d?.error ?? (text || `Errore ${res.status}`), d?.code ?? '') }
    return data as T
  }
  get = <T>(p: string) => this.req<T>('GET', p)
  post = <T>(p: string, b?: Json) => this.req<T>('POST', p, b ?? {})
  patch = <T>(p: string, b: Json) => this.req<T>('PATCH', p, b)
  put = <T>(p: string, b: Json) => this.req<T>('PUT', p, b)
  del = <T>(p: string) => this.req<T>('DELETE', p)

  status = () => this.get<Status>('/status')
  downloads = () => this.get<Download[]>('/downloads')
  addDownload = (b: { link: string; category?: number } | { hash: string; name: string; size: number; category?: number }) => this.post<Download>('/downloads', b)
  pause = (h: string) => this.post<Download>(`/downloads/${h}/pause`)
  resume = (h: string) => this.post<Download>(`/downloads/${h}/resume`)
  stop = (h: string) => this.post<Download>(`/downloads/${h}/stop`)
  remove = (h: string) => this.del<Download>(`/downloads/${h}`)
  clearCompleted = () => this.post<{ removed: number }>('/downloads/clear-completed')
  sources = (h: string) => this.get<Source[]>(`/downloads/${h}/sources`)
  findSources = (h: string) => this.post<{ server: boolean; kad: boolean; paused: boolean; wait_s: number }>(`/downloads/${h}/find-sources`)
  blocks = (h: string) => this.get<Blocks>(`/downloads/${h}/blocks`)
  parts = (h: string) => this.get<Parts>(`/downloads/${h}/parts`)
  templates = () => this.get<TemplateList>('/templates')
  rescanTemplates = () => this.post<TemplateList>('/templates/rescan')
  streamLink = (h: string) => this.post<StreamLink>(`/downloads/${h}/stream`)
  names = (h: string) => this.get<FileNames>(`/downloads/${h}/names`)
  comments = (h: string) => this.get<FileComments>(`/downloads/${h}/comments`)
  searchKadComments = (h: string) => this.post<{ started: boolean }>(`/downloads/${h}/comments/search-kad`)
  patchDownload = (h: string, b: { category?: number; priority?: Priority; name?: string }) => this.patch<Download>(`/downloads/${h}`, b)

  servers = () => this.get<Server[]>('/servers')
  addServer = (b: { address: string; name?: string; priority?: ServerPriority; static?: boolean }) => this.post<Server>('/servers', b)
  patchServer = (addr: string, b: { priority?: ServerPriority; static?: boolean; name?: string }) => this.patch<Server>(`/servers/${addr}`, b)
  removeServer = (addr: string) => this.del<Server>(`/servers/${addr}`)
  removeAllServers = () => this.del<{ removed: number }>('/servers')
  content = (hash: string) => this.get<Content>(`/downloads/${hash}/content`)
  contentStart = (hash: string) => this.post<Content>(`/downloads/${hash}/content`)
  details = (hash: string) => this.get<Details>(`/downloads/${hash}/details`)
  kadBootstrapState = () => this.get<KadBootstrap>('/kad/bootstrap')
  kadBootstrap = (b: { url?: string; address?: string }) => this.post<KadBootstrap>('/kad/bootstrap', b)
  serverUrls = () => this.get<string[]>('/servers/urls')
  serverUpdateState = () => this.get<ServerUpdate>('/servers/update')
  updateServers = (url: string) => this.post<ServerUpdate>('/servers/update', { url })
  connectServer = (address?: string) => this.post<unknown>('/servers/connect', address ? { address } : {})
  disconnectServer = () => this.post<unknown>('/servers/disconnect')

  startSearch = (b: NewSearch) => this.post<Search>('/searches', b as unknown as Json)
  searches = () => this.get<Search[]>('/searches')
  search = (id: number) => this.get<Search>(`/searches/${id}`)
  refreshSearch = (id: number) => this.post<Search>(`/searches/${id}/refresh`, {})
  cancelSearch = (id: number) => this.post<Search>(`/searches/${id}/cancel`, {})
  removeSearch = (id: number) => this.del<Search>(`/searches/${id}`)

  shared = () => this.get<Shared[]>('/shared')
  reloadShared = () => this.post<{ count: number; hashing: number }>('/shared/reload')
  setSharedPriority = (h: string, priority: Priority) => this.post<Shared>(`/shared/${h}/priority`, { priority })
  uploads = () => this.get<Uploads>('/uploads')

  kad = () => this.get<KadState>('/kad')
  kadConnect = () => this.post<unknown>('/kad/connect')
  kadDisconnect = () => this.post<unknown>('/kad/disconnect')
  kadRecheck = () => this.post<unknown>('/kad/recheck-firewall')

  preferences = () => this.get<Preferences>('/preferences')
  patchPreferences = (b: Preferences) => this.patch<PrefsPatchResult>('/preferences', b)
  log = (q: { after?: number; limit?: number; level?: string }) => this.get<LogPage>('/log?' + new URLSearchParams(Object.entries(q).filter(([, v]) => v !== undefined).map(([k, v]) => [k, String(v)])).toString())
  clearLog = () => this.del<{ removed: number }>('/log')
  fs = (path?: string, hidden = false) => this.get<FsList>(`/fs?${path ? `path=${encodeURIComponent(path)}&` : ''}hidden=${hidden ? 1 : 0}`)
  fsMkdir = (path: string) => this.post<{ path: string }>('/fs/mkdir', { path })
  setupStatus = () => this.get<{ pending: boolean }>('/setup')
  importStatus = () => this.get<{ available: boolean; imported: boolean; candidates?: string[] }>('/setup/import')
  importScan = (path: string, rules?: ImportRule[]) => this.post<ImportScan>('/setup/import/scan', { path, rules })
  importRun = (path: string, rules: ImportRule[]) => this.post<{ staged: boolean; restarting: boolean }>('/setup/import', { path, rules })
  portsTestUrl = (lang: number) => this.get<{ url: string; tcp: number; udp: number; obfuscation: boolean }>(`/ports/test-url?lang=${lang}`)
  systemRestart = () => this.post<{ restarting: boolean }>('/system/restart')
  setupDone = () => this.post<{ pending: boolean }>('/setup/done')
  portsCheck = (tcp: number, udp: number) => this.get<PortsCheck>(`/ports/check?tcp=${tcp}&udp=${udp}`)
  directories = () => this.get<Directories>('/directories')
  putDirectories = (b: Partial<Directories>) => this.put<{ directories: Directories; saved: boolean }>('/directories', b as Json)
  categories = () => this.get<Category[]>('/categories')
  addCategory = (b: Partial<Category>) => this.post<Category>('/categories', b as Json)
  patchCategory = (id: number, b: Partial<Category>) => this.patch<Category>(`/categories/${id}`, b as Json)
  removeCategory = (id: number) => this.del<Category>(`/categories/${id}`)
}
