// Il punto d'ingresso comune dei template di serie: monta l'applicazione con il guscio e la schermata Trasferimenti del template.
import { createApp, type Component } from 'vue'
import { createPinia } from 'pinia'
import App from './App.vue'
import { createAppRouter } from './router'
import { installChunkReload } from './lib/chunkReload'
import { i18n } from './i18n'
import './style.css'

export function createTemplateApp(t: { shell: Component; transfers: Component }) {
  const router = createAppRouter(t.transfers)
  installChunkReload(router)
  return createApp(App, { shell: t.shell }).use(createPinia()).use(router).use(i18n).mount('#app')
}
