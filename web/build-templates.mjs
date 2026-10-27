// Builds the built-in templates: one per folder in templates/ (dist/templates/<id>/), each independent (page, scripts, styles and its own template.yaml).
//   node build-templates.mjs            all
//   node build-templates.mjs dashboard  only that one
// The shared kit (src/) is included in each template at build time: at runtime templates share nothing, they only talk to the API.
import { execFileSync } from 'node:child_process'
import { copyFileSync, existsSync, readdirSync, readFileSync, statSync } from 'node:fs'
import { join } from 'node:path'

const root = new URL('.', import.meta.url).pathname
const all = readdirSync(join(root, 'templates'), { withFileTypes: true }).filter((d) => d.isDirectory() && existsSync(join(root, 'templates', d.name, 'template.yaml'))).map((d) => d.name).sort()
const wanted = process.argv.slice(2)
for (const w of wanted) if (!all.includes(w)) { console.error(`template "${w}" not found (available: ${all.join(', ')})`); process.exit(2) }
const ids = wanted.length ? wanted : all
for (const id of ids) {
  const yaml = readFileSync(join(root, 'templates', id, 'template.yaml'), 'utf8')
  if (!new RegExp(`^id:\\s*${id}\\s*$`, 'm').test(yaml)) { console.error(`templates/${id}/template.yaml: the id must be "${id}" (same as the folder)`); process.exit(2) }
  console.log(`== template ${id}`)
  execFileSync(process.execPath, [join(root, 'node_modules/vite/bin/vite.js'), 'build'], { cwd: root, stdio: 'inherit', env: { ...process.env, TEMPLATE: id } })
  const out = join(root, 'dist', 'templates', id)
  copyFileSync(join(root, 'templates', id, 'template.yaml'), join(out, 'template.yaml'))
  const preview = join(root, 'templates', id, 'preview.png')
  if (existsSync(preview)) { if (statSync(preview).size > 2 << 20) console.warn(`templates/${id}/preview.png is over 2 MB: the daemon will not use it`); copyFileSync(preview, join(out, 'preview.png')) }
  else console.warn(`templates/${id}/preview.png is missing: the template will have no preview`)
}
console.log(`\nDone: ${ids.length} template(s) in ${join(root, 'dist', 'templates')}`)
