import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'
import { fileURLToPath, URL } from 'node:url'

// Ogni template di serie si costruisce da solo: TEMPLATE=<id> (predefinito console) -> dist/templates/<id>/. `npm run build` li costruisce tutti (build-templates.mjs).
// In sviluppo (`TEMPLATE=dashboard npm run dev`) le chiamate /api (e il WebSocket) vanno al demone: EMULED_API=http://127.0.0.1:8765
const id = process.env.TEMPLATE ?? 'console'
const api = process.env.EMULED_API ?? 'http://127.0.0.1:8765'
const here = (p: string) => fileURLToPath(new URL(p, import.meta.url))
export default defineConfig({
  root: here(`./templates/${id}`),
  base: './',												// il template sta in /t/<id>/: gli indirizzi dei suoi file sono relativi
  publicDir: here('./public'),								// favicon: finisce nella radice del template
  plugins: [vue()],
  resolve: { alias: { '@': here('./src') } },
  server: { proxy: { '/api': { target: api, ws: true } }, fs: { allow: [here('.')] } },
  build: { outDir: here(`./dist/templates/${id}`), emptyOutDir: true, sourcemap: false },
})
