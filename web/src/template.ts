// The common entry point of the bundled templates: mounts the application with the shell and the template's Transfers screen.
import { createApp, type Component } from 'vue'
import { createPinia } from 'pinia'
import App from './App.vue'
import { createAppRouter } from './router'
import { installChunkReload } from './lib/chunkReload'
import { i18n } from './i18n'
import './style.css'

// transfers and uploads are the two screens every template can draw its own way; without `uploads` the common one is used
export function createTemplateApp(t: { shell: Component; transfers: Component; uploads?: Component }) {
  const router = createAppRouter(t.transfers, t.uploads)
  installChunkReload(router)
  return createApp(App, { shell: t.shell }).use(createPinia()).use(router).use(i18n).mount('#app')
}
