// Real-browser test of template selection: entry page, list in Preferences, choice remembered in the browser, warning for non-built-in templates,
// invalid templates with the reason, emergency address. Against a RUNNING, OFFLINE daemon, with the built-in templates and two user templates
// (one good, one without an entry page):
//   mkdir -p H/.emuled/templates/mio H/.emuled/templates/rotto && cp -r ../docs/examples/template-ciao/. H/.emuled/templates/mio/ (with id: mio) ...
//   export E2E_USER_TEMPLATES=H/.emuled/templates        (the error-case tests move and create folders in there, and put everything back as it was)
//   HOME=H emuled --run --fake-profile --profile H/config --offline --api-port 18766 --templates-dir dist/templates &
//   EMULED_URL=http://127.0.0.1:18766/ EMULED_TOKEN_FILE=H/config/emuled.token npm run e2e:templates
import puppeteer from 'puppeteer-core'
import fs from 'node:fs'
import path from 'node:path'

const origin = new URL(process.env.EMULED_URL ?? 'http://127.0.0.1:8765/').origin
const token = (process.env.EMULED_TOKEN ?? (process.env.EMULED_TOKEN_FILE ? fs.readFileSync(process.env.EMULED_TOKEN_FILE, 'utf8') : '')).trim()
if (!token) { console.error('EMULED_TOKEN or EMULED_TOKEN_FILE is required'); process.exit(2) }
const userDir = process.env.E2E_USER_TEMPLATES
const api = (m, p) => fetch(new URL('/api/v1' + p, origin).href, { method: m, headers: { Authorization: 'Bearer ' + token } }).then((r) => r.json())
const wait = (ms) => new Promise((r) => setTimeout(r, ms))
const fails = []
const ok = (c, m) => { if (!c) { fails.push(m); console.log('FAIL', m) } else console.log('ok  ', m) }
const b = await puppeteer.launch({ executablePath: process.env.CHROME ?? '/usr/bin/google-chrome', args: ['--no-sandbox'], headless: 'new' })
const p = await b.newPage(); const errs = []
p.on('pageerror', (e) => errs.push(e.message))
await p.evaluateOnNewDocument(() => document.addEventListener('securitypolicyviolation', (e) => console.error('CSP: ' + e.violatedDirective + ' ' + e.blockedURI)))
p.on('console', (m) => { if (m.type() === 'error' && !/Failed to load resource/.test(m.text())) errs.push(m.text()) })
await p.setViewport({ width: 1280, height: 900 })
const where = () => p.evaluate(() => location.pathname + location.hash)
const key = () => p.evaluate(() => localStorage.getItem('emuled.template'))
const text = () => p.evaluate(() => document.body.innerText)
const btn = async (label, within = 'body') => (await p.evaluateHandle((l, w) => [...document.querySelector(w).querySelectorAll('button')].find((x) => x.innerText.trim() === l && x.getClientRects().length > 0), label, within)).asElement()
// the "Use this" button of the card with that name
const useBtn = async (name) => (await p.evaluateHandle((n) => [...document.querySelectorAll('#template li')].find((li) => li.querySelector('h3')?.innerText.trim() === n)?.querySelector('button'), name)).asElement()

// 1. with no choice the entry page leads to console
await p.goto(origin + '/', { waitUntil: 'networkidle0' })
await p.evaluate((t) => { localStorage.setItem('emuled.token', t); localStorage.setItem('emuled.lang', 'it') }, token)
await p.goto(origin + '/', { waitUntil: 'networkidle0' }); await wait(800)
ok((await where()).startsWith('/t/console/'), 'no choice: /t/console/')
ok((await key()) === null, 'no choice: no key in the browser')

// 2. Preferences: the Appearance section with the templates
await p.goto(origin + '/t/console/#/prefs', { waitUntil: 'networkidle0' }); await wait(1200)
let t = await text()
for (const n of ['Console', 'Cruscotto', 'Studio', 'Mobile first', 'Hello world']) ok(t.includes(n), `list: ${n}`)
ok(await p.evaluate(() => [...document.querySelectorAll('#template li')].find((li) => li.querySelector('h3')?.innerText.trim() === 'Console')?.innerText.includes('In uso')), 'Console is "In uso"')
ok(await p.evaluate(() => document.querySelectorAll('#template img').length >= 4), 'built-in template previews are visible (images loaded from the daemon)')
ok(await p.evaluate(() => [...document.querySelectorAll('#template img')].every((i) => !i.complete || i.naturalWidth > 0)), 'previews are not broken')
ok(t.includes('Non caricati') && t.includes('rotto') && /entry page/.test(t), 'the broken template is among the not loaded, with the reason')
ok(await p.evaluate(() => [...document.querySelectorAll('#template li')].find((li) => li.querySelector('h3')?.innerText.trim() === 'Hello world')?.innerText.includes('Tuo')), 'the user template is marked "Tuo"')

// 3. choose Cruscotto: /t/dashboard/ opens on the same Preferences and the choice stays
await (await useBtn('Cruscotto')).click(); await wait(2000)
ok((await where()) === '/t/dashboard/#/prefs', 'chose Cruscotto: /t/dashboard/#/prefs')
ok((await key()) === 'dashboard', 'the choice is stored in the browser')
t = await text(); ok(await p.evaluate(() => [...document.querySelectorAll('#template li')].find((li) => li.querySelector('h3')?.innerText.trim() === 'Cruscotto')?.innerText.includes('In uso')), 'now Cruscotto is "In uso"')
await p.goto(origin + '/', { waitUntil: 'networkidle0' }); await wait(800)
ok((await where()).startsWith('/t/dashboard/'), 'reopening "/" goes to the chosen template')

// 4. a user template asks for confirmation; cancelling changes nothing
await p.goto(origin + '/t/dashboard/#/prefs', { waitUntil: 'networkidle0' }); await wait(1200)
await (await useBtn('Hello world')).click(); await wait(500)
t = await text(); ok(t.includes('Usare questo template?') && t.includes('di cui ti fidi'), 'warning for a non-built-in template')
await (await btn('Annulla', 'dialog[open]')).click(); await wait(500)
ok((await where()).startsWith('/t/dashboard/') && (await key()) === 'dashboard', 'cancelling leaves everything as it was')
await (await useBtn('Hello world')).click(); await wait(500)
await (await btn('Usa questo', 'dialog[open]')).click(); await wait(1800)
ok((await where()).startsWith('/t/ciao/') && (await key()) === 'ciao', 'confirming goes to the user template')

// 5. the emergency address
await p.goto(origin + '/?template=console', { waitUntil: 'networkidle0' }); await wait(1000)
ok((await where()).startsWith('/t/console/'), 'emergency: /?template=console goes back to console')

// 6. going back to the default from Preferences removes the choice
await p.goto(origin + '/t/console/#/prefs', { waitUntil: 'networkidle0' }); await wait(1200)
await (await useBtn('Studio')).click(); await wait(1800); ok((await key()) === 'studio', 'chose Studio')
await p.goto(origin + '/t/studio/#/prefs', { waitUntil: 'networkidle0' }); await wait(1200)
await (await useBtn('Console')).click(); await wait(1800)
ok((await where()).startsWith('/t/console/') && (await key()) === null, 'choosing Console removes the choice from the browser')
// 7. the user's example template really works (uses the same token as the browser, without asking again)
await p.goto(origin + '/?template=ciao', { waitUntil: 'networkidle0' }); await wait(2000)
t = await text(); ok((await where()).startsWith('/t/ciao/') && t.includes('Hello world') && /Downloading at \d+ KB\/s/.test(t) && !t.includes('Incolla il token'), 'the user template shows live status without asking for the token')
// 8. error cases (only if the location of the user templates is known)
if (userDir) {
  // a built-in name from the user folder is rejected
  fs.mkdirSync(path.join(userDir, 'studio'), { recursive: true }); fs.writeFileSync(path.join(userDir, 'studio', 'template.yaml'), 'id: studio\nname: Falso studio\nversion: 1.0.0\n'); fs.writeFileSync(path.join(userDir, 'studio', 'index.html'), 'x')
  await api('POST', '/templates/rescan')
  await p.goto(origin + '/t/console/#/prefs', { waitUntil: 'networkidle0' }); await p.reload({ waitUntil: 'networkidle0' }); await wait(1500)
  t = await text(); ok(t.includes('Non caricati') && /reserved for the built-in templates/.test(t), 'a user template with a built-in name is rejected, with the reason')
  ok(!(await p.evaluate(() => [...document.querySelectorAll('#template li h3')].some((h) => h.innerText.trim() === 'Falso studio'))), 'and it is not among the usable ones')
  fs.rmSync(path.join(userDir, 'studio'), { recursive: true })
  // the chosen template disappears from disk: no blank screen, the broken choice is removed and the default is restored
  await p.goto(origin + '/?template=ciao', { waitUntil: 'networkidle0' }); await wait(1000)
  ok((await key()) === 'ciao', 'chose the user template')
  fs.renameSync(path.join(userDir, 'ciao'), path.join(userDir, 'ciao.off')); await api('POST', '/templates/rescan')
  await p.goto(origin + '/', { waitUntil: 'networkidle0' }); await wait(1200)
  t = await text(); ok(/Template not found/.test(t) && t.includes('Back to the default template'), 'template gone: help page, not a blank screen')
  ok((await key()) === null && t.includes('I removed this template from your choice'), 'the broken choice was removed')
  await (await p.$('a')).click(); await wait(1500); ok((await where()).startsWith('/t/console/'), 'the link leads to the default')
  await p.goto(origin + '/', { waitUntil: 'networkidle0' }); await wait(800); ok((await where()).startsWith('/t/console/'), 'next time the default opens straight away')
  fs.renameSync(path.join(userDir, 'ciao.off'), path.join(userDir, 'ciao')); await api('POST', '/templates/rescan')   // everything as before
}
ok(errs.length === 0, 'no console errors or CSP violations ' + JSON.stringify(errs))
await b.close()
console.log(fails.length ? `\n${fails.length} FAILED` : '\nall ok'); process.exit(fails.length ? 1 : 0)
