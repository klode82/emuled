// Prova nel browser vero contro un demone ACCESO e OFFLINE (meglio una copia del profilo: aggiunge e cancella download, cambia il nome utente, crea categorie).
//   emuled --run --fake-profile --profile COPIA --offline --api-port 18765 --templates-dir dist/templates &
//   EMULED_URL=http://127.0.0.1:18765/t/console/ EMULED_TOKEN_FILE=COPIA/emuled.token npm run e2e     (l'indirizzo del template da provare; l'API e' sempre alla radice)
// Serve un profilo con qualche server (es. copia di server.met): le prove sui server lo richiedono.
// Variabili: EMULED_URL, EMULED_TOKEN | EMULED_TOKEN_FILE, CHROME (predefinito /usr/bin/google-chrome), E2E_SHOTS (cartella dove salvare le schermate)
import puppeteer from 'puppeteer-core'
import fs from 'node:fs'

const base = process.env.EMULED_URL ?? 'http://127.0.0.1:8765/t/console/'
const token = (process.env.EMULED_TOKEN ?? (process.env.EMULED_TOKEN_FILE ? fs.readFileSync(process.env.EMULED_TOKEN_FILE, 'utf8') : '')).trim()
if (!token) { console.error('Serve EMULED_TOKEN o EMULED_TOKEN_FILE'); process.exit(2) }
const shots = process.env.E2E_SHOTS
if (shots) fs.mkdirSync(shots, { recursive: true })
const wait = (ms) => new Promise((r) => setTimeout(r, ms))
const fails = []
const ok = (c, m) => { if (!c) { fails.push(m); console.log('FAIL', m) } else console.log('ok  ', m) }

const b = await puppeteer.launch({ executablePath: process.env.CHROME ?? '/usr/bin/google-chrome', args: ['--no-sandbox'], headless: 'new' })
const p = await b.newPage(); const errs = []
p.on('console', (m) => { if (m.type() === 'error') errs.push(m.text()) }); p.on('pageerror', (e) => errs.push('pageerror: ' + e.message))
const shot = (n) => (shots ? p.screenshot({ path: `${shots}/${n}.png` }) : null)
const text = () => p.evaluate(() => document.body.innerText)
const btn = async (label) => (await p.evaluateHandle((l) => [...document.querySelectorAll('button')].find((x) => x.innerText.trim() === l && x.getClientRects().length > 0), label)).asElement()
const uniq = (n) => Array.from({ length: n }, () => '0123456789ABCDEF'[Math.floor(Math.random() * 16)]).join('')

await p.setViewport({ width: 1280, height: 800 })
// accesso: token sbagliato rifiutato, token giusto accettato
await p.goto(base, { waitUntil: 'networkidle0' })
await p.evaluate(() => localStorage.setItem('emuled.lang', 'it')); await p.reload({ waitUntil: 'networkidle0' })
await p.type('input[type=password]', 'sbagliato'); await p.keyboard.press('Enter'); await wait(600)
ok((await text()).includes('Token non valido'), 'token sbagliato: messaggio')
await p.click('input[type=password]', { clickCount: 3 }); await p.type('input[type=password]', token); await p.keyboard.press('Enter')
await p.waitForSelector('aside[aria-label=Sezioni]', { timeout: 8000 }); await wait(1500)
ok((await text()).includes('Trasferimenti'), 'accesso con il token')
await shot('transfers')

// blocchi e fonti nel dettaglio: un download da 20 000 000 byte ha 3 blocchi
const api = (m, path, body) => fetch(new URL('/api/v1' + path, base).href, { method: m, headers: { Authorization: 'Bearer ' + token, 'Content-Type': 'application/json' }, body: body ? JSON.stringify(body) : undefined })
const bhash = uniq(32), bname = 'blocchi-' + uniq(4) + '.bin'
ok((await api('POST', '/downloads', { hash: bhash, name: bname, size: 20000000 })).status === 201, 'download da 20 MB aggiunto via API'); await wait(1500)
await p.evaluate((n) => [...document.querySelectorAll('ul[role=list] li')].find((l) => l.innerText.includes(n)).click(), bname); await wait(3800)
const tabBtn = (label) => p.evaluate((l) => [...document.querySelectorAll('[role=tab]')].find((x) => x.innerText.trim().startsWith(l)).click(), label)
ok(/Hash AICH/.test(await text()) && /Trasferimenti/.test(await text()) && /Corrotti/.test(await text()) && /Recuperate/.test(await text()) && !/mappa dei blocchi/i.test(await text()), 'dettaglio: tab Generale con i riquadri come eMule (senza la mappa dei blocchi)'); await shot('detail_general')
await tabBtn('Avanzamento'); await wait(400); ok(/mappa dei blocchi/i.test(await text()) && !/Hash AICH/.test(await text()), 'dettaglio: tab Avanzamento con barra e mappa dei blocchi'); await shot('detail_blocks')
ok(await p.evaluate(() => document.querySelectorAll('figure [title^="Blocco"]').length === 3), 'la mappa ha 3 blocchi')
await tabBtn('Fonti'); await wait(300); ok((await text()).includes('Nessuna fonte per ora'), 'dettaglio: tab Fonti')
await tabBtn('Contenuto'); await wait(2500); ok(/non riconosce|non è installato|Matroska|Non sono riuscito/.test(await text()), 'dettaglio: tab Contenuto risponde (mediainfo, o il messaggio per spiegare perche no)')
await api('DELETE', '/downloads/' + bhash); await wait(1200)
await p.keyboard.press('Escape')

// aggiunta da dialogo, dettaglio, pausa e ripresa
const name = 'prova-e2e-' + uniq(6) + '.bin'
await (await btn('Aggiungi')).click(); await wait(300)
await p.waitForSelector('dialog[open] textarea'); await p.type('dialog[open] textarea', `ed2k://|file|${name}|123456|${uniq(32)}|/`); await wait(200)
await p.evaluate(() => [...document.querySelectorAll('dialog[open] button')].find((x) => x.innerText.trim() === 'Aggiungi').click()); await wait(1000)
ok((await text()).includes(name), 'download aggiunto dal dialogo')
await p.evaluate((n) => [...document.querySelectorAll('ul[role=list] li')].find((l) => l.innerText.includes(n)).click(), name); await wait(400)
await tabBtn('Indirizzo'); await wait(300)
ok(await p.evaluate(() => (document.querySelector('textarea[readonly]')?.value ?? '').startsWith('ed2k://|file|')), 'dettaglio: tab Indirizzo eD2K con il link'); await shot('detail')
await (await btn('Pausa')).click(); await wait(800); ok(!!(await btn('Riprendi')), 'pausa: il dettaglio offre Riprendi')
ok(await p.evaluate((n) => { const r = [...document.querySelectorAll('ul[role=list] li')].find((l) => l.innerText.includes(n)); return r.querySelector('[role=img][aria-label="In pausa"]') !== null && r.querySelector('[role=img]').className.includes('bg-fg') }, name), "pausa: la riga mostra l'icona di stato «In pausa» (piena, ben visibile)")
await shot('status_icons')
await (await btn('Riprendi')).click(); await wait(800); ok(!!(await btn('Pausa')), 'ripresa: il dettaglio offre di nuovo Pausa')
ok(await p.evaluate((n) => { const r = [...document.querySelectorAll('ul[role=list] li')].find((l) => l.innerText.includes(n)); return r.querySelector('[role=img]').getAttribute('aria-label').startsWith('In attesa') }, name), "ripresa: l'icona torna a «In attesa…»")
await (await btn('Annulla download')).click(); await wait(400); ok((await text()).includes('Annullare il download?'), 'annulla chiede conferma'); await shot('confirm')
ok(await p.evaluate((n) => { const d = document.querySelector('dialog[open]'); return !!d && d.querySelector('ul') !== null && d.querySelector('ul').innerText.includes(n) }, name), 'conferma: elenca i file che si cancellano')
await p.evaluate(() => [...document.querySelectorAll('dialog[open] button')].find((x) => x.innerText.trim() === 'Annulla download').click()); await wait(1000)
ok(!(await text()).includes(name), 'download annullato e tolto dalla lista')

// ricerca con risultati finti (richieste intercettate: niente rete): nome su due righe, ordinamento delle colonne
await p.setRequestInterception(true)
const fakeResults = [
  { hash: 'A'.repeat(32), name: 'Un nome di file molto lungo che deve avere spazio per andare a capo su due righe senza essere tagliato subito - parte seconda del titolo.pdf', type: 'Doc', size: 7743600, sources: 12, complete_sources: 3, answers: 2 },
  { hash: 'B'.repeat(32), name: 'beta.iso', type: 'Iso', size: 5900000000, sources: 340, complete_sources: 40, answers: 5 },
  { hash: 'C'.repeat(32), name: 'Alfa.mkv', type: 'Video', size: 1400000000, sources: 3, complete_sources: 0, answers: 1 },
]
p.on('request', (r) => {
  const u = r.url()
  if (/\/api\/v1\/searches$/.test(u) && r.method() === 'GET') r.respond({ status: 200, contentType: 'application/json', body: '[]' })
  else if (/\/api\/v1\/searches$/.test(u) && r.method() === 'POST') r.respond({ status: 202, contentType: 'application/json', body: JSON.stringify({ id: 99, query: 'prova', scope: 'server', state: 'running', elapsed_s: 0, result_count: 0 }) })
  else if (/\/api\/v1\/searches\/99$/.test(u)) r.respond({ status: 200, contentType: 'application/json', body: JSON.stringify({ id: 99, query: 'prova', scope: 'server', state: 'finished', elapsed_s: 3, result_count: 3, results: fakeResults }) })
  else r.continue()
})
await p.goto(base + '#/search'); await wait(600)
await p.type('input[placeholder^="Cosa cerchi"]', 'prova'); await p.keyboard.press('Enter'); await wait(1800)
const firstNames = async () => p.$$eval('tbody tr td:nth-child(2) p', (a) => a.map((x) => x.textContent.trim().slice(0, 8)))
ok((await firstNames()).join() === 'beta.iso,Un nome ,Alfa.mkv', 'ricerca: ordine iniziale per fonti decrescente'); await shot('search_results')
ok(await p.evaluate(() => { const el = [...document.querySelectorAll('tbody tr td:nth-child(2) p')].find((x) => x.textContent.startsWith('Un nome')); return el.getBoundingClientRect().height > 20 && getComputedStyle(el).webkitLineClamp === '2' }), 'ricerca: il nome lungo va a capo su due righe')
const sortBy = async (label) => p.evaluate((l) => [...document.querySelectorAll('thead button')].find((b) => b.innerText.trim().toLowerCase() === l.toLowerCase()).click(), label)
const pcts = async () => p.$$eval('tbody tr td:nth-child(5)', (a) => a.map((x) => x.textContent.trim()))
ok((await pcts()).join() === '12%,25%,0%', 'ricerca: percentuale di fonti complete (40 su 340 = 12%, 3 su 12 = 25%, 0 su 3 = 0%)')
await sortBy('Complete'); await wait(200); ok((await firstNames()).join() === 'Un nome ,beta.iso,Alfa.mkv', 'ricerca: ordine per fonti complete (percentuale)')
await sortBy('Nome'); await wait(200); ok((await firstNames()).join() === 'Alfa.mkv,beta.iso,Un nome ', 'ricerca: ordine per nome crescente (senza distinguere maiuscole)')
await sortBy('Nome'); await wait(200); ok((await firstNames()).join() === 'Un nome ,beta.iso,Alfa.mkv', 'ricerca: secondo clic = decrescente')
await sortBy('Dimensione'); await wait(200); ok((await firstNames()).join() === 'beta.iso,Alfa.mkv,Un nome ', 'ricerca: per dimensione, il piu grande in alto')
ok(await p.evaluate(() => document.querySelector('th[aria-sort="descending"]') !== null), 'ricerca: aria-sort sulla colonna ordinata')
ok(await p.evaluate(() => parseFloat(getComputedStyle(document.querySelector('tbody td:nth-child(2) p')).fontSize) === 12), 'ricerca: testi a 12 px')
ok(await p.evaluate(() => [...document.querySelectorAll('[role=tab]')].some((x) => x.innerText.includes('prova'))), 'ricerca: la ricerca resta come scheda')
// scelta multipla: due risultati scelti, un solo clic li scarica (i download veri partono sul demone offline di prova)
{
  const boxes = await p.$$('tbody tr td:first-child input[type=checkbox]'); ok(boxes.length === 3, 'ricerca: una casella per ogni risultato')
  await boxes[0].click(); await boxes[1].click(); await wait(200)
  ok((await text()).includes('2 file scelti'), 'ricerca: conta i file scelti')
  await p.evaluate(() => [...document.querySelectorAll('button')].find((b) => /^Scarica i 2 file scelti$/.test(b.innerText.trim())).click()); await wait(1500)
  ok((await p.$$eval('tbody tr', (a) => a.filter((r) => /Già in download/.test(r.innerText)).length)) === 2, 'ricerca: i due file scelti sono partiti insieme')
}
p.removeAllListeners('request'); await p.setRequestInterception(false)

// transfers: menu "Opzioni categorie": crea una categoria da li' e ci sposta il file scelto
{
  const api = (m, path, body) => p.evaluate(async (m, path, body, t) => (await fetch('/api/v1' + path, { method: m, headers: { Authorization: 'Bearer ' + t, 'Content-Type': 'application/json' }, body: body ? JSON.stringify(body) : undefined })).json().catch(() => ({})), m, path, body, token)
  await p.goto(base + '#/transfers'); await wait(1200)
  await p.$eval('li input[type=checkbox]', (el) => el.click()); await wait(200)
  await p.evaluate(() => [...document.querySelectorAll('button')].find((b) => b.innerText.trim() === 'Opzioni categorie').click()); await wait(200)
  ok(await p.evaluate(() => document.querySelector('[role=menu]') !== null && document.querySelector('[role=menu]').innerText.includes('Nessuna categoria')), 'categorie: il menu si apre')
  await p.evaluate(() => [...document.querySelectorAll('[role=menuitem]')].find((b) => b.innerText.trim() === 'Nuova categoria…' && !b.disabled).click()); await wait(300)
  const cname = 'E2E ' + uniq(4)
  await p.type('dialog[open] input[required]', cname); await p.evaluate(() => document.querySelector('dialog[open] button[form="cat-form"]').click()); await wait(1500)
  const cats = await api('GET', '/categories'); const made = cats.find((c) => c.title === cname)
  ok(!!made, 'categorie: la nuova categoria e stata creata dal menu')
  const dls = await api('GET', '/downloads'); ok(dls.some((d) => made && d.category === made.id), 'categorie: il file scelto e stato spostato nella nuova categoria')
  if (made) { for (const d of dls) if (d.category === made.id) await api('PATCH', '/downloads/' + d.hash, { category: 0 }); await api('DELETE', '/categories/' + made.id) }
  for (const d of dls) if (/^[BC]{32}$/.test(d.hash)) await api('DELETE', '/downloads/' + d.hash)
}


// server: ordinamento per nome e per utenti
await p.goto(base + '#/servers'); await wait(1200)
const srvNames = async () => p.$$eval('tbody tr td:first-child p:first-child', (a) => a.map((x) => x.textContent.trim().toLowerCase()))
await p.evaluate(() => [...document.querySelectorAll('thead button')].find((b) => /^nome$/i.test(b.innerText.trim())).click()); await wait(200)
{ const n = await srvNames(); ok(n.length > 1 && n.join('|') === [...n].sort((x, y) => x.localeCompare(y, undefined, { numeric: true, sensitivity: 'base' })).join('|'), 'server: ordinati per nome') }
await p.evaluate(() => [...document.querySelectorAll('thead button')].find((b) => /^utenti$/i.test(b.innerText.trim())).click()); await wait(200)
{ const u = await p.$$eval('tbody tr td:nth-child(3)', (a) => a.map((x) => parseInt(x.textContent.replace(/\D/g, '') || '0', 10))); ok(u.length > 1 && u.every((v, i) => i === 0 || u[i - 1] >= v), 'server: per utenti, il piu grande in alto') }

// ricerca offline: messaggio comprensibile
await p.goto(base + '#/search'); await wait(500)
await p.type('input[placeholder^="Cosa cerchi"]', 'ubuntu'); await p.keyboard.press('Enter'); await wait(800)
ok(/non connesso|Non sei collegato/i.test(await text()), 'ricerca senza server: messaggio'); await shot('search')

// preferenze: salvataggio, avviso di identita' temporanea
await p.goto(base + '#/prefs'); await wait(800)
const nick = 'N' + Date.now()
await p.$eval('#p-nick', (e, v) => { e.value = v; e.dispatchEvent(new Event('input')) }, nick); await wait(300); await shot('prefs')
await (await btn('Salva')).click(); await wait(800)
const pr = await (await fetch(new URL('/api/v1/preferences', base).href, { headers: { Authorization: 'Bearer ' + token } })).json()
ok(pr.nick === nick, 'preferenza salvata nel demone')

// server: aggiungi, cambia priorita', togli
await p.goto(base + '#/servers'); await wait(800)
const srvName = 'ProvaE2E' + Date.now() % 10000, srvAddr = '203.0.113.' + (100 + Date.now() % 100) + ':4661'
await (await btn('Aggiungi server')).click(); await wait(300)
await p.waitForSelector('dialog[open] input[placeholder="176.123.5.89:4725"]'); await p.type('dialog[open] input[placeholder="176.123.5.89:4725"]', srvAddr); await p.type('dialog[open] input[placeholder="Facoltativo"]', srvName)
await p.evaluate(() => [...document.querySelectorAll('dialog[open] button')].find((x) => x.innerText.trim() === 'Aggiungi').click()); await wait(1000)
ok((await text()).includes(srvName), 'server aggiunto'); await shot('servers')
await p.evaluate((n) => { const r = [...document.querySelectorAll('tbody tr')].find((x) => x.innerText.includes(n)); const sel = r.querySelector('select'); sel.value = 'high'; sel.dispatchEvent(new Event('change')) }, srvName); await wait(800)
ok(((await (await api('GET', '/servers')).json()).find((x) => x.name === srvName) ?? {}).priority === 'high', 'priorita del server cambiata')
await p.evaluate((n) => [...document.querySelectorAll('tbody tr')].find((x) => x.innerText.includes(n)).querySelector('button[aria-label^="Togli"]').click(), srvName); await wait(400)
await p.evaluate(() => [...document.querySelectorAll('dialog[open] button')].find((x) => x.innerText.trim() === 'Togli').click()); await wait(1000)
ok(!(await text()).includes(srvName), 'server tolto')

// cartelle: errore del demone mostrato, poi cambio valido
await p.goto(base + '#/prefs'); await wait(800)
const tmpDir = '/tmp/e2e-arrivo-' + Date.now()
await p.$eval('#dirs input', (e, v) => { e.value = v; e.dispatchEvent(new Event('input')) }, 'relativo/dir'); await wait(200)
await (await btn('Applica le cartelle')).click(); await wait(800); ok(/assoluto/.test(await text()), 'cartelle: il demone rifiuta un percorso relativo')
await p.$eval('#dirs input', (e, v) => { e.value = v; e.dispatchEvent(new Event('input')) }, tmpDir); await wait(200)
await (await btn('Applica le cartelle')).click(); await wait(1000)
ok((await (await api('GET', '/directories')).json()).incoming === tmpDir + '/', 'cartella dei completati cambiata'); await shot('dirs')

// categorie
await (await btn('Nuova')).click(); await wait(300); await p.type('form input[required]', 'Cat' + Date.now()); await (await btn('Salva categoria')).click(); await wait(800)
ok((await text()).includes('Categoria creata'), 'categoria creata')

// pagina rimasta aperta dopo un aggiornamento: il file della schermata Kad non esiste piu' (404) -> la pagina si ricarica da sola e la schermata si apre
{
  await p.goto(base + '#/transfers'); await p.reload({ waitUntil: 'networkidle0' }); await wait(600)
  await p.setRequestInterception(true); let blocked = 0
  p.on('request', (r) => { if (/\/assets\/KadView-[^/]*\.js/.test(r.url()) && blocked < 1) { blocked++; r.respond({ status: 404, body: 'non trovato' }) } else r.continue() })
  await p.evaluate(() => { location.hash = '#/kad' }); await wait(2500)
  ok(blocked === 1 && /Kad/.test(await p.evaluate(() => document.querySelector('h1')?.textContent ?? '')), 'dopo un aggiornamento la schermata Kad si apre da sola (ricarica automatica)')
  p.removeAllListeners('request'); await p.setRequestInterception(false)
}

// registro: la riga d'avvio, le azioni fatte (aggiunta e annullo del download), filtro, comparsa in diretta, svuotamento
await p.goto(base + '#/log'); await wait(1200)
await shot('log')	// (la riga d'avvio e' verificata dal test C++ daemon_api: qui il demone puo' essere stato usato gia' da un'altra prova)
ok(/download added: /.test(await text()) && /download cancelled/.test(await text()), "registro: le azioni fatte dall'interfaccia")
await p.type('input[type=search]', 'download added'); await wait(300)
ok(/download added/.test(await text()) && !/download cancelled/.test(await text()), 'registro: filtro per testo')
await p.$eval('input[type=search]', (e) => { e.value = ''; e.dispatchEvent(new Event('input')) }); await wait(200)
const lname = 'log-live-' + uniq(4) + '.bin'
await api('POST', '/downloads', { hash: uniq(32), name: lname, size: 1000 }); await wait(2500)
ok((await text()).includes('download added: ' + lname), 'registro: la riga nuova compare da sola (WebSocket)')
await p.evaluate(() => document.querySelector('button[aria-label="Svuota il registro"]').click()); await wait(400)
await p.evaluate(() => [...document.querySelectorAll('dialog[open] button')].find((x) => x.innerText.trim() === 'Svuota il registro').click()); await wait(1000)
ok(/registro è vuoto/i.test(await text()), 'registro svuotato')

// nessun doppio scroll: la pagina non scorre mai da sola, con tante righe di avviso nel registro e la finestra bassa
await p.setViewport({ width: 1280, height: 600 })
await p.setRequestInterception(true)
const manyWarnings = Array.from({ length: 120 }, (_, i) => ({ id: i + 1, time: 1700000000000 + i * 1000, level: i % 3 === 0 ? 'error' : 'warning', source: i % 5 === 0 ? 'server' : 'general', text: 'Riga di prova ' + i + ' con un testo abbastanza lungo da occupare spazio nella lista del registro' }))
p.on('request', (r) => { if (/\/api\/v1\/log\?/.test(r.url()) && r.method() === 'GET') r.respond({ status: 200, contentType: 'application/json', body: JSON.stringify({ lines: manyWarnings, last_id: 120, truncated: false }) }); else r.continue() })
for (const route of ['log', 'transfers', 'search', 'servers', 'shared', 'kad', 'stats', 'prefs']) {
  await p.goto(base + '#/' + route); await p.reload({ waitUntil: 'networkidle0' }); await wait(900)
  const m = await p.evaluate(() => ({ doc: document.documentElement.scrollHeight, view: innerHeight }))
  ok(m.doc <= m.view, `nessun scroll della pagina in /${route} (documento ${m.doc} px, finestra ${m.view} px)`)
}
await p.goto(base + '#/log'); await p.reload({ waitUntil: 'networkidle0' }); await wait(900); await shot('log_many')
ok(await p.evaluate(() => { const o = document.querySelector('ol'); return o.scrollHeight > o.clientHeight }), 'registro: scorre la lista (un solo scroll)')
p.removeAllListeners('request'); await p.setRequestInterception(false); await p.setViewport({ width: 1280, height: 800 })

// lingua: inglese
await p.goto(base + '#/prefs'); await wait(800)
await p.select('#lang', 'en'); await wait(400); ok((await text()).includes('Preferences') && (await text()).includes('Folders'), 'lingua inglese'); await shot('english')
await p.select('#lang', 'it'); await wait(300)

// tema scuro
await p.select('#theme', 'dark'); await wait(300)
ok(await p.evaluate(() => document.documentElement.dataset.theme === 'dark'), 'tema scuro applicato'); await shot('dark')
await p.select('#theme', 'system')

// altre schermate senza errori e telefono
for (const n of ['servers', 'shared', 'kad', 'stats']) { await p.goto(base + '#/' + n); await wait(1200); ok((await text()).length > 50, 'schermata ' + n); await shot(n) }
await p.setViewport({ width: 390, height: 780, isMobile: true }); await p.goto(base + '#/transfers'); await wait(1200); await shot('phone')
await (await btn('Altro')).click(); await wait(300); ok((await text()).includes('Statistiche'), 'telefono: menu Altro'); await shot('phone_more')

const bad = errs.filter((e) => !/status of (401|404|409|422)/.test(e))   // errori attesi: token sbagliato, file di schermata bloccato di proposito, doppioni, percorso rifiutato
ok(bad.length === 0, 'nessun errore in console' + (bad.length ? ': ' + bad.join(' | ') : ''))
await b.close()
console.log(fails.length ? `FALLITI ${fails.length}` : 'TUTTO OK'); process.exit(fails.length ? 1 : 0)
