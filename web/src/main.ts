import { createApp } from 'vue'
import { createPinia } from 'pinia'
import App from './App.vue'
import { router } from './router'
import { installChunkReload } from './lib/chunkReload'
import { i18n } from './i18n'
import './style.css'

installChunkReload(router)
createApp(App).use(createPinia()).use(router).use(i18n).mount('#app')
