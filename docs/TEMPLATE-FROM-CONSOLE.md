# Manual: creating a template starting from "Console" (model A)

For anyone who wants a new look **reusing the emuled kit** (data, screens, components, translations) and changing only what is needed: colors, fonts, shell (side bar, top bar, status bar) and the Transfers screen.
If instead you want to write a template from scratch, without the kit (an HTML page or another framework), read [`TEMPLATES.md`](TEMPLATES.md): the contract with the daemon is the same.

You need the emuled **source** (the `web/` folder) and Node.js 20 or newer. The built-in templates are built exactly this way.

## 1. How "Console" is made

`web/templates/console/` contains only 7 files:

| File | What it is for |
|---|---|
| `template.yaml` | identity card (see `TEMPLATES.md`, section 2) |
| `index.html` | entry page: a `<div id="app">` and the `main.ts` script |
| `main.ts` | imports fonts and colors, and mounts the application with `createTemplateApp({ shell, transfers })` |
| `tokens.css` | the **colors** and the **fonts** (CSS variables) |
| `Shell.vue` | the **frame**: side bar, "search or paste a link" box, status bar |
| `TransfersView.vue` | the **Transfers > Download** screen |
| `preview.png` | preview shown in Preferences (at most 2 MB) |

Everything else lives in the **common kit** `web/src/`: data stores (`stores/`), API calls (`lib/api.ts`), screens (`views/`: Search, Servers, Shared, Kad, Log, Statistics, Preferences, Upload), components (`components/`), Italian and English translations (`i18n/`). The kit is **included in the template at build time**: at runtime templates share nothing, they only talk to the API.

## 2. Step by step: the "Mine" template

### 2.1 Copy the folder

    cd web
    cp -r templates/console templates/mio

The `id` must be 2-32 characters made of lowercase letters, digits and hyphen. **`console`, `dashboard`, `studio`, `mobile` are reserved** for the built-in templates.

### 2.2 Change `template.yaml`

    id: mio
    name: My look
    version: 1.0.0
    description: A sentence that explains what it looks like.
    author: Your name
    license: GPL-2.0-or-later
    api: 1
    entry: index.html
    preview: preview.png

`id` equal to the folder name, otherwise `npm run build:template` refuses. The license must be compatible with the GPL, because the kit is (see section 6).

### 2.3 Change the colors and fonts (`tokens.css`)

The kit components only read these **names** (through Tailwind: `bg-bg`, `text-fg`, `border-line`, `bg-accent`, ...). You change the **values**:

| Variable | What it colors |
|---|---|
| `--bg` | page background |
| `--surface` | panels, bars, cards |
| `--sunk` | recessed background (fields, mouse hover) |
| `--line` | borders and separators |
| `--fg` / `--mute` | normal text / secondary text |
| `--accent` / `--onaccent` | main color / text on top of it |
| `--soft` | soft accent (active item, labels) |
| `--ok`, `--warn`, `--bad` | states: success, warning, error |
| `--font-sans`, `--font-mono` | normal and fixed-width fonts |

The file has **three blocks**: light theme (`:root`), dark when the system chooses it (`@media (prefers-color-scheme: dark)`), forced dark (`:root[data-theme="dark"]`). Change the values in all three. Check that the text has good contrast (at least 4.5 : 1 with the background).

Fonts: to stay **without internet** use the `@fontsource/*` packages already present (`dm-sans`, `ibm-plex-sans`, `ibm-plex-mono`, `manrope`, `public-sans`): they are imported in `main.ts` (e.g. `import '@fontsource/manrope/500.css'`) and named in `--font-sans`. **Never a font from another site**: the daemon blocks everything that does not come from it (see `TEMPLATES.md`, section 4, rule 4).

### 2.4 Change the frame (`Shell.vue`)

It is a Vue component with Tailwind classes. The kit already gives it ready:
- `useShellBar()` (`@/lib/shell`): the search/link box (`q`, `go`, `onPaste`) and the ed2k and Kad status labels (`ed2k`, `kad`, `st`);
- `SECTIONS` (`@/router`): the list of sections (name, path, icon) to build the menu;
- `useSession()`: `logout()`, `link` (state of the connection with the daemon); `useUi()`: `openAdd()` opens the "Add download" window;
- `<RouterView />`: **the chosen screen goes in here**. Without it nothing is shown.
- the translations: `t('nav.transfers')`, `t('shell.ports', {...})` ... (keys in `src/i18n/it.ts` and `en.ts`).

Rules not to break (they apply to all templates):
1. **Only Tailwind classes** for the layout. No hand-written CSS and no `style=`.
2. **The frame never scrolls**: only the content does. The pattern is `h-full overflow-hidden` on the root and `min-h-0 flex-1 overflow-y-auto` on the `<main>` that contains `<RouterView />`.
3. It must work **from a phone to a wide screen**: below `md` a bottom bar, from `md` icons, from `lg` labels (see how Console does it).
4. **Texts always from `t()`**, never written in the template: the interface comes in 14 languages.
5. Users' data (file names, comments) is shown with `{{ }}` (which treats it as text), **never with `v-html`**.

### 2.5 The Transfers screen (`TransfersView.vue`)

It is the only screen that you draw yourself in every template (optionally also `uploads`). If the Console one is enough for you, **copy it without touching it**. Otherwise start from there and use the kit's data:
- `useDownloads()` (`@/stores/downloads`): `list` (filtered and sorted rows), `counts`, `selected`, `detail`, actions `pause`, `resume`, `cancel`, `setCategoryMany`...
- `statusInfo(d)`, `toneClass(...)` (`@/lib/labels`) for state and label; `bytes`, `speed`, `eta`, `percent` (`@/lib/format`) for the numbers;
- the components `ProgressBar`, `StatusIcon`, `DownloadDetail` (detail card), `TransfersTabs`, `EmptyState`.

### 2.6 `main.ts` and `index.html`

`main.ts` is all here:

    createTemplateApp({ shell: Shell, transfers: TransfersView })       // + uploads: UploadsView, if you have one of your own

In `index.html` only change `<title>` if you want. Keep `class="h-full overflow-hidden bg-bg ..."` on the `<body>`: it is needed for the rule "the frame does not scroll".

### 2.7 Preview

A PNG or JPEG image, at most 2 MB, usually 1280x800: take a screenshot of your template in action and save it as `preview.png`.

## 3. Building and trying

    cd web
    npm install
    npm run build:template -- mio        # builds only yours: dist/templates/mio/
    npx vue-tsc --noEmit                # type check
    npx vitest run                      # interface tests

To try it live without touching the real profile:

    mkdir -p /tmp/prova/config
    build/emuled --fake-profile --offline --api-port 18765 --profile /tmp/prova/config --templates-dir web/dist/templates
    # open http://127.0.0.1:18765/?template=mio   (the token is in /tmp/prova/config/emuled.token)

Development with instant reload: `TEMPLATE=mio EMULED_API=http://127.0.0.1:18765 npm run dev`.

## 4. Installing and distributing

Copy the built folder (**not** the source one) to `~/.emuled/templates/mio/`, then `POST /api/v1/templates/rescan` or restart the daemon. The Log says whether the template was loaded or why not. To choose the look: Preferences > Look.
To distribute it: a `.tar.gz` or `.zip` archive of the `dist/templates/mio/` folder, with the license. Whoever installs it must trust you: a template is a program that runs in the page and sees the token (`TEMPLATES.md`, section 7).

## 5. If something goes wrong

| Symptom | Likely cause |
|---|---|
| `l'id deve essere "mio"` | `id:` in `template.yaml` is different from the folder name |
| The template does not appear in the list | invalid manifest (look at the Log) or folder not built (`index.html` missing in `dist`) |
| Blank page | `<RouterView />` missing in the shell, or an error in the browser console |
| Colors that do not change | variable changed in only one of the three blocks of `tokens.css` |
| Font or image that does not load | it comes from another site: the daemon's CSP blocks it; include it in the folder |
| Tailwind classes with no effect | the file is not in `templates/mio/` or `src/`: Tailwind only looks for classes there |
| The content scrolls together with the frame | `overflow-hidden` missing on the root or `overflow-y-auto` on the `<main>` |

## 6. License

The kit (`web/src/`) and the built-in templates are **GPL-2.0-or-later**. A template built with the kit embeds kit code: it must also be distributed under a compatible GPL license and with the source. A template written from scratch (`TEMPLATES.md`) only talks to the API and can have whatever license you want.
