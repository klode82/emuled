// Description of the API preferences (docs/APIDOC.md, Preferences section), split into the pages of eMule for Windows (PPg*.cpp, same order).
// Label and explanation are in i18n: prefs.items.<key>.label / .hint; choices: prefs.items.<key>.choices.<n>; pages: prefs.pages.<page>; sections: prefs.sections.<section>.
// tools/check_prefs.py (ctest prefs_sync) checks that these keys are the ones of core/api/Prefs.cpp, docs/APIDOC.md and tools/gen_openapi.py.
export type PrefType = 'bool' | 'int' | 'string' | 'choice' | 'secret'
export interface PrefMeta {
  key: string
  page: PrefPage
  section?: string
  type: PrefType
  optional?: boolean	// string that may be empty
  min?: number
  max?: number
  choices?: number[]	// type 'choice': the allowed values
  unit?: string
  restart?: boolean
  inactive?: boolean	// the feature is not in emuled yet: the value is only kept in the profile (compatible with eMule)
  locked?: boolean		// cannot be turned on in emuled (shown, not editable)
}

export const PREF_PAGES = ['general', 'display', 'connection', 'proxy', 'server', 'directories', 'files', 'notifications', 'statistics', 'messages', 'security', 'scheduler', 'web', 'advanced'] as const
export type PrefPage = (typeof PREF_PAGES)[number]
// whole pages whose feature is not in emuled yet
export const INACTIVE_PAGES: PrefPage[] = ['proxy', 'notifications', 'scheduler']

const KB = 1000000
export const PREFS: PrefMeta[] = [
  // General
  { key: 'nick', page: 'general', type: 'string' },
  { key: 'online_signature', page: 'general', type: 'bool', inactive: true },
  { key: 'check_new_version', page: 'general', type: 'bool', inactive: true },
  { key: 'check_new_version_days', page: 'general', type: 'int', min: 2, max: 7, inactive: true },

  // Display
  { key: 'remove_finished_downloads', page: 'display', type: 'bool' },
  { key: 'store_searches', page: 'display', type: 'bool', inactive: true },

  // Connection
  { key: 'download_capacity_kbps', page: 'connection', section: 'capacity', type: 'int', min: 1, max: KB, unit: 'kB/s', inactive: true },
  { key: 'upload_capacity_kbps', page: 'connection', section: 'capacity', type: 'int', min: 0, max: KB, unit: 'kB/s' },
  { key: 'max_download_kbps', page: 'connection', section: 'limits', type: 'int', min: 0, max: KB, unit: 'kB/s' },
  { key: 'max_upload_kbps', page: 'connection', section: 'limits', type: 'int', min: 0, max: KB, unit: 'kB/s' },
  { key: 'tcp_port', page: 'connection', section: 'ports', type: 'int', min: 1, max: 65535, restart: true },
  { key: 'udp_port', page: 'connection', section: 'ports', type: 'int', min: 0, max: 65535, restart: true },
  { key: 'upnp_enabled', page: 'connection', section: 'ports', type: 'bool' },
  { key: 'max_sources_per_file', page: 'connection', section: 'limits', type: 'int', min: 1, max: 100000 },
  { key: 'max_connections', page: 'connection', section: 'limits', type: 'int', min: 1, max: 100000 },
  { key: 'network_ed2k', page: 'connection', section: 'networks', type: 'bool' },
  { key: 'network_kademlia', page: 'connection', section: 'networks', type: 'bool', restart: true },
  { key: 'auto_connect', page: 'connection', section: 'networks', type: 'bool', restart: true },
  { key: 'reconnect', page: 'connection', section: 'networks', type: 'bool' },
  { key: 'show_overhead', page: 'connection', section: 'networks', type: 'bool' },

  // Proxy (the proxy layer is not in emuled yet: it cannot be turned on)
  { key: 'proxy_enabled', page: 'proxy', type: 'bool', locked: true },
  { key: 'proxy_type', page: 'proxy', type: 'choice', choices: [0, 1, 2, 3, 4, 5] },
  { key: 'proxy_host', page: 'proxy', type: 'string', optional: true },
  { key: 'proxy_port', page: 'proxy', type: 'int', min: 1, max: 65535 },
  { key: 'proxy_auth', page: 'proxy', type: 'bool' },
  { key: 'proxy_user', page: 'proxy', type: 'string', optional: true },
  { key: 'proxy_password', page: 'proxy', type: 'secret' },

  // Server
  { key: 'dead_server_retries', page: 'server', type: 'int', min: 1, max: 100 },
  { key: 'auto_update_server_list', page: 'server', type: 'bool' },
  { key: 'add_servers_from_server', page: 'server', type: 'bool' },
  { key: 'add_servers_from_clients', page: 'server', type: 'bool' },
  { key: 'use_server_priorities', page: 'server', type: 'bool' },
  { key: 'smart_lowid_check', page: 'server', type: 'bool' },
  { key: 'server_source_reask_minutes', page: 'server', type: 'int', min: 1, max: 60, unit: 'min' },
  { key: 'safe_server_connect', page: 'server', type: 'bool' },
  { key: 'static_servers_only', page: 'server', type: 'bool' },
  { key: 'manual_servers_high_priority', page: 'server', type: 'bool', inactive: true },

  // Files
  { key: 'add_new_downloads_paused', page: 'files', type: 'bool' },
  { key: 'advanced_time_remaining', page: 'files', type: 'bool' },
  { key: 'preview_chunks_first', page: 'files', type: 'bool' },
  { key: 'auto_down_priority', page: 'files', type: 'bool' },
  { key: 'auto_up_priority', page: 'files', type: 'bool' },
  { key: 'transfer_full_chunks', page: 'files', type: 'bool' },
  { key: 'start_next_paused', page: 'files', type: 'choice', choices: [0, 1, 2, 3] },
  { key: 'filename_cleanup', page: 'files', type: 'bool' },
  { key: 'filename_cleanup_filter', page: 'files', type: 'string', optional: true },
  { key: 'remember_downloaded_files', page: 'files', type: 'bool' },
  { key: 'remember_cancelled_files', page: 'files', type: 'bool' },

  // Notifications (nothing is sent by emuled yet)
  { key: 'notify_download_finished', page: 'notifications', section: 'events', type: 'bool', inactive: true },
  { key: 'notify_new_download', page: 'notifications', section: 'events', type: 'bool', inactive: true },
  { key: 'notify_chat', page: 'notifications', section: 'events', type: 'bool', inactive: true },
  { key: 'notify_log', page: 'notifications', section: 'events', type: 'bool', inactive: true },
  { key: 'notify_important_error', page: 'notifications', section: 'events', type: 'bool', inactive: true },
  { key: 'notify_new_version', page: 'notifications', section: 'events', type: 'bool', inactive: true },
  { key: 'email_enabled', page: 'notifications', section: 'email', type: 'bool', inactive: true },
  { key: 'email_to', page: 'notifications', section: 'email', type: 'string', optional: true, inactive: true },
  { key: 'email_from', page: 'notifications', section: 'email', type: 'string', optional: true, inactive: true },
  { key: 'smtp_server', page: 'notifications', section: 'email', type: 'string', optional: true, inactive: true },
  { key: 'smtp_port', page: 'notifications', section: 'email', type: 'int', min: 0, max: 65535, inactive: true },
  { key: 'smtp_security', page: 'notifications', section: 'email', type: 'choice', choices: [0, 1, 2], inactive: true },
  { key: 'smtp_auth', page: 'notifications', section: 'email', type: 'choice', choices: [0, 1, 2], inactive: true },
  { key: 'smtp_user', page: 'notifications', section: 'email', type: 'string', optional: true, inactive: true },
  { key: 'smtp_password', page: 'notifications', section: 'email', type: 'secret', inactive: true },

  // Statistics
  { key: 'stats_graph_interval_s', page: 'statistics', type: 'int', min: 0, max: 200, unit: 's' },
  { key: 'stats_tree_interval_s', page: 'statistics', type: 'int', min: 0, max: 100, unit: 's' },
  { key: 'stats_average_minutes', page: 'statistics', type: 'int', min: 1, max: 100, unit: 'min' },

  // Messages and comments
  { key: 'messages_friends_only', page: 'messages', type: 'bool' },
  { key: 'advanced_spam_filter', page: 'messages', type: 'bool' },
  { key: 'chat_captchas', page: 'messages', type: 'bool' },
  { key: 'message_filter', page: 'messages', type: 'string', optional: true },
  { key: 'comment_filter', page: 'messages', type: 'string', optional: true },

  // Security
  { key: 'ip_filter_level', page: 'security', section: 'ipfilter', type: 'int', min: 0, max: 255, inactive: true },
  { key: 'filter_servers_by_ip', page: 'security', section: 'ipfilter', type: 'bool', inactive: true },
  { key: 'secure_ident', page: 'security', section: 'identity', type: 'bool', restart: true },
  { key: 'crypt_supported', page: 'security', section: 'obfuscation', type: 'bool', restart: true },
  { key: 'crypt_requested', page: 'security', section: 'obfuscation', type: 'bool', restart: true },
  { key: 'crypt_required', page: 'security', section: 'obfuscation', type: 'bool', restart: true },
  { key: 'search_spam_filter', page: 'security', section: 'identity', type: 'bool', inactive: true },
  { key: 'see_shares', page: 'security', section: 'identity', type: 'choice', choices: [0, 1, 2] },

  // Scheduler (not in emuled yet)
  { key: 'scheduler_enabled', page: 'scheduler', type: 'bool' },

  // Extended (PPgTweaks)
  { key: 'max_connections_per_5s', page: 'advanced', section: 'tcp', type: 'int', min: 1, max: 1000 },
  { key: 'max_half_connections', page: 'advanced', section: 'tcp', type: 'int', min: 1, max: 1000 },
  { key: 'fast_connection_reject', page: 'advanced', section: 'tcp', type: 'bool' },
  { key: 'server_keepalive_minutes', page: 'advanced', section: 'tcp', type: 'int', min: 0, max: 1440, unit: 'min' },
  { key: 'credit_system', page: 'advanced', section: 'misc', type: 'bool' },
  { key: 'filter_lan_ips', page: 'advanced', section: 'misc', type: 'bool' },
  { key: 'a4af_save_cpu', page: 'advanced', section: 'misc', type: 'bool' },
  { key: 'own_hostname', page: 'advanced', section: 'misc', type: 'string', optional: true },
  { key: 'sparse_part_files', page: 'advanced', section: 'files', type: 'bool' },
  { key: 'allocate_full_file', page: 'advanced', section: 'files', type: 'bool' },
  { key: 'check_disk_space', page: 'advanced', section: 'files', type: 'bool' },
  { key: 'min_free_disk_space_mb', page: 'advanced', section: 'files', type: 'int', min: 0, max: 4095, unit: 'MB' },
  { key: 'safe_file_writing', page: 'advanced', section: 'files', type: 'choice', choices: [0, 1, 2] },
  { key: 'extract_metadata', page: 'advanced', section: 'files', type: 'choice', choices: [0, 1], inactive: true },
  { key: 'file_buffer_kib', page: 'advanced', section: 'files', type: 'int', min: 16, max: 1536, unit: 'KiB' },
  { key: 'upload_queue_size', page: 'advanced', section: 'files', type: 'int', min: 2000, max: 10000 },
  { key: 'save_log_to_disk', page: 'advanced', section: 'log', type: 'bool' },
  { key: 'verbose_log', page: 'advanced', section: 'log', type: 'bool' },
  { key: 'log_level', page: 'advanced', section: 'log', type: 'int', min: 1, max: 5 },
  { key: 'save_debug_to_disk', page: 'advanced', section: 'log', type: 'bool' },
  { key: 'log_source_exchange', page: 'advanced', section: 'log', type: 'bool' },
  { key: 'log_banned_clients', page: 'advanced', section: 'log', type: 'bool' },
  { key: 'log_ratings', page: 'advanced', section: 'log', type: 'bool' },
  { key: 'log_secure_ident', page: 'advanced', section: 'log', type: 'bool' },
  { key: 'log_filtered_ips', page: 'advanced', section: 'log', type: 'bool' },
  { key: 'log_file_saving', page: 'advanced', section: 'log', type: 'bool' },
  { key: 'log_a4af', page: 'advanced', section: 'log', type: 'bool' },
  { key: 'log_updown_events', page: 'advanced', section: 'log', type: 'bool' },
  { key: 'uss_enabled', page: 'advanced', section: 'uss', type: 'bool', inactive: true },
  { key: 'uss_min_upload_kbps', page: 'advanced', section: 'uss', type: 'int', min: 1, max: KB, unit: 'kB/s', inactive: true },
  { key: 'uss_ping_tolerance_percent', page: 'advanced', section: 'uss', type: 'int', min: 100, max: 10000, unit: '%', inactive: true },
  { key: 'uss_ping_tolerance_ms', page: 'advanced', section: 'uss', type: 'int', min: 1, max: 10000, unit: 'ms', inactive: true },
  { key: 'uss_use_ms_tolerance', page: 'advanced', section: 'uss', type: 'bool', inactive: true },
  { key: 'uss_going_up_divider', page: 'advanced', section: 'uss', type: 'int', min: 1, max: 1000, inactive: true },
  { key: 'uss_going_down_divider', page: 'advanced', section: 'uss', type: 'int', min: 1, max: 1000, inactive: true },
  { key: 'uss_pings', page: 'advanced', section: 'uss', type: 'int', min: 1, max: 100, inactive: true },
  { key: 'upnp_close_on_exit', page: 'advanced', section: 'upnp', type: 'bool' },
]
