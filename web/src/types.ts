// Forma delle risposte dell'API di emuled (docs/openapi.json). Aggiornare insieme.
export type Priority = 'auto' | 'very_low' | 'low' | 'normal' | 'high' | 'release'
export type DownloadStatus =
  | 'ready' | 'empty' | 'waiting_for_hash' | 'hashing' | 'error' | 'insufficient_disk_space' | 'unknown' | 'paused' | 'completing' | 'complete'

export interface Download {
  hash: string; name: string; size: number; completed: number; percent: number
  status: DownloadStatus; stopped: boolean; sources: number; sources_transferring: number
  sources_other_files: number; best_queue_rank: number
  file_op?: 'none' | 'hashing' | 'copying' | 'uncompressing' | 'import_parts'; file_op_percent?: number   // operazione sul file in corso (barra verde)
  speed: number; priority: Priority; category: number; part_file: string
  completed_at?: number; path?: string   // solo per i completati
}
export interface Server {
  name: string; description: string; address: string; ip: string; port: number
  users: number; files: number; max_users: number; ping_ms: number; failed_count: number; static: boolean; priority: ServerPriority
}
export type ServerPriority = 'low' | 'normal' | 'high'
export interface Ed2kState { connected: boolean; connecting: boolean; id?: number; high_id?: boolean; server?: Server }
export interface KadState {
  indexed?: { keywords: number; sources: number; notes: number }
  published?: { files: number; sources: number; notes: number }
  running: boolean; connected: boolean; tcp_firewalled?: boolean; udp_firewalled?: boolean; udp_verified?: boolean; contacts?: number; users?: number
}
export interface Transfers { downloads: number; download_speed: number; upload_speed: number; uploading: number; waiting: number }
export interface TemplateInfo {
  id: string; dir_name: string; name: string; version: string; description: string; author: string; license: string; api: number
  source: 'system' | 'user' | 'dev'; valid: boolean; errors: string[]; reserved: boolean; url: string | null; preview: string | null
}
export interface TemplateList { templates: TemplateInfo[]; api_version: number; dirs: { system?: string; user?: string; extra?: string } }
export interface BlockSource { name: string; software: string; ip: string; port: number; speed: number; blocks: { start: number; end: number; transferred: number }[] }
export interface BlockActive { part: number; size: number; missing: number; receiving?: boolean; sources: BlockSource[] }
export interface Blocks { part_size: number; block_size: number; active: BlockActive[] }
export interface StreamLink { path: string; name: string; size: number; expires_in: number; mime: string }
export interface FileNames { current: string; names: { name: string; count: number }[] }
export interface FileComment { network: 'ed2k' | 'kad'; rating: number; comment: string; file_name: string; user: string }
export interface FileComments { comments: FileComment[]; kad_connected: boolean; kad_searching: boolean; filter: string }
export interface Details {
  name: string; part_met: string; part_met_path: string; hash: string; aich_hash: string | null
  aich_status: 'error' | 'empty' | 'untrusted' | 'trusted' | 'verified' | 'complete'
  hashset: { ed2k: boolean; aich: boolean }; size: number; size_on_disk: number; type: string
  parts: { total: number; available: number; available_pct: number }; status: string; stopped: boolean
  sources: { total: number; valid: number; transferring: number; no_needed_parts: number; other_file: number }
  speed: number; transferred: number
  compression_gain: { bytes: number; pct: number }; completed: { bytes: number; pct: number }; corrupted: { bytes: number; pct: number }; remaining: { bytes: number; pct: number }
  recovered_parts: number; added: number; active_seconds: number; last_seen_complete: number; last_received: number
}
export interface ContentTrack { codec: string | null; commercial?: string | null; profile?: string | null; bitrate: number | null; width?: number | null; height?: number | null; aspect?: number | null; fps?: number | null; channels?: number | null; sampling?: number | null; lang: string | null; title: string | null; default?: boolean; forced?: boolean }
export interface Content {
  state: 'idle' | 'running' | 'done' | 'failed' | 'unavailable'; reason?: string; recognized?: boolean
  general?: { format: string | null; size: number | null; duration_s: number | null; overall_bitrate: number | null; title: string | null; movie: string | null; date: string | null }
  video?: ContentTrack[]; audio?: ContentTrack[]; text?: ContentTrack[]
}
export interface KadBootstrap { state: 'idle' | 'running' | 'done' | 'failed'; kind?: 'url' | 'address'; target?: string; added?: number; contacts?: number; error?: string }
export interface ServerUpdate { state: 'idle' | 'running' | 'done' | 'failed'; url?: string; added?: number; total?: number; error?: string }
export interface Status {
  version: string; emuled_version: string; base_version: string; nick: string; uptime_s: number; ports: { tcp: number; udp: number }
  ed2k: Ed2kState; kad: KadState; transfers: Transfers
  upnp?: { enabled: boolean; state: 'off' | 'offline' | 'pending' | 'forwarded' | 'failed' | 'unavailable' }
}
export type SearchScope = 'server' | 'all' | 'kad' | 'global'
export interface SearchResult { hash: string; name: string; type: string; size: number; sources: number; complete_sources: number; answers: number }
export interface Search {
  id: number; query: string; scope: SearchScope; state: 'running' | 'finished' | 'cancelled'; elapsed_s: number
  result_count: number; servers_asked?: number; servers_skipped?: number; results?: SearchResult[]
}
export interface NewSearch { query: string; scope: SearchScope; ext?: string; type?: string; min_size?: number; max_size?: number; min_sources?: number; min_complete?: number }
export interface Shared {
  hash: string; name: string; size: number; path: string; part_file: boolean; priority: Priority
  requests: number; accepted: number; transferred: number; all_time_requests: number; all_time_accepted: number; all_time_transferred: number; queued: number
}
export interface UploadClient {
  name: string; ip: string; port: number; software: string; user_hash: string; file_hash: string; file_name?: string; speed: number; session_uploaded: number; score: number
}
export interface Uploads { uploading: UploadClient[]; waiting: UploadClient[]; speed: number }
export interface Category { id: number; title: string; comment: string; incoming_path: string; priority: number; auto_cat: string; regexp: string; color: number }
export type Preferences = Record<string, string | number | boolean>
export interface PrefsPatchResult { preferences: Preferences; saved: boolean; restart_required: string[] }

export interface LogLine { id: number; time: number; level: 'debug' | 'info' | 'warning' | 'error'; source: 'general' | 'server' | 'api'; text: string }
export interface LogPage { lines: LogLine[]; last_id: number; truncated: boolean }

// eventi del WebSocket /api/v1/events
export type ServerEvent =
  | { type: 'stats'; data: Transfers }
  | { type: 'server.state'; data: Ed2kState }
  | { type: 'kad.state'; data: KadState }
  | { type: 'download.added' | 'download.updated'; data: Download }
  | { type: 'download.removed'; data: { hash: string; name: string } }
  | { type: 'search.results'; data: Search }
  | { type: 'log'; data: { lines: LogLine[]; last_id: number } }

export type SourceState =
  | 'downloading' | 'on_queue' | 'connected' | 'connecting' | 'waiting_callback' | 'waiting_callback_kad' | 'requesting_hashset' | 'no_needed_parts'
  | 'too_many_connections' | 'low_to_low_id' | 'banned' | 'error' | 'remote_queue_full' | 'none'
export interface Source {
  other_file?: string   // fonte A4AF: il download per cui e' usata adesso
  name: string; software: string; ip: string; port: number; low_id: boolean; state: SourceState; from: 'server' | 'kad' | 'source_exchange' | 'passive' | 'link' | 'unknown'
  queue_rank: number; speed: number; session_downloaded: number; available_parts: number; part_count: number
}
export interface Part { index: number; size: number; missing: number; sources: number; state: 'empty' | 'partial' | 'complete' | 'corrupted'; verified?: boolean }
export interface Parts { part_size: number; size: number; count: number; hashset?: boolean; parts: Part[] }
export interface Directories { incoming: string; temp: string[]; shared: string[]; shared_missing?: string[] }
