// Costruisce i template di serie: uno per cartella in templates/ (dist/templates/<id>/), ognuno indipendente (pagina, script, stili e il suo template.yaml).
//   node build-templates.mjs            tutti
//   node build-templates.mjs dashboard  solo quello
// Il kit comune (src/) viene incluso in ogni template al momento della costruzione: a runtime i template non condividono niente, parlano solo con l'API.
import { execFileSync } from 'node:child_process'
import { copyFileSync, existsSync, readdirSync, readFileSync, statSync } from 'node:fs'
import { join } from 'node:path'

const root = new URL('.', import.meta.url).pathname
const all = readdirSync(join(root, 'templates'), { withFileTypes: true }).filter((d) => d.isDirectory() && existsSync(join(root, 'templates', d.name, 'template.yaml'))).map((d) => d.name).sort()
const wanted = process.argv.slice(2)
for (const w of wanted) if (!all.includes(w)) { console.error(`template "${w}" non trovato (ci sono: ${all.join(', ')})`); process.exit(2) }
const ids = wanted.length ? wanted : all
for (const id of ids) {
  const yaml = readFileSync(join(root, 'templates', id, 'template.yaml'), 'utf8')
  if (!new RegExp(`^id:\\s*${id}\\s*$`, 'm').test(yaml)) { console.error(`templates/${id}/template.yaml: l'id deve essere "${id}" (come la cartella)`); process.exit(2) }
  console.log(`== template ${id}`)
  execFileSync(process.execPath, [join(root, 'node_modules/vite/bin/vite.js'), 'build'], { cwd: root, stdio: 'inherit', env: { ...process.env, TEMPLATE: id } })
  const out = join(root, 'dist', 'templates', id)
  copyFileSync(join(root, 'templates', id, 'template.yaml'), join(out, 'template.yaml'))
  const preview = join(root, 'templates', id, 'preview.png')
  if (existsSync(preview)) { if (statSync(preview).size > 2 << 20) console.warn(`templates/${id}/preview.png supera i 2 MB: il demone non la usera'`); copyFileSync(preview, join(out, 'preview.png')) }
  else console.warn(`templates/${id}/preview.png manca: il template non avra' anteprima`)
}
console.log(`\nPronto: ${ids.length} template in ${join(root, 'dist', 'templates')}`)
