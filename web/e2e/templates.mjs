// Prova nel browser vero della scelta del template: pagina d'ingresso, elenco nelle Preferenze, scelta ricordata nel browser, avviso per i template non di serie,
// template non validi con il motivo, indirizzo di emergenza. Contro un demone ACCESO e OFFLINE, con i template di serie e due template dell'utente
// (uno buono, uno senza pagina d'ingresso):
//   mkdir -p H/.emuled/templates/mio H/.emuled/templates/rotto && cp -r ../docs/examples/template-ciao/. H/.emuled/templates/mio/ (con id: mio) ...
//   export E2E_USER_TEMPLATES=H/.emuled/templates        (le prove dei casi di errore spostano e creano cartelle li' dentro, e rimettono tutto com'era)
//   HOME=H emuled --run --fake-profile --profile H/config --offline --api-port 18766 --templates-dir dist/templates &
//   EMULED_URL=http://127.0.0.1:18766/ EMULED_TOKEN_FILE=H/config/emuled.token npm run e2e:templates
import puppeteer from 'puppeteer-core'
import fs from 'node:fs'
import path from 'node:path'

const origin = new URL(process.env.EMULED_URL ?? 'http://127.0.0.1:8765/').origin
const token = (process.env.EMULED_TOKEN ?? (process.env.EMULED_TOKEN_FILE ? fs.readFileSync(process.env.EMULED_TOKEN_FILE, 'utf8') : '')).trim()
if (!token) { console.error('Serve EMULED_TOKEN o EMULED_TOKEN_FILE'); process.exit(2) }
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
// il pulsante "Usa questo" della scheda che ha quel nome
const useBtn = async (name) => (await p.evaluateHandle((n) => [...document.querySelectorAll('#template li')].find((li) => li.querySelector('h3')?.innerText.trim() === n)?.querySelector('button'), name)).asElement()

// 1. senza nessuna scelta la pagina d'ingresso porta a console
await p.goto(origin + '/', { waitUntil: 'networkidle0' })
await p.evaluate((t) => { localStorage.setItem('emuled.token', t); localStorage.setItem('emuled.lang', 'it') }, token)
await p.goto(origin + '/', { waitUntil: 'networkidle0' }); await wait(800)
ok((await where()).startsWith('/t/console/'), 'senza scelta: /t/console/')
ok((await key()) === null, 'senza scelta: niente chiave nel browser')

// 2. Preferenze: la sezione Aspetto con i template
await p.goto(origin + '/t/console/#/prefs', { waitUntil: 'networkidle0' }); await wait(1200)
let t = await text()
for (const n of ['Console', 'Cruscotto', 'Studio', 'Mobile first', 'Ciao mondo']) ok(t.includes(n), `elenco: ${n}`)
ok(await p.evaluate(() => [...document.querySelectorAll('#template li')].find((li) => li.querySelector('h3')?.innerText.trim() === 'Console')?.innerText.includes('In uso')), 'Console e\' "In uso"')
ok(await p.evaluate(() => document.querySelectorAll('#template img').length >= 4), 'le anteprime dei template di serie si vedono (immagini caricate dal demone)')
ok(await p.evaluate(() => [...document.querySelectorAll('#template img')].every((i) => !i.complete || i.naturalWidth > 0)), 'le anteprime non sono rotte')
ok(t.includes('Non caricati') && t.includes('rotto') && /pagina d.ingresso/.test(t), 'il template rotto e\' tra i non caricati, col motivo')
ok(await p.evaluate(() => [...document.querySelectorAll('#template li')].find((li) => li.querySelector('h3')?.innerText.trim() === 'Ciao mondo')?.innerText.includes('Tuo')), 'il template dell\'utente e\' marcato "Tuo"')

// 3. scegliere Cruscotto: si apre /t/dashboard/ sulle stesse Preferenze e la scelta resta
await (await useBtn('Cruscotto')).click(); await wait(2000)
ok((await where()) === '/t/dashboard/#/prefs', 'scelto Cruscotto: /t/dashboard/#/prefs')
ok((await key()) === 'dashboard', 'la scelta e\' nel browser')
t = await text(); ok(await p.evaluate(() => [...document.querySelectorAll('#template li')].find((li) => li.querySelector('h3')?.innerText.trim() === 'Cruscotto')?.innerText.includes('In uso')), 'ora Cruscotto e\' "In uso"')
await p.goto(origin + '/', { waitUntil: 'networkidle0' }); await wait(800)
ok((await where()).startsWith('/t/dashboard/'), 'riaprendo "/" si va al template scelto')

// 4. un template dell'utente chiede conferma; annullando non cambia niente
await p.goto(origin + '/t/dashboard/#/prefs', { waitUntil: 'networkidle0' }); await wait(1200)
await (await useBtn('Ciao mondo')).click(); await wait(500)
t = await text(); ok(t.includes('Usare questo template?') && t.includes('di cui ti fidi'), 'avviso per un template che non e\' di serie')
await (await btn('Annulla', 'dialog[open]')).click(); await wait(500)
ok((await where()).startsWith('/t/dashboard/') && (await key()) === 'dashboard', 'annullando resta tutto com\'era')
await (await useBtn('Ciao mondo')).click(); await wait(500)
await (await btn('Usa questo', 'dialog[open]')).click(); await wait(1800)
ok((await where()).startsWith('/t/ciao/') && (await key()) === 'ciao', 'confermando si va al template dell\'utente')

// 5. l'indirizzo di emergenza
await p.goto(origin + '/?template=console', { waitUntil: 'networkidle0' }); await wait(1000)
ok((await where()).startsWith('/t/console/'), 'emergenza: /?template=console riporta a console')

// 6. tornare al predefinito dalle Preferenze toglie la scelta
await p.goto(origin + '/t/console/#/prefs', { waitUntil: 'networkidle0' }); await wait(1200)
await (await useBtn('Studio')).click(); await wait(1800); ok((await key()) === 'studio', 'scelto Studio')
await p.goto(origin + '/t/studio/#/prefs', { waitUntil: 'networkidle0' }); await wait(1200)
await (await useBtn('Console')).click(); await wait(1800)
ok((await where()).startsWith('/t/console/') && (await key()) === null, 'scegliere Console toglie la scelta dal browser')
// 7. il template d'esempio dell'utente funziona davvero (usa lo stesso token del browser, senza chiederlo di nuovo)
await p.goto(origin + '/?template=ciao', { waitUntil: 'networkidle0' }); await wait(2000)
t = await text(); ok((await where()).startsWith('/t/ciao/') && t.includes('Ciao mondo') && /Scarico a \d+ KB\/s/.test(t) && !t.includes('Incolla il token'), 'il template dell\'utente mostra lo stato dal vivo senza richiedere il token')
// 8. casi di errore (solo se si sa dove sono i template dell'utente)
if (userDir) {
  // un nome di serie dalla cartella dell'utente e' rifiutato
  fs.mkdirSync(path.join(userDir, 'studio'), { recursive: true }); fs.writeFileSync(path.join(userDir, 'studio', 'template.yaml'), 'id: studio\nname: Falso studio\nversion: 1.0.0\n'); fs.writeFileSync(path.join(userDir, 'studio', 'index.html'), 'x')
  await api('POST', '/templates/rescan')
  await p.goto(origin + '/t/console/#/prefs', { waitUntil: 'networkidle0' }); await p.reload({ waitUntil: 'networkidle0' }); await wait(1500)
  t = await text(); ok(t.includes('Non caricati') && /riservato ai template di serie/.test(t), 'un template dell\'utente con un nome di serie e\' rifiutato col motivo')
  ok(!(await p.evaluate(() => [...document.querySelectorAll('#template li h3')].some((h) => h.innerText.trim() === 'Falso studio'))), 'e non compare fra quelli che si possono usare')
  fs.rmSync(path.join(userDir, 'studio'), { recursive: true })
  // il template scelto sparisce dal disco: niente schermo vuoto, la scelta rotta si toglie e si torna al predefinito
  await p.goto(origin + '/?template=ciao', { waitUntil: 'networkidle0' }); await wait(1000)
  ok((await key()) === 'ciao', 'scelto il template dell\'utente')
  fs.renameSync(path.join(userDir, 'ciao'), path.join(userDir, 'ciao.off')); await api('POST', '/templates/rescan')
  await p.goto(origin + '/', { waitUntil: 'networkidle0' }); await wait(1200)
  t = await text(); ok(/Template non trovato/.test(t) && t.includes('Torna al template predefinito'), 'template sparito: pagina d\'aiuto, non uno schermo vuoto')
  ok((await key()) === null && t.includes('Ho tolto questo template dalla tua scelta'), 'la scelta rotta e\' stata tolta')
  await (await p.$('a')).click(); await wait(1500); ok((await where()).startsWith('/t/console/'), 'il link porta al predefinito')
  await p.goto(origin + '/', { waitUntil: 'networkidle0' }); await wait(800); ok((await where()).startsWith('/t/console/'), 'la volta dopo si apre subito il predefinito')
  fs.renameSync(path.join(userDir, 'ciao.off'), path.join(userDir, 'ciao')); await api('POST', '/templates/rescan')   // tutto come prima
}
ok(errs.length === 0, 'nessun errore ne\' violazione della CSP in console ' + JSON.stringify(errs))
await b.close()
console.log(fails.length ? `\n${fails.length} FALLITI` : '\ntutto ok'); process.exit(fails.length ? 1 : 0)
