// Template "Console": punto d'ingresso. Caratteri: sono di questo template (e finiscono nella sua cartella). Tutto cio' che e' comune (dati, schermate, componenti, traduzioni) sta nel kit (src/); qui solo il guscio e la schermata Trasferimenti.
import { createTemplateApp } from '@/template'
import './tokens.css'
import '@fontsource/ibm-plex-sans/400.css'
import '@fontsource/ibm-plex-sans/500.css'
import '@fontsource/ibm-plex-sans/600.css'
import '@fontsource/ibm-plex-mono/400.css'
import Shell from './Shell.vue'
import TransfersView from './TransfersView.vue'

createTemplateApp({ shell: Shell, transfers: TransfersView })
