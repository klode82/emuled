# APIDOC — the manual of the `emuled` API calls

This manual explains, in simple terms, every call of the daemon's API. The formal, machine-readable description (OpenAPI 3.0.3) is in [`openapi.json`](openapi.json) and the daemon also serves it at `/api/v1/openapi.json`. The two are kept aligned by a test (`ctest`, test `daemon_api`): every path declared in the description must really exist in the daemon.

The eMule acronyms and terms are explained in the [MANUAL](MANUAL.md); only the ones needed to understand a field are repeated here.

## 1. How to talk to the daemon

**Start.** The daemon is `emuled --run` (see MANUAL, B5). The API lives inside the same process and starts by itself:

```
emuled --run --profile /percorso/eMule/config --api-port 8765
```

- **Where it listens.** Only on `127.0.0.1` (the machine itself), port `8765` (change it with `--api-port`). The address is changed with `--api-bind`, but opening it to other computers is your own choice: the API encrypts nothing (if you need access from outside, put a proxy with TLS in front of it).
- **Without API:** `--no-api`.
- **Web interface.** With `--web-dir FOLDER` the daemon serves the same files as the web UI on the same port (`http://127.0.0.1:8765/`), so the web UI uses the API without other settings.

**Who gets in.** The web interface uses an **account** (user name and password, see section 2 "Account"): `POST /api/v1/auth/login` answers with a session token. Scripts and tools can use instead the **file token** described below. The two are equally valid as Bearer.

**The token.** Every call (except `ping`, `openapi.json` and the three under `auth/`) must have the header `Authorization: Bearer TOKEN`. The token is in `emuled.token` in the profile's configuration folder (the daemon creates it at first start, 64 hexadecimal characters, readable only by the file's owner). You can choose it with `--api-token`. The browsers' WebSocket, which cannot set headers, also accepts `?token=TOKEN` in the address.

```
TOKEN=$(cat /percorso/eMule/config/emuled.token)
curl -H "Authorization: Bearer $TOKEN" http://127.0.0.1:8765/api/v1/status
```

**Answers.** They are always JSON, in UTF-8. An error has the form `{"error": "explanation in English", "code": "stable_code"}`: `error` is for people reading, **`code` does not change** with the language or with the wording and is meant for programs (the interface translates it). The HTTP status is one of these:

| Code | When |
|---|---|
| 400 | body is not a JSON object; invalid hash |
| 401 | token missing or wrong |
| 404 | path does not exist; download or server not found |
| 405 | method not allowed for that path (for example `DELETE` on `/status`) |
| 409 | the file is already downloading |
| 413 | body over 1 MB |
| 422 | data valid as JSON but wrong (link, hash, size, address) |

**`code` values.** Generic, from the status: `bad_request`, `unauthorized`, `forbidden`, `not_found`, `method_not_allowed`, `conflict`, `body_too_large`, `invalid`, `internal`. Specific: `bad_json`, `bad_hash`, `bad_link`, `bad_address`, `bad_param`, `bad_priority`, `bad_category`, `bad_scope`, `bad_search`, `bad_directories`, `unknown_pref`, `unknown_field`, `url_not_http`, `route_not_found`, `download_not_found`, `server_not_found`, `category_not_found`, `search_not_found`, `file_not_found`, `not_shared`, `already_downloading`, `server_exists`, `server_refused`, `category_zero`, `job_running`, `search_not_started`, `not_added`, `ed2k_not_connected`, `kad_not_connected`, `kad_off`, `kad_unavailable`, `kad_already_connected`, `no_network`, `no_web_ui`, `forbidden_path`, `headers_too_long`, `loop_unavailable`.

**Safety limits.** Headers up to 16 KB, body up to 1 MB, 64 connections at once, idle connections closed after 60 seconds. The static file paths cannot leave the web UI folder (`..` and `%2e%2e` are rejected).

### The daemon's other addresses (outside `/api/`)

The daemon also serves the interface pages, **without a token**: `/t/<id>/…` is the folder of the template `<id>` (with its entry page also for paths without an extension, as in single-page applications; `/t/<id>` without the trailing slash redirects to `/t/<id>/`; paths that leave the folder, even through symbolic links, give **403**; a template that does not exist or is not valid gives a help page with **404**; a file over **32 MB** gives **413**). The template pages have the headers `Content-Security-Policy` (requests, scripts, styles, fonts and images only towards the same address), `Referrer-Policy: no-referrer` and `X-Frame-Options: DENY`. `/` is a small page that reads the saved choice from the browser (`localStorage`, key `emuled.template`; with no choice, `console`) and leads to `/t/<id>/`, keeping the part after `#`. `/?template=<id>` sets the choice (also to share it) and `/?template=console` is the **emergency address** that brings you back to the default template even if the chosen one is broken. If the daemon is started with `web_dir`, `/` serves that folder as before (a single interface). Templates are searched in `/usr/share/emuled/templates/`, `~/.emuled/templates/` and in the extra folder (`--templates-dir`).

## 2. Status

### `GET /api/v1/ping` — is the daemon on? *(no token)*

```
{"pong": true}
```

### `GET /api/v1/openapi.json` — the formal description *(no token)*

### Account — `GET /api/v1/auth/status`, `POST /api/v1/auth/setup`, `POST /api/v1/auth/login` *(no token)*

The web interface is used with a user name and password. The account is in the file `emuled.users` in the profile folder (permissions `0600`): name, a random **salt**, the PBKDF2-HMAC-SHA256 hash of the password (200000 rounds) and a secret that signs the sessions. The password cannot be recovered, only reset from the terminal with `emuled --reset-user` (this also changes the secret: sessions already open expire; it also works while the daemon is running).

- `GET /api/v1/auth/status` → `{"configured": true|false, "setup_required": true|false}`. It tells the interface whether to show the login or the first start.
- `POST /api/v1/auth/setup` `{"setup_code": "...", "username": "mario", "password": "at-least-8-characters"}` creates the **first** account and answers `201` with `{"token": "s1.…", "username": "mario", "expires_in": 2592000}`. The **setup code** is the token of the file `emuled.token`: the daemon prints it at start as long as there is no account, so whoever reaches the port without having it cannot take over the interface. Errors: `403 bad_setup_code`, `400 bad_username`/`bad_password`, `409 account_exists` (an account already exists).
- `POST /api/v1/auth/login` `{"username": "...", "password": "..."}` → `200` with the same token (valid 30 days). Errors: `401 bad_credentials` (the daemon does not say whether the name or the password is wrong), `429 too_many_attempts` with `Retry-After: 60`: five wrong attempts from the same address block that address for one minute.

The session token is used like the file token (`Authorization: Bearer s1.…`, or `?token=` for the WebSocket). In the browser it is stored under the same key `localStorage['emuled.token']`.

### First start — `GET /api/v1/setup`, `POST /api/v1/setup/done`, `GET /api/v1/ports/check`

They serve the first-start **web Wizard** (name on the network, ports, obfuscation, limits, folders), which the interface shows full screen after the first account is created.
- `GET /api/v1/setup` → `{"pending": true|false}`. `pending` becomes `true` when the first account is created (`auth/setup`) and stays so until it is confirmed. A profile that never had the Wizard (from before this feature) answers `false`.
- `POST /api/v1/setup/done` → `{"pending": false}`: the interface calls it when the user finishes the Wizard.
- `GET /api/v1/ports/check?tcp=P&udp=Q` → `{"tcp": {"port": P, "valid": true, "free": true, "own": false}, "udp": {...}}`. It really tries to listen on (TCP) or open (UDP) the port, then closes it. `own: true` = it is the port emuled is already using, so it is not a conflict (`free` is `true`). You can pass only `tcp` or only `udp`. It changes nothing: the Wizard uses it to **warn**, without blocking. The real ports are set with `PATCH /api/v1/preferences` (`tcp_port`, `udp_port`, they apply at restart).

### Restart and port test — `POST /api/v1/system/restart`, `GET /api/v1/ports/test-url`

- `POST /api/v1/system/restart` → `200 {"restarting": true}`. The daemon shuts down cleanly (saves everything) and **starts again by itself**, same process and same arguments (`execv`: it works with systemd, Docker or from a terminal). It answers at once and restarts in less than a second; to know it is back, just call `GET /status` again (`uptime_s` starts from zero). It is needed for new ports (`restart_required` of `PATCH /preferences`). `501` if restart is not available.
- `GET /api/v1/ports/test-url?lang=1040` → `{"url": "https://porttest.emule-project.net/connectiontest.php?tcpport=P&udpport=Q&lang=1040&obf=0", "tcp": P, "udp": Q, "obfuscation": false}`. It is the address eMule uses (`TriggerPortTest`) with the ports **in use right now**: if you have just changed them, restart first. With obfuscation on, the address has `obf=1&clienthash=<user hash>`. `lang` is the Windows language identifier (1040 Italian, 1033 English, the default). **The daemon does not open the address**: the browser opens it, on a user click; the site tries to connect to the TCP and UDP ports and the daemon answers (`OP_PORTTEST`).

### Importing an existing eMule — `GET /api/v1/setup/import`, `POST /api/v1/setup/import/scan`, `POST /api/v1/setup/import`

For those who already have eMule (usually on Windows): the web Wizard can bring over identity, credits, known files, servers, Kad nodes and preferences. **Only during the first start** (Wizard not finished) and only once: after that, `409`.
- `GET /api/v1/setup/import` → `{"available": true, "imported": false, "candidates": ["/home/utente/.emuled.old/profile"]}`. `candidates` (only if `available`): the computer folders that look like an eMule configuration — old `~/.emuled*` installations, `~/.config/emule`, `~/eMule/config`, eMule under Wine, `/mnt` and `/media` —, excluding the profile in use.
- `POST /api/v1/setup/import/scan` `{"path": "/mnt/z/eMule/config", "rules": [...]}` → `{"files": [{"name", "size"}], "total_bytes", "nick", "dirs": [{"windows": "Z:\\eMule\\Incoming\\", "path": "/mnt/z/eMule/Incoming/", "mapped": true, "exists": true}], "rules": [{"win": "Z:/eMule", "linux": "/mnt/z/eMule"}]}`. **Read only.** Without `rules` the daemon proposes them by looking, above the given folder, for the place where the Windows folders really exist. Errors: `422` (it does not look like an eMule folder — `preferences.ini`, `known.met` and `clients.met` are missing —, or it is emuled's own folder, or a rule is not valid), `409` (it is not the first start).
- `POST /api/v1/setup/import` `{"path": "...", "rules": [{"win": "Z:\\eMule", "linux": "/mnt/z/eMule"}]}` → `200 {"staged": true, "restarting": true}`. It copies the files to `<profile>/import-ready/` fixing the paths in `preferences.ini`, `shareddir.dat` (stays UTF-16) and `Category.ini`; then **the daemon restarts by itself** (same process, same arguments) and at start, before reading the profile, puts the files in their place. The replaced files go to `<profile>/pre-import-YYYYMMDD-hhmmss/`. Downloads in progress (`Temp/`) and the Kad indexes are not copied. An import interrupted before the end is discarded at start. Errors: `422` as above or not enough space.

### Computer folders — `GET /api/v1/fs`, `POST /api/v1/fs/mkdir`

They serve the "Choose a folder" window of the Wizard and of Preferences (Incoming, Temp, shared folders). **These are the folders of the computer where the daemon runs**, not those of the browser's PC: with Docker they are those of the container and of the mounted volumes. Like every other call they require the token; the daemon sees only what the user it runs as can see. Never the content of files.
- `GET /api/v1/fs?path=/percorso&hidden=0|1` → `{"path": "/percorso", "parent": "/" | null, "home": "/home/utente", "writable": true, "free_bytes": 123, "truncated": false, "entries": [{"name": "Download", "path": "/percorso/Download", "readable": true, "writable": true}]}`. Without `path` it starts from the user's folder. The path is normalized (`..` and symbolic links are resolved). Only subfolders, in alphabetical order ignoring case; hidden ones (starting with `.`) only with `hidden=1`; at most 2000 (`truncated`). Errors: `400 bad_path` (not absolute), `400 not_a_directory`, `404 dir_not_found`, `403 forbidden_path`.
- `POST /api/v1/fs/mkdir` `{"path": "/percorso/nuova"}` creates **one** folder (the one containing it must exist) and answers `201 {"path": ...}`. Errors: `409 dir_exists`, `404 dir_not_found`, `400 bad_path`/`bad_dirname`, `403 forbidden_path`.

### `GET /api/v1/status` — the general status

`version` is the eMule protocol/client version (the one announced on the network); `emuled_version` is the emuled version; `base_version` is the eMule code it derives from.

```json
{
  "version": "0.72a", "emuled_version": "0.1.0", "base_version": "eMule 0.72a-community", "nick": "eMuleLinux", "uptime_s": 3600,
  "ports": {"tcp": 55662, "udp": 55672},
  "ed2k": {"connected": true, "connecting": false, "id": 1757362257, "high_id": true,
           "server": {"name": "eMule Sunrise", "address": "176.123.5.89", "port": 4725, "users": 48000, "files": 9000000}},
  "kad":  {"running": true, "connected": true, "tcp_firewalled": false, "udp_firewalled": false,
           "udp_verified": true, "contacts": 169, "users": 2400000},
  "transfers": {"downloads": 2, "download_speed": 524288, "upload_speed": 12000, "uploading": 1, "waiting": 3}
}
```

- **`ed2k`** — the connection to the server. **`high_id`**: the server was able to connect to our TCP port, so other clients can reach us; if it is `false` (low ID, *LowID*) we can only call out ourselves (and we go through the server for "callbacks"). `id` is the number the server gave us.
- **`kad`** — the Kademlia network (*Kad*), without servers. `tcp_firewalled` and `udp_firewalled` say whether our ports are closed from the outside (*firewall*); `udp_verified` that the check was confirmed by real nodes. `contacts` are the nodes in the routing table (*routing*).
- **`transfers`** — speeds in **bytes per second**; `uploading` the clients we are giving data to, `waiting` those queued with us.

## 3. Downloads

A download is a file being downloaded. Its identity is the **hash** (MD4 as 32 hexadecimal characters, uppercase).

**The `Download` object**

| Field | Meaning |
|---|---|
| `hash` | the file's MD4 |
| `name`, `size` | name and size in bytes |
| `completed`, `percent` | **valid** bytes downloaded and percentage |
| `status` | `ready` (in progress), `empty` (no data), `waiting_for_hash`, `hashing` (the file is being checked), `error`, `insufficient_disk_space`, `paused`, `completing` (being moved to Incoming), `complete`, `unknown` |
| `stopped` | stopped (it lost its sources) |
| `sources`, `sources_transferring` | known sources and those currently transferring |
| `speed` | bytes per second |
| `priority` | `auto`, `very_low`, `low`, `normal`, `high`, `release` |
| `category` | category number (0 = all) |
| `part_file` | name of the `.part.met` in `Temp/` |

### `GET /api/v1/downloads` — the list

```
curl -H "Authorization: Bearer $TOKEN" http://127.0.0.1:8765/api/v1/downloads
```

### `POST /api/v1/downloads` — add a download

With an ed2k link:

```
curl -X POST -H "Authorization: Bearer $TOKEN" -d '{"link":"ed2k://|file|Nome.pdf|7743600|B2C072C57F57A9A6AEECB50390DC27F8|/"}' http://127.0.0.1:8765/api/v1/downloads
```

Or with `hash`, `name` and `size`:

```
-d '{"hash":"B2C072C57F57A9A6AEECB50390DC27F8","name":"Nome.pdf","size":7743600}'
```

Answers: **201** with the download; **409** if it already exists; **422** if link, hash or size are not valid; **400** if the body is not JSON.

### `GET /api/v1/downloads/{hash}` — one download

### `POST /api/v1/downloads/{hash}/pause` · `/resume` · `/stop`

- **pause** — pauses and keeps the sources;
- **resume** — resumes (even if it was paused for insufficient space);
- **stop** — stops and discards the sources.

### The completed ones

When a download finishes it leaves the queue, but — as in eMule's window — **it stays in the list as "Complete"** (`status: "complete"`, with `completed_at` and `path`) until you remove it. It is removed with `DELETE /downloads/{hash}` (it only leaves the list: **the file stays on disk**) or all together with `POST /downloads/clear-completed` (answer `{"removed": N}`). With the preference `remove_finished_downloads` on, completed downloads never enter the list. The list of completed downloads is in memory only (like eMule's window): it is emptied when the daemon restarts. On a completed one, `pause`, `resume`, `stop`, `sources` and `parts` answer **409**.

### `GET /api/v1/downloads/{hash}/sources` — the sources

Every download also has `best_queue_rank`: the closest queue position among all its sources (0 = none in queue), and `file_op` / `file_op_percent`: the operation in progress on the file (`none`, `hashing` = hash check, `copying`, `uncompressing`, `import_parts`) with its progress 0-100. The interface shows it as a green bar above the completion one.

Besides the file's own sources, the list includes those **"for another file"** (A4AF, *Ask For Another File*): users who also have this file but whom eMule is already using for another download of the same user (typical with the episodes of a series). They have the field `other_file` with the download they are currently used for, and they do not count in the download's `sources` (which has the field `sources_other_files`).

For each source: `name`, `software` (e.g. "eMule v0.50a"), `ip`, `port`, `low_id` (low ID: it can only be reached with a callback), `state` (`downloading`, `on_queue`, `connected`, `connecting`, `waiting_callback`, `waiting_callback_kad`, `requesting_hashset`, `no_needed_parts` = it has no parts we need, `too_many_connections`, `low_to_low_id`, `banned`, `error`, `remote_queue_full`, `none`), `from` (`server`, `kad`, `source_exchange`, `passive`, `link`), `queue_rank` (position in its queue), `speed`, `session_downloaded`, `available_parts` and `part_count`.

### `POST /api/v1/downloads/{hash}/find-sources` — look for sources now

Usually eMule asks the server for new sources every **15 minutes** and Kad with waits of up to **1 hour** per attempt: a download with 0 sources can stay stuck for a long time. This call resets those waits: on the next round the daemon asks right away. It answers **202** with `server` and `kad` (`false` if you are not connected to that network: the request for it does not start), `paused` (a stopped download searches when it starts again) and `wait_s`: **the server accepts one group of source requests every 5 minutes** (protection of the original, 15 files × 20 s), so the queued request starts in `wait_s` seconds, not immediately. **404/409** like the other calls on downloads.

### `POST /api/v1/downloads/{hash}/stream` and `GET /api/v1/stream/{id}/{nome}` — streaming

To watch or listen to a file **while it downloads** (or afterwards) with a local player (VLC, mpv…). `POST …/stream` creates a **temporary link** and answers **201** with `path` (to be appended to the daemon's address, e.g. `http://127.0.0.1:8765` + `path`), `name`, `size`, `mime` and `expires_in` (seconds: 6 hours). The link is valid for **one file only** and **does not contain the token**: the authorization is the `id` in the path (128 random bits), so it can be given to a player without exposing the rest of the API. For a download in progress it also turns on the *preview priority*: eMule downloads the file's **first and last part** first, which many formats need in order to start.

`GET /api/v1/stream/{id}/{nome}` serves the file with range requests (`Range`, answer **206**; `Accept-Ranges: bytes`). `{nome}` is just the file name, for the players: it does not matter. For a **download in progress** only what is already on disk is sent: a range that begins in a part not yet downloaded gives **503** with `Retry-After: 3` (the player retries), and one that goes beyond the bytes present is shortened. **403** = link expired or unknown, **416** = invalid range. It also works for completed ones.

### `GET /api/v1/downloads/{hash}/names` — the names used by the sources

It is eMule's "Name" tab: `current` (the current name) and `names`, the list of names the sources call the file by, each with `count` (how many sources use it), the most common first. The list is empty until some source has answered. **404** if the download does not exist or is already completed. To change the name: `PATCH /api/v1/downloads/{hash}` with `{"name": "new name.mkv"}` (not empty, without `/`, at most 255 bytes; it applies at once and also when the download is complete; **409** if the file is being checked or completed; **422** if the name is not valid).

### `GET /api/v1/downloads/{hash}/comments` — comments and ratings

It is eMule's "All comments" tab. `comments` is a list of `{network, rating, comment, file_name, user}`: `network` is `ed2k` (it came from a source) or `kad` (a note found on Kad), `rating` goes from 1 (fake) to 5 (excellent), 0 = no rating. There are also `kad_connected`, `kad_searching` (a note search is in progress) and `filter` (the anti-spam filter in use, see the preference `comment_filter`).

### `POST /api/v1/downloads/{hash}/comments/search-kad` — search for comments on Kad

The "Search on Kad" button: it asks Kad for other users' notes about this file. It answers **202** with `{"started": true}`; the results arrive little by little and are read with `GET …/comments`. **422** if Kad is not connected, **409** if a note search for this file is already in progress.

### `GET /api/v1/downloads/{hash}/blocks` — the blocks being downloaded now

eMule downloads a *block* (chunk, `part_size` = 9 728 000 bytes, the one of the map) in **pieces** of `block_size` = 184 320 bytes (180 KB) that it requests from the sources. This call says which blocks are arriving at this moment: `active` has one element for each block with `part` (number from 0), `size`, `missing` (bytes still missing) and `sources`: the sources that are transferring it, each with `name`, `software`, `ip`, `port`, `speed` (bytes/s) and `blocks`, the requested pieces with `start` and `end` (positions in the file, both ends included) and `transferred` (bytes already arrived of that piece). Only the request already sent to the source counts, not those waiting to be sent. `receiving` says whether the block is **really arriving** (a source has speed or a piece already has bytes) or is only **requested** and waiting for the answer. Empty list if no source is transferring. **404** if the download does not exist or is completed.

### `GET /api/v1/downloads/{hash}/details` — the details

It is eMule's "General" tab in JSON, for a download in progress (**404** if it does not exist or is already completed): `name`, `part_met` and `part_met_path`, `hash`, `aich_hash` (or `null`) with `aich_status` (`error`, `empty`, `untrusted`, `trusted`, `verified`, `complete`), `hashset` (`ed2k` and `aich`: are the hashes of all parts available?), `size` and `size_on_disk`, `type` (extension), `parts` (`total`, `available`, `available_pct`), `status`, `sources` (`total`, `valid`, `transferring`, `no_needed_parts`, `other_file`), `speed`, `transferred`, `compression_gain`, `completed`, `corrupted`, `remaining` (each with `bytes` and `pct`, truncated to the tenth like eMule), `recovered_parts` (the parts recovered by I.C.H.), and the dates in Unix seconds: `added`, `active_seconds`, `last_seen_complete`, `last_received`.

### `POST /api/v1/downloads/{hash}/content` and `GET …/content` — the file's content

It is eMule's "Content" tab, done by the external program **`mediainfo`** (searched in the `PATH`; it can be specified with the `mediainfo` key of `config.yaml` or with `EMULED_MEDIAINFO`). `POST` starts the reading on the `.part` of a download in progress (it is enough that the container header is there: for an mkv, the first bytes) or on the file of a completed one and answers **202**; `GET` gives the state: `idle`, `running`, `done`, `failed` (`reason`: `timeout`, `bad_output`, `spawn`) or `unavailable` (`not_installed`: mediainfo is not there, and in that case `POST` too answers 200 with this state). When the reading is finished (`done`): `recognized` (`false` if mediainfo does not understand the file), `general` (`format`, `size`, `duration_s`, `overall_bitrate`, `title`, `movie`, `date`), `video[]` (`codec`, `profile`, `codec_id`, `bitrate`, `width`, `height`, `aspect`, `fps`, `lang`, `title`), `audio[]` (`codec`, `commercial`, `bitrate`, `channels`, `sampling`, `lang`, `title`, `default`) and `text[]` (subtitles: `codec`, `lang`, `title`, `default`, `forced`). The program is launched **without a shell** (the path is an argument) and with a limit of 10 seconds.

### `GET /api/v1/downloads/{hash}/parts` — the blocks

eMule divides the file into blocks of **9 728 000 bytes** (`part_size`). For each block: `index`, `size` (the last one is shorter), `missing` (bytes still to download), `sources` (in how many sources it is present) and `state`: `empty`, `partial`, `complete` or `corrupted` (to be downloaded again), and `verified`: `false` for a complete block that **cannot be checked** because the file has no list of the blocks' hashes (hashset; with a single block the file's hash applies). In addition, at the top level, `hashset` (is the list of hashes there?). With these the block map is drawn: the legend also distinguishes the **rare** blocks (1-2 sources: `sources` ≤ 2) and those **without sources** (`sources` = 0). Whoever does not know `verified` or `hashset` (older daemon versions) treats them as "verified".

### `DELETE /api/v1/downloads/{hash}` — cancel

It removes the download from the queue and **deletes the partial file** (`.part` and `.part.met`). It cannot be undone. It answers with the download's last state.

## 4. ed2k servers

### `GET /api/v1/servers` — the list (the `server.met`)

Fields: `name`, `description`, `address`, `ip`, `port`, `users`, `files` (how many it indexes), `max_users`, `ping_ms`, `failed_count` (failed attempts), `static` ("fixed" server, not removed if it dies).

### `POST /api/v1/servers` — add a server

Body: `{"address": "176.123.5.89:4725", "name": "eMule Sunrise", "priority": "normal", "static": false}` (only `address` is required; the address can also be a DNS name). **201** with the server; **409** if it already exists; **422** if `address` is not `address:port`, or if the address is not allowed (local network, `0.0.0.0`, filtered): eMule itself rejects it. The list is rewritten at once into `server.met`.

### `PATCH /api/v1/servers/{indirizzo:porta}` — change

Body with one or more fields: `priority` (`low`, `normal`, `high`: with "use server priorities" the high servers are tried first), `static` (**fixed** server: it is not removed by itself when it stops answering), `name`. **404** if it is not in the list.

### `DELETE /api/v1/servers/{indirizzo:porta}` — remove

If it is the server you are connected to, it disconnects you first. It answers with the removed server.

### `DELETE /api/v1/servers` — empty the whole list

Like "Remove all" in eMule's server list: every server is removed and `server.met` is rewritten empty. A connection attempt in progress is stopped; the connection to the current server stays until it closes. It answers `{"removed": N}`. To get a list again use `POST /api/v1/servers/update` (from an address) or add servers one by one.

### `POST /api/v1/servers/connect` — connect

With `{"address": "176.123.5.89:4725"}` to that server; without a body to any one. It answers **202** at once: the result is read from `/status` (`ed2k.connected`). **404** if the server is not in the list.

### `POST /api/v1/servers/disconnect` — disconnect

## 5. Search

One search at a time: starting a new one cancels the previous one, and the results of the last one are kept.

### `POST /api/v1/searches` — start

Body: `{"query": "ubuntu iso", "scope": "server"}`. `query` has the syntax of eMule's search field (quotes, `OR`, `NOT`). `scope`:
- `server` (default): only the ed2k server you are connected to;
- `all`: global search, also all the other servers in the list, over UDP (one every 750 ms);
- `kad`: Kademlia network (uses the first word as the key);
- `global`: ed2k (all servers, like `all`) **and** Kad together, results merged by hash. It is enough that one of the two networks is connected; the other is skipped.

Optional filters: `ext` (extension without the dot), `type` (`Audio`, `Video`, `Image`, `Doc`, `Pro`, `Arc`, `Iso`), `min_size`, `max_size` (bytes), `min_sources`, `min_complete`.
It answers **202** with `id` and `state: "running"`. Only one search runs at a time: a new one stops the previous one. Every search is a **tab** that stays with its results, even after a daemon restart (file `searches.json` in the configuration folder), until it is removed with `DELETE`. **409** if you are not connected (to the server, to Kad for `kad`, to neither for `global`); **422** for an invalid expression or `scope`.

### `GET /api/v1/searches/{id}` — state and results

`state` is `running`, `finished` (all queried and no new answer for 6 s, or 120 s elapsed) or `cancelled`. `results` is the list of files, merged by `hash` (the same file from several servers counts once) and sorted by `sources`: `hash`, `name`, `type`, `size`, `sources` (the highest declared value), `complete_sources`, `answers` (from how many servers/nodes it came). To download a result: `POST /downloads` with `hash`, `name`, `size`. **404** if the search is unknown or was removed.

### `GET /api/v1/searches` — searches in memory

All the tabs, the most recent first, without `results`.

### `POST /api/v1/searches/{id}/refresh` — refresh the tab

It redoes the same search (same word, same `scope` and same filters, which the tab remembers even after a restart) **in the same tab**: the `id` does not change and the results are redone from scratch. Like `POST /searches`, it stops the search that was running. **404** if the tab does not exist; **409** if the chosen network is not connected (the tab and its results stay).

### `POST /api/v1/searches/{id}/cancel` — stop

It stops the search in progress; the tab and the results already arrived stay.

### `DELETE /api/v1/searches/{id}` — remove the tab

It stops the search if it is in progress and removes the tab for good: it is the only way a tab disappears.

## 6. WebSocket (live events)

Instead of polling the API continuously, you connect to `ws://127.0.0.1:8765/api/v1/events?token=TOKEN` (the browser cannot set the `Authorization` header in a WebSocket, that is why the token goes in the address; without a valid token the answer is 401). The daemon sends JSON text messages `{"type": "...", "data": {...}}`; messages coming from the client are ignored.

Once a second the daemon compares the state with the previous one and sends only what changed:

| `type` | when | `data` |
|---|---|---|
| `stats` | every second | like `transfers` of `/status` (speeds, number of downloads, uploads in progress and in queue) |
| `server.state` | the connection to the ed2k server changes | like `ed2k` of `/status` |
| `kad.state` | Kad's state changes (connected, firewall, contacts) | like `kad` of `/status` |
| `download.added` | a download enters the queue | the Download object |
| `download.updated` | something about a download changes (percentage, sources, state…) | the Download object |
| `download.removed` | a download leaves (cancelled or completed and removed) | `{hash, name}` |
| `search.results` | the search in progress changes (state or number of answers) | the Search object, without the results: to read them `GET /searches/{id}` |
| `log` | there are new lines in the log (in batches, at most 200) | `{"lines": [...], "last_id": N}`: lines as in `GET /log` |

Events come from a periodic comparison, so the maximum latency is 1 s.

## 7. Shared files and uploads

### `POST /api/v1/servers/update` — update the list from an address

Body `{"url": "https://esempio.org/server.met"}`. It downloads the file (only `http` and `https`, certificate verified, at most 8 MB and 60 s, up to 5 redirects) and **merges** it into the list: new servers are added, none is removed. It answers **202**; the outcome is read with `GET /api/v1/servers/update`: `state` is `running`, `done` (with `added`, the new servers, and `total`) or `failed` (with `error`). The successful address is remembered in `AC_ServerMetURLs.dat` (like eMule); `GET /api/v1/servers/urls` lists them. At the end it **rewrites `server.met` at once** (before, servers that came from a web address were saved only when the daemon shut down cleanly).

### `POST /api/v1/kad/bootstrap` — repopulate Kad

Body `{"url": "https://esempio.org/nodes.dat"}` or `{"address": "1.2.3.4:4672"}` (one of the two). With `url` it downloads a `nodes.dat` (only `http`/`https`, same rules as `/servers/update`), adds its contacts to Kad's table and, if Kad was off, starts it: it answers **202** and the outcome is read with `GET /api/v1/kad/bootstrap` (`state`: `running`, `done` with `added` and `contacts`, `failed` with `error`). With `address` it asks that node for the first contacts (Kad must not be connected yet). **409** if Kad is not available (UDP port off or daemon without network), if it is already connected (with `address`) or if a bootstrap is already in progress; **422** for invalid parameters.

### `GET /api/v1/shared` — the files you offer
For each file: `hash`, `name`, `size`, `path`, `part_file` (it is a download still in progress), `priority` (`auto`, `very_low`, `low`, `normal`, `high`, `release`), `requests` and `accepted` (requests and accepted requests since the daemon was turned on), `transferred` (data bytes), the three `all_time_*` values (since ever) and `queued` (clients queued for that file).

### `POST /api/v1/shared/reload` — recheck the folders
It rereads the shared folders and computes the hash of the new files (it can take a long time on big files). It answers **202** with `count` and `hashing` (files still to be computed).

### `POST /api/v1/shared/{hash}/priority` — priority
Body `{"priority": "high"}`. The higher it is, the more the file moves ahead in the queues of those who request it. **404** if the file is not shared, **422** if the priority does not exist.

### `GET /api/v1/uploads` — who downloads from you
`uploading` (who is receiving data now) and `waiting` (the queue), plus the total `speed`. For each client: `name`, `ip`, `port`, `software`, `user_hash`, `file_hash`, `file_name`, `speed`, `session_uploaded`, `score` (the score that decides the order in the queue).

## 8. Kad

### `GET /api/v1/kad` — state
`running`, `connected`, `tcp_firewalled`, `udp_firewalled`, `udp_verified` (the UDP firewall test has finished), `contacts`, `users` (estimate). The last fields are there only if Kad is on. It is the same object as `kad` in `/status`.

### `POST /api/v1/kad/connect` — start
Starts (or restarts) Kad with the profile's `nodes.dat`. When the daemon starts, Kad starts by itself if the profile wants it (`NetworkKademlia` in `preferences.ini`, on by default) and there is no `--no-kad`; otherwise it is started with this command. The connection takes a few tens of seconds, the state is read from `/kad`. **202** if it starts; **409** if the daemon has no Kad (`--no-kad` or `--offline`) or if it cannot start.

### `POST /api/v1/kad/recheck-firewall` — recheck the firewall
Redoes the reachability test (TCP and UDP), like eMule's command. It answers **202**; the result is read from `/kad` after a few tens of seconds. **409** if Kad is off.

### `POST /api/v1/kad/disconnect` — stop
Stops Kad and saves the contacts. It answers 200 even if it was already stopped.

## 9. Preferences

### `GET /api/v1/preferences` — read
A flat key/value object. The keys are eMule's preferences (same members and same `preferences.ini` keys), listed here by the page of eMule's Preferences dialog where they live; the web interface shows the same pages as tabs. Options marked **not active yet** are kept in the profile (compatible with eMule for Windows) but emuled does not use them yet. *From restart* = the change applies after the daemon restarts.

**Passwords are write-only**: `proxy_password` and `smtp_password` are never returned; `GET` shows `proxy_password_set` / `smtp_password_set` (true/false) instead, and `PATCH` accepts the new value (an empty text clears it). eMule stores them in plain text in `preferences.ini`, and so does emuled.

| Key | Type | Meaning |
|---|---|---|
| **General** | | |
| `nick` | text | the name you appear under to others |
| `online_signature` | yes/no | writes `onlinesig.dat` with your status (*not active yet*) |
| `check_new_version`, `check_new_version_days` | yes/no, integer 2–7 | check for a new version every N days (*not active yet*) |
| **Display** | | |
| `remove_finished_downloads` | yes/no | remove finished downloads from the list |
| `store_searches` | yes/no | remember open searches across restarts (*not active yet*) |
| **Connection** | | |
| `download_capacity_kbps` | integer ≥ 1 | line download capacity in kB/s (graph scale; *not active yet*) |
| `upload_capacity_kbps` | integer | line upload capacity in kB/s; **0 = unknown** (used to size the upload slots) |
| `max_download_kbps`, `max_upload_kbps` | integer | limit in kB/s; **0 = unlimited** |
| `tcp_port`, `udp_port` | integer | listening ports (1–65535; UDP also 0 = off). *From restart* |
| `upnp_enabled` | yes/no | UPnP: asks the router to forward the TCP and UDP ports (it turns on and off at once, without restart; the state is in `GET /status`, field `upnp`) |
| `max_sources_per_file` | integer | sources kept for each download |
| `max_connections` | integer | connections open at once (maximum) |
| `network_ed2k`, `network_kademlia` | yes/no | which networks to use (Kad: *from restart*) |
| `auto_connect` | yes/no | connect to a server at start (*from restart*) |
| `reconnect` | yes/no | reconnect if the server drops |
| `show_overhead` | yes/no | count protocol messages (overhead) separately in the statistics |
| **Proxy** (*not active yet*) | | |
| `proxy_enabled` | yes/no | use a proxy. **It cannot be set to `true`** (422): the proxy layer is not in emuled yet and would break every connection |
| `proxy_type` | integer 0–5 | 0 none, 1 SOCKS4, 2 SOCKS4a, 3 SOCKS5, 4 HTTP 1.0, 5 HTTP 1.1 |
| `proxy_host`, `proxy_port` | text, integer | proxy address and port |
| `proxy_auth`, `proxy_user`, `proxy_password` | yes/no, text, text (write-only) | proxy login |
| **Server** | | |
| `dead_server_retries` | integer | failed attempts after which a server is removed |
| `auto_update_server_list` | yes/no | download the server list at start from the addresses in `addresses.dat` |
| `add_servers_from_server`, `add_servers_from_clients` | yes/no | add to the list the servers reported by servers and clients |
| `use_server_priorities` | yes/no | choose servers by priority |
| `smart_lowid_check` | yes/no | on a low ID, try another server before giving up |
| `server_source_reask_minutes` | integer 1–60 | how often each file asks the connected server for sources, in minutes (eMule: fixed 15); `preferences.ini` key `ServerSourceReaskMinutes` |
| `safe_server_connect` | yes/no | "safe" connection: one server at a time |
| `static_servers_only` | yes/no | connect only to fixed servers |
| `manual_servers_high_priority` | yes/no | servers added by hand get high priority (*not active yet*) |
| **Files** | | |
| `add_new_downloads_paused` | yes/no | new downloads start paused |
| `advanced_time_remaining` | yes/no | remaining time from the average speed (eMule key `UseSimpleTimeRemainingcomputation`, inverted) |
| `preview_chunks_first` | yes/no | download the first and last parts first (preview) |
| `auto_down_priority`, `auto_up_priority` | yes/no | automatic priority for new downloads / new shared files |
| `transfer_full_chunks` | yes/no | finish the current block before moving to the next client |
| `start_next_paused` | integer 0–3 | when a download finishes start: 0 nothing, 1 the next paused one, 2 preferably of the same category, 3 only of the same category |
| `filename_cleanup`, `filename_cleanup_filter` | yes/no, text | clean the names of new files; parts to remove, separated by `\|` |
| `remember_downloaded_files`, `remember_cancelled_files` | yes/no | warn when adding again a file already downloaded / cancelled |
| **Notifications** (*not active yet*) | | |
| `notify_download_finished`, `notify_new_download`, `notify_chat`, `notify_log`, `notify_important_error`, `notify_new_version` | yes/no | events to notify |
| `email_enabled`, `email_to`, `email_from` | yes/no, text, text | send an email (finished downloads, important errors) |
| `smtp_server`, `smtp_port` | text, integer | outgoing mail server (port 0 = not set) |
| `smtp_security` | integer 0–2 | 0 none, 1 SSL/TLS, 2 STARTTLS |
| `smtp_auth` | integer 0–2 | 0 none, 1 PLAIN, 2 LOGIN |
| `smtp_user`, `smtp_password` | text, text (write-only) | SMTP login |
| **Statistics** | | |
| `stats_graph_interval_s`, `stats_tree_interval_s` | integer (s) | update interval of graphs (0–200) and statistics (0–100); 0 = never |
| `stats_average_minutes` | integer 1–100 | minutes used for the average speed |
| **Messages and comments** | | |
| `messages_friends_only` | yes/no | accept messages from friends only |
| `advanced_spam_filter` | yes/no | discard messages that look like automatic advertising |
| `chat_captchas` | yes/no | ask a captcha to whoever writes to you the first time |
| `message_filter` | text | words separated by `\|` that make a message be discarded |
| `comment_filter` | text | anti-spam filter for comments: words separated by `\|` (lowercase) that make a comment from another user be discarded; default: `http://\|https://\|ftp://\|www.\|ftp.`; may be empty |
| **Security** | | |
| `ip_filter_level`, `filter_servers_by_ip` | integer 0–255, yes/no | IP filter level and whether it applies to servers (*not active yet*) |
| `secure_ident` | yes/no | secure identification of clients (signature of credits) (*from restart*) |
| `crypt_supported`, `crypt_requested`, `crypt_required` | yes/no | protocol obfuscation (RC4): supported, preferred, required (*from restart*) |
| `search_spam_filter` | yes/no | hide fake search results (*not active yet*) |
| `see_shares` | integer 0–2 | who can see your shared files: 0 everybody, 1 friends, 2 nobody |
| **Scheduler** (*not active yet*) | | |
| `scheduler_enabled` | yes/no | use the scheduler |
| **Advanced** (eMule's "Extended" page) | | |
| `max_connections_per_5s` | integer | new connections every 5 seconds (protects the router) |
| `max_half_connections` | integer | connections started but not yet accepted (an old Windows XP limit) |
| `fast_connection_reject` | yes/no | reject extra connections at once |
| `server_keepalive_minutes` | integer 0–1440 | refresh the server connection every N minutes; 0 = never |
| `credit_system` | yes/no | credit system (whoever gave you data moves ahead in the queue) |
| `filter_lan_ips` | yes/no | ignore servers and sources with local-network addresses |
| `a4af_save_cpu` | yes/no | fewer A4AF ("ask for another file") checks |
| `own_hostname` | text | host name for your own ed2k links |
| `sparse_part_files`, `allocate_full_file` | yes/no | `.part` files as sparse files; otherwise reserve the whole size at once |
| `check_disk_space`, `min_free_disk_space_mb` | yes/no, integer 0–4095 (MB) | stop downloading below this free space |
| `safe_file_writing` | integer 0–2 | flush `.met`/`.dat` files to disk: 0 never, 1 on shutdown, 2 always |
| `extract_metadata` | integer 0–1 | read media data (ID3 tags) of shared files (*not active yet*) |
| `file_buffer_kib` | integer 16–1536 (KiB) | data kept in memory per file before writing |
| `upload_queue_size` | integer 2000–10000 | users that can wait in the upload queue |
| `save_log_to_disk` | yes/no | also write the log to the file `logs/emuled.log` of the profile (it stays after a restart or a sudden stop; at 20 MB it moves to `emuled.log.1`) |
| `verbose_log` | yes/no | also write debug messages to the log (there are many: useful for hunting a problem) |
| `log_level` | integer 1–5 | debug detail (5 = everything) |
| `save_debug_to_disk` | yes/no | also write the debug lines to the file (applies only with `verbose_log` on) |
| `log_source_exchange`, `log_banned_clients`, `log_ratings`, `log_secure_ident`, `log_filtered_ips`, `log_file_saving`, `log_a4af`, `log_updown_events` | yes/no | extra debug topics |
| `uss_enabled`, `uss_min_upload_kbps`, `uss_ping_tolerance_percent`, `uss_ping_tolerance_ms`, `uss_use_ms_tolerance`, `uss_going_up_divider`, `uss_going_down_divider`, `uss_pings` | yes/no, integers | Upload SpeedSense: finds the upload limit from the ping (*not active yet*) |
| `upnp_close_on_exit` | yes/no | UPnP: removes the rules from the router when the daemon stops |

The folders and the categories have their own routes (`/directories`, section 10).

### `PATCH /api/v1/preferences` — change
Body: only the keys to change, e.g. `{"max_upload_kbps": 50}`. **Everything is checked first**: if a key does not exist or a value is not valid (type, limits) it answers **422** and changes nothing. Answer: `preferences` (all of them, updated), `restart_required` (the changed keys that apply from the next start) and `saved`.

`saved` is `false` when the daemon is started with a **temporary** identity or ports (with `--fake-profile`, or with `--tcp-port`/`--udp-port`): in that case the preferences change only in memory, because saving them would write the temporary user hash and the provisional ports into the profile.

## 10. Categories

They serve to sort downloads (destination folder, priority). Category **0** ("all") always exists and cannot be deleted, renamed or given another folder.

- `GET /api/v1/categories` — the list: `id`, `title`, `comment`, `incoming_path`, `priority` (0 low, 1 normal, 2 high, 3 highest, 4 very low, 5 automatic), `auto_cat` (words that assign the category to new downloads), `regexp`, `color` (-1 = default).
- `POST /api/v1/categories` — create (`title` is required); **201**, saved at once in `Category.ini`.
- `PATCH /api/v1/categories/{id}` — changes only the given fields.
- `DELETE /api/v1/categories/{id}` — deletes; its downloads go back to "all" and the following ids shift down by one.

To put a download in a category: `"category": ID` in `POST /downloads`, or `PATCH /api/v1/downloads/{hash}` with `{"category": ID}` and/or `{"priority": "high"}` (`auto`, `very_low`, `low`, `normal`, `high`, `release`).

## 11. Folders

### `GET /api/v1/directories`
`incoming` (where **completed** files go; it is shared automatically), `temp` (where the `.part` files are while downloading: at least one, if there is more than one eMule alternates them) and `shared` (the other folders you offer to others). In addition, read only, `shared_missing`: those of `shared` that do not exist on disk (they stay in the list until you remove them).

### `PUT /api/v1/directories`
Body with one or more fields: `{"incoming": "/dati/Incoming", "temp": ["/dati/Temp"], "shared": ["/home/utente/Libri"]}`. **Everything is checked first** and, if there is an error (**422**), nothing changes:
- paths must be **absolute** and without `..`;
- `incoming` and `temp` are created if missing and must be **writable**; `shared` must exist and be **readable**;
- `incoming` and `temp` must be different; the folder for completed files and the temporary one **cannot be shared** (the first already is);
- no duplicates.

When `shared` changes, the daemon rechecks the files and computes the hashes of the new ones (it can take a while: the `/shared` list fills up gradually). The `.part` files already started **stay where they are**: the new temporary folder applies to new downloads. The answer has `directories` (the state after the change) and `saved`: as with preferences, the folders are written to the profile only if the daemon uses the profile's identity and ports; otherwise they apply as long as it is running.

## 12. Event log

It is eMule's "Log" tab: everything the daemon writes while working (connections to servers, errors, warnings) plus a `[api] …` line for every action done through the API (download added or cancelled, server added, preferences changed, …), so you can see **who did what**. It is an in-memory buffer of the last 3000 lines: it starts empty again when the daemon restarts.

### `GET /api/v1/log` — read

Optional parameters:
- `limit` — from 1 to 2000 lines (default 500);
- `level` — minimum level: `debug`, `info` (default), `warning`, `error`;
- `after` — number of the last line already seen: it returns only the **newest** ones, from the oldest. Without `after` it returns the last `limit`.

Answer: `lines` (each with a progressive `id` that is never reused, `time` in Unix milliseconds, `level`, `source` and `text`), `last_id` (to pass as `after` next time) and `truncated` (with `after`: some lines in between were already discarded from the buffer). `source` is `general`, `server` (messages from the ed2k servers) or `api` (actions done through the API).

The **debug** lines appear only with the preference `verbose_log` on (there are many and they slow things down a little).

### `DELETE /api/v1/log` — clear

It removes all the lines (`{"removed": N}`); the numbering continues. To follow the log live, the WebSocket is enough (event `log`).

## 13. Interface templates

A *template* is a folder with a small web application that talks only to this API; it is chosen in Preferences and the choice is remembered in the browser (see "The daemon's other addresses" at the beginning). The daemon looks for them in `/usr/share/emuled/templates/` (built-in), `~/.emuled/templates/` (the user's) and in the extra folder `--templates-dir`; every subfolder with a `template.yaml` is a candidate.

### `GET /api/v1/templates` — the list

Answer: `templates` (one element per folder found, **including the invalid ones**, to explain to the user why they do not appear), `api_version` (the version of the template API that the daemon understands: 1) and `dirs` (the folders searched: `system`, `user`, `extra`; useful to tell the user where to copy a template). For each template: `id`, `name`, `version`, `description`, `author`, `license`, `api` (version required), `source` (`system`, `user` or `dev` = extra folder), `valid`, `errors` (list of sentences, empty if valid), `reserved` (name reserved for the built-in templates), `dir_name`, `url` (`/t/<id>/`, `null` if not valid) and `preview` (address of the preview image, `null` if missing).

### `POST /api/v1/templates/rescan` — rereads the folders

To be used after copying or removing a template. It answers with the same list. In the Log the summary appears ("Template: N loaded, M invalid") and a warning for every invalid template.

The choice of template does **not** go through the API: it lives only in the browser.

## 14. Still to do

Friends. Each group will have its own section here and its entries in `openapi.json` when it exists: the manual describes only what is there.
