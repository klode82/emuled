// Description of the API preferences (docs/APIDOC.md, Preferences section): group, type, limits. Label and explanation are in i18n: prefs.items.<key>.label / .hint
export interface PrefMeta { key: string; group: string; type: 'bool' | 'int' | 'string'; optional?: boolean; min?: number; max?: number; unit?: string; restart?: boolean }

export const PREF_GROUPS = ['general', 'connection', 'network', 'security', 'downloads', 'diagnostics'] as const

export const PREFS: PrefMeta[] = [
  { key: 'nick', group: 'general', type: 'string' },

  { key: 'tcp_port', group: 'connection', type: 'int', min: 1, max: 65535, restart: true },
  { key: 'udp_port', group: 'connection', type: 'int', min: 0, max: 65535, restart: true },
  { key: 'upnp_enabled', group: 'connection', type: 'bool' },
  { key: 'upnp_close_on_exit', group: 'connection', type: 'bool' },
  { key: 'max_download_kbps', group: 'connection', type: 'int', min: 0, max: 1000000, unit: 'kB/s' },
  { key: 'max_upload_kbps', group: 'connection', type: 'int', min: 0, max: 1000000, unit: 'kB/s' },
  { key: 'max_connections', group: 'connection', type: 'int', min: 1, max: 100000 },
  { key: 'max_connections_per_5s', group: 'connection', type: 'int', min: 1, max: 1000 },
  { key: 'max_sources_per_file', group: 'connection', type: 'int', min: 1, max: 100000 },

  { key: 'network_ed2k', group: 'network', type: 'bool' },
  { key: 'network_kademlia', group: 'network', type: 'bool', restart: true },
  { key: 'auto_connect', group: 'network', type: 'bool', restart: true },
  { key: 'reconnect', group: 'network', type: 'bool' },
  { key: 'add_servers_from_server', group: 'network', type: 'bool' },
  { key: 'add_servers_from_clients', group: 'network', type: 'bool' },
  { key: 'use_server_priorities', group: 'network', type: 'bool' },
  { key: 'safe_server_connect', group: 'network', type: 'bool' },
  { key: 'static_servers_only', group: 'network', type: 'bool' },
  { key: 'dead_server_retries', group: 'network', type: 'int', min: 1, max: 100 },

  { key: 'crypt_supported', group: 'security', type: 'bool', restart: true },
  { key: 'crypt_requested', group: 'security', type: 'bool', restart: true },
  { key: 'crypt_required', group: 'security', type: 'bool', restart: true },
  { key: 'secure_ident', group: 'security', type: 'bool', restart: true },
  { key: 'credit_system', group: 'security', type: 'bool' },

  { key: 'add_new_downloads_paused', group: 'downloads', type: 'bool' },
  { key: 'auto_up_priority', group: 'downloads', type: 'bool' },
  { key: 'auto_down_priority', group: 'downloads', type: 'bool' },
  { key: 'transfer_full_chunks', group: 'downloads', type: 'bool' },
  { key: 'remove_finished_downloads', group: 'downloads', type: 'bool' },
  { key: 'comment_filter', group: 'downloads', type: 'string', optional: true },

  { key: 'verbose_log', group: 'diagnostics', type: 'bool' },
  { key: 'save_log_to_disk', group: 'diagnostics', type: 'bool' },
  { key: 'save_debug_to_disk', group: 'diagnostics', type: 'bool' },
]
