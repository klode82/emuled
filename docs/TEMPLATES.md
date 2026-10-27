# How to create an interface template

This document is for anyone who wants to make a new look for emuled. You do not need to know emuled's code: a template talks to the daemon **only through its API** (`docs/APIDOC.md`, `docs/openapi.json`) and can be written however you like (an HTML page, Vue, React, whatever you prefer). The smallest possible example that works is in [`docs/examples/template-ciao/`](examples/template-ciao/).

## 1. In a nutshell

A *template* is a **folder** with:
- a **`template.yaml`** file that describes it;
- an **entry page** (usually `index.html`) and all the files it needs (scripts, styles, images);
- optionally, a **preview image**.

Copy it to `~/.emuled/templates/<id>/` and it appears in the list of templates. The built-in ones live in `/usr/share/emuled/templates/` and **must not be touched**: to start from one of them, copy the folder and give it another name.

## 2. The `template.yaml` file

`key: value` lines, comments with `#`, no lists and no nesting. Keys it does not know are ignored.

| Key | Required | Meaning |
|---|---|---|
| `id` | yes | From 2 to 32 characters: lowercase letters, digits and hyphen (not at the start). **Must be equal to the folder name.** The names `console`, `dashboard`, `studio`, `mobile` are reserved for the built-in templates. |
| `name` | yes | The name shown in Preferences (at most 60 characters). |
| `version` | yes | Three numbers separated by dots: `1.0.0`. |
| `description` | no | One sentence (at most 300 characters). |
| `author`, `license` | no | Who made it, and under which license. |
| `api` | no | The version of the template API that you need. Default `1`. A daemon older than that rejects the template and explains why. |
| `entry` | no | The entry page, path relative to the folder. Default `index.html`. |
| `preview` | no | Preview image (PNG or JPEG), relative path. |

Paths (`entry`, `preview`) can only contain letters, digits and `. _ - /`; no `..`, no leading `/`.

## 3. How it is served

The daemon serves the folder at **`/t/<id>/`**, without asking for the token (the pages are public; the data is not: the API requires it). So:
- write the addresses of your files **relative** (`app.js`, `img/logo.png`, not `/app.js`), because the template is not at the root;
- single-page applications are fine: a path with no extension that does not exist returns the entry page;
- you cannot get out of the folder (neither with `..` nor with symbolic links) and no code runs on the server: only files.

## 4. The rules of the contract

They are few, but they are needed so that templates can live together.

1. **The token** is in the browser under the key `localStorage['emuled.token']` (plain text). If it is there, use it and **do not ask to sign in again**: it is valid for all templates. If it is missing or the daemon answers `401`, show your own sign-in screen and save it under the **same key**. The normal sign-in is with **username and password**: `GET /api/v1/auth/status` says whether the first start is needed (`setup_required`), `POST /api/v1/auth/login` `{"username","password"}` answers with `{"token"}`, which is a session token to save under the key and to use as Bearer. The first account is created with `POST /api/v1/auth/setup` (it needs the setup code printed by the daemon). The token in the `emuled.token` file is still valid (scripts and tools): offer that too if you want. Details in `docs/APIDOC.md`.
2. **Language and theme** (optional, but welcome): `localStorage['emuled.lang']` is `it` or `en`; `localStorage['emuled.theme']` is `system`, `light` or `dark`.
3. **A way to change the look**: put a link to **`/?template=console`**. It leads to the default template, where the choice is made in Preferences. If you want to offer the choice directly, `GET /api/v1/templates` lists the templates (also with `preview`) and, to switch, just write `localStorage['emuled.template'] = '<id>'` and open `/`.
4. **Do not send the token to anyone**: only to `/api/v1/…` on the same address you were opened from. No scripts, styles, fonts or images taken from other sites: a template must work even without internet. The daemon **enforces** this with a security header (`Content-Security-Policy`) on the template pages: requests (`fetch`, WebSocket), scripts, styles, fonts, images and audio/video are only allowed towards the same address as the daemon (plus `data:` and `blob:`); scripts and styles written inside the page are allowed. A request to another site is blocked by the browser. See also section 7.
5. **No HTML from data**: file names, user names and comments are decided by whoever shares them. Show them with `textContent` (or the equivalent in your tool), never with `innerHTML`.

## 5. Talking to the daemon

Everything goes through the REST API at `/api/v1/…` with the header `Authorization: Bearer <token>`; live updates come from the WebSocket `/api/v1/events?token=<token>`. The manual for the calls is `docs/APIDOC.md`; the machine-readable description is `docs/openapi.json` (also at `/api/v1/openapi.json`, without a token). Example:

```js
const token = localStorage.getItem('emuled.token');
const stato = await fetch('/api/v1/status', { headers: { Authorization: 'Bearer ' + token } }).then(r => r.json());
```

The "contract" with templates is this API: as long as a call of the `/api/v1/…` kind exists, your template works. The `api` number in the manifest says which version of the templates you need.

## 6. Trying it out

1. Copy the folder to `~/.emuled/templates/<id>/`.
2. Ask the daemon to read again: `POST /api/v1/templates/rescan` (or restart it). The emuled **Log** says "Templates: N loaded, M invalid" and, for each template that does not load, the reason.
3. Open `http://127.0.0.1:8765/?template=<id>`.
4. If the template you had chosen disappears or breaks, the help page says so and **removes the choice by itself**: the next time the default one opens. If something goes wrong, `http://127.0.0.1:8765/?template=console` takes you back to the default template.

To develop without copying every time: start the daemon with `--templates-dir <your folder>` (or `templates_dir:` in `config.yaml`, or `EMULED_TEMPLATES_DIR`) and put the template there.

## 7. Security, stated plainly

A template is a **program that runs in the page** and sees the token: whoever writes it could, in theory, use it to command your daemon. That is why emuled does not download templates from anywhere and never writes into the template folders: **only install templates you trust**, as you would with any program. Preferences warn you when you choose a template that is not built-in.

What the daemon does to limit the damage: it only serves normal files inside the template folder (no `..`, no links that leave it; **at most 32 MB per file**), it sends on the template pages the `Content-Security-Policy` header described above plus `Referrer-Policy: no-referrer` and `X-Frame-Options: DENY` (the template cannot be framed in another site), it accepts a manifest of at most 8 KB and a preview of at most 2 MB. **This is not enough to stop everything**: a malicious template could still move the page to another site, taking the token with it. That is why the rule remains: only templates you trust.

## 8. The example

[`docs/examples/template-ciao/`](examples/template-ciao/) is a complete template in two files (`template.yaml` and `index.html`): it asks for the token if it is missing, shows the speed and the list of downloads, and has the "Change look" link. It is the recommended starting point.
