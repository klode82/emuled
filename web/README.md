# emuled — web interface

Vue 3 + Vite + TypeScript + Pinia + Tailwind. It talks to the daemon only through its API (`docs/APIDOC.md`, `docs/openapi.json`):
REST for commands, WebSocket (`/api/v1/events`) for live updates. If the WebSocket drops, the interface reconnects by itself and,
in the meantime, polls the API every 5 seconds.

## See it right away

```bash
cd web && npm install && ./demo.sh        # starts an OFFLINE daemon on a temporary folder, with sample downloads and servers
```
It prints the address and the token (which on first start serves as the code to create the account); Ctrl-C stops everything and deletes the temporary folder. Your profile is not touched.

## Try it with your profile

```bash
cd web
npm install
npm run build                                   # creates dist/templates/<id>/ (one template per folder)
emuled --profile CONFIG --templates-dir dist/templates    # the daemon serves the API and the templates on the same port (8765)
# then open http://127.0.0.1:8765/ and create your account with the setup code printed by the daemon (CONFIG/emuled.token) and sign in with username and password
```
The `/` page leads to the template chosen in the browser (with no choice, `console`); `http://127.0.0.1:8765/?template=console` is the emergency address.
To always use it this way, in the daemon's `config.yaml`: `templates_dir: /path/to/web/dist/templates`.

In development (`TEMPLATE=dashboard npm run dev`) Vite serves the chosen template on port 5173 and proxies `/api` (the WebSocket too) to the daemon: `EMULED_API=http://127.0.0.1:8765 npm run dev`.

## Built-in templates

The interface is made of **templates**: each one is a small independent application (`dist/templates/<id>/`, with its own `template.yaml`) that only talks to the API.
The four built-in ones have the same data, the same common screens (Search, Servers, Shared, Kad, Log, Statistics, Preferences) and the same
download detail panel; they change the frame, the colors and the Transfers screen. How to create your own template: `docs/TEMPLATES.md` (from scratch) and `docs/TEMPLATE-FROM-CONSOLE.md` (from Console, with the kit).

| Template | Folder | What it looks like |
|---|---|---|
| A · Console (`console`) | `templates/console/` | side bar, dense table, status bar (default and fallback) |
| B · Dashboard (`dashboard`) | `templates/dashboard/` | sections as pills at the top, summary tiles (speed with trend, ed2k server, Kad), a card with a ring for each download, detail in a side panel |
| C · Studio (`studio`) | `templates/studio/` | tabs at the top, command box (Ctrl+K), compact list on the left and detail on the right, fixed-width status bar |
| D · Mobile first (`mobile`) | `templates/mobile/` | large search box, rows with a ring, detail as a sheet from the bottom (phone) or a column on the right, bottom bar and + button |

Each folder has: `Shell.vue` (the shell), `TransfersView.vue` (the Transfers screen), `tokens.css` (the colors), `main.ts` (the entry point), `index.html`, `template.yaml`
(the manifest) and `preview.png`. The **common kit** (`src/`: data, screens, components, translations) is included in every template at build time; at runtime templates
share nothing. Colors are CSS tokens (`--bg`, `--accent`, ...): each template redefines the same names; the rest is only Tailwind classes. **Fonts belong to the template**: Console uses IBM Plex Sans, Dashboard Manrope, Studio Public Sans, Mobile first DM Sans (plus IBM Plex Mono for fixed-width text). They are imported in the template's `main.ts` (`@fontsource/…`, local packages) and the family is the `--font-sans` (and `--font-mono`) variable in `tokens.css`: the font files end up in the template folder and nothing is downloaded from outside. Code shared between the shells: `src/lib/shell.ts` (search/link box, ed2k and Kad status), `src/lib/transferActions.ts` (cancel, pause), `src/template.ts` (mounts the application).

## Commands

| Command | What it does |
|---|---|
| `npm run build` | checks the types (`vue-tsc`) and builds **all** templates in `dist/templates/` |
| `npm run build:template -- dashboard` | builds a single template |
| `TEMPLATE=studio npm run dev` | development of a template (default `console`) |
| `npm run typecheck` | type check only |
| `npm test` | unit tests (Vitest): formats (Italian and English), stores, API client, WebSocket, translations |
| `npm run e2e:templates` | browser test of the template choice (entry page, Preferences, warning, emergency): see the top of `e2e/templates.mjs` |
| `npm run e2e` | test in the real browser (Chrome + puppeteer-core) against a running daemon: see `e2e/flow.mjs` (uses `EMULED_URL=…/t/console/`) |
| `npm run screenshots` | redoes the README screenshots (`docs/images/frontend-*.png`) and the template previews (`templates/*/preview.png`): it starts an offline test daemon by itself with four sample downloads (needs `../build/emuled` and `dist/templates`); then `npm run build` |

## How it is made

```
src/lib/        API client (api.ts), WebSocket with reconnection (events.ts), formats (format.ts), texts (labels.ts, prefsMeta.ts), shell.ts, transferActions.ts
src/stores/     Pinia: session (token, connection), core (state, speed history), downloads, search, servers, shared, prefs, ui, toasts
src/components/ pieces with no graphic model: Icon, ProgressBar, Ring, Sparkline, Modal, dialogs, DownloadDetail, SpeedChart, ...
src/views/      screens common to all templates: Search, Servers, Shared, Kad, Statistics, Preferences, Login
src/i18n/       translations: it.ts (reference) and en.ts; language choice
src/template.ts, src/router.ts, src/App.vue   the common application: the template gives it its shell and its Transfers screen
templates/<id>/ the built-in templates (see above)
build-templates.mjs   builds the templates (one at a time, each in dist/templates/<id>/)
```

**Why the states and components live in the kit.** A template is only a *shell* (navigation and layout) plus the Transfers screen. Logic, API, filters, sorting and formats
are written once: adding a built-in template touches none of this.

## The token

Every request carries `Authorization: Bearer TOKEN`; the WebSocket takes it from `?token=`. The token stays in the browser's `localStorage` until you sign out
(or until the daemon answers 401). The daemon only listens on `127.0.0.1`: to use it from another machine you need a proxy with TLS in front.

## Keyboard

`Del` cancels the selected downloads (with confirmation) · `Ctrl+A` selects all · `Esc` closes the detail or clears the selection · `Enter` in the top box searches (or opens "Add download" if it is an ed2k link).

## Languages (i18n)

Italian, English and 12 other languages (Spanish, Chinese, Hindi, Arabic, French, Portuguese, Russian, German, Japanese, Korean, Turkish, Indonesian). It is chosen in Preferences or on the sign-in screen and remembered in the browser; the first time it ALWAYS starts in English (even with an Italian browser), then the user changes it.

- No text lives in the components: there are only keys (`t('transfers.emptyTitle')`). The texts are in `src/i18n/it.ts`; `en.ts` and the other 12 languages (`es zh hi ar fr pt ru de ja ko tr id`) have **the same keys** (the TypeScript type enforces it).
- The `i18n` test also checks: no empty text, same placeholders (`{n}`) and same plural forms in all languages, and that every key used in the code exists.
- Plurals: `"1 file | {n} file"`; numbers, sizes, dates and durations follow the language (`15,47 MB` in Italian, `15.47 MB` in English).
- **To add a language**: copy `it.ts`, translate, add it to `LANGS` in `src/i18n/index.ts` and to `messages`.
- Error messages coming from the daemon are in English (curl users see them as they are) and carry a stable `code`; the interface shows the translation of the code (`apiErrors`) and falls back to the daemon's English text when a code has no translation.
