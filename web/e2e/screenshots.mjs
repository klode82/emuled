// Screenshots of the four templates, always from the same demo data (no real profile, no network):
//   - docs/images/frontend-<template>.png   used by the README (1360x820; mobile 390x780 at 2x)
//   - web/templates/<template>/preview.png  the preview shown in Preferences (1280x800)
// It starts its own emuled (fake profile, --offline, test ports), creates the account, adds four sample downloads, takes the pictures and stops the daemon.
// Needs: web/dist/templates (npm run build) and ../build/emuled.   Run:  npm run screenshots   (from web/)
import puppeteer from 'puppeteer-core'
import { spawn } from 'node:child_process'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'
import { fileURLToPath } from 'node:url'

const here = path.dirname(fileURLToPath(import.meta.url)), web = path.resolve(here, '..'), root = path.resolve(web, '..')
const bin = process.env.EMULED_BIN ?? path.join(root, 'build/emuled')
const PORT = Number(process.env.SHOT_PORT ?? 18890)
const base = `http://127.0.0.1:${PORT}`
const tmp = path.join(os.tmpdir(), 'emuled-demo')		// fixed, neutral name: the Download details show the Temp path
fs.rmSync(tmp, { recursive: true, force: true }); fs.mkdirSync(tmp)
const wait = (ms) => new Promise((r) => setTimeout(r, ms))
for (const f of [bin, path.join(web, 'dist/templates')]) if (!fs.existsSync(f)) { console.error('Missing: ' + f); process.exit(2) }

fs.writeFileSync(path.join(tmp, 'conf.yaml'), '# screenshots\n')
fs.mkdirSync(path.join(tmp, 'profile'))
const daemon = spawn(bin, ['--run', '--fake-profile', '--profile', path.join(tmp, 'profile'), '--offline', '--no-kad', '--api-port', String(PORT), '--tcp-port', '23211', '--udp-port', '23212',
  '--templates-dir', path.join(web, 'dist/templates'), '--status-interval', '0'], { env: { ...process.env, HOME: tmp, EMULED_CONF: path.join(tmp, 'conf.yaml') }, stdio: 'ignore' })
let b
const stop = async () => { try { await b?.close() } catch { /* already closed */ } daemon.kill('SIGTERM'); await wait(500); fs.rmSync(tmp, { recursive: true, force: true }) }
process.on('SIGINT', async () => { await stop(); process.exit(130) })

try {
  const tokenFile = path.join(tmp, 'profile/emuled.token')
  for (let i = 0; i < 60 && !(fs.existsSync(tokenFile) && (await fetch(base + '/api/v1/ping').then((r) => r.ok).catch(() => false))); i++) await wait(250)
  const code = fs.readFileSync(tokenFile, 'utf8').trim()
  const api = (m, p, token, body) => fetch(base + '/api/v1' + p, { method: m, headers: { Authorization: 'Bearer ' + token }, body: body ? JSON.stringify(body) : undefined })
  const acc = await (await fetch(base + '/api/v1/auth/setup', { method: 'POST', body: JSON.stringify({ setup_code: code, username: 'demo', password: 'demo-password' }) })).json()
  const session = acc.token
  await api('POST', '/setup/done', session)
  const files = [['debian-12.5.0-amd64-DVD-1.iso', 3994091520, 'A1B2C3D4E5F60718293A4B5C6D7E8F90'], ['Project_Gutenberg_Collection.zip', 812342845, 'B2C3D4E5F60718293A4B5C6D7E8F90A1'],
    ['Public_Domain_Film_1936.mkv', 1503238553, 'C3D4E5F60718293A4B5C6D7E8F90A1B2'], ['ubuntu-24.04.1-desktop-amd64.iso', 6342123520, 'D4E5F60718293A4B5C6D7E8F90A1B2C3']]
  for (const [n, s, h] of files) await api('POST', '/downloads', session, { link: `ed2k://|file|${n}|${s}|${h}|/` })
  await wait(1500)

  b = await puppeteer.launch({ executablePath: process.env.CHROME ?? '/usr/bin/google-chrome', args: ['--no-sandbox'], headless: 'new' })
  const shot = async (tpl, w, h, scale, out) => {
    const p = await b.newPage(); await p.emulateMediaFeatures([{ name: 'prefers-color-scheme', value: 'light' }])
    await p.setViewport({ width: w, height: h, deviceScaleFactor: scale })
    await p.goto(`${base}/t/${tpl}/`); await p.evaluate((t) => { localStorage.setItem('emuled.token', t); localStorage.setItem('emuled.lang', 'en') }, session)
    await p.goto(`${base}/t/${tpl}/`); await wait(2500)
    fs.mkdirSync(path.dirname(out), { recursive: true }); await p.screenshot({ path: out }); await p.close(); console.log('ok  ', path.relative(root, out))
  }
  for (const tpl of ['console', 'dashboard', 'studio', 'mobile']) {
    await shot(tpl, tpl === 'mobile' ? 390 : 1360, tpl === 'mobile' ? 780 : 820, tpl === 'mobile' ? 2 : 1, path.join(root, `docs/images/frontend-${tpl}.png`))
    await shot(tpl, 1280, 800, 1, path.join(web, `templates/${tpl}/preview.png`))
  }
} finally { await stop() }
