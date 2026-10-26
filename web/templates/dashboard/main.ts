// Template "Cruscotto": punto d'ingresso. Tutto cio' che e' comune (dati, schermate, componenti, traduzioni) sta nel kit (src/); qui solo il guscio e la schermata Trasferimenti.
import { createTemplateApp } from '@/template'
import './tokens.css'
import Shell from './Shell.vue'
import TransfersView from './TransfersView.vue'

createTemplateApp({ shell: Shell, transfers: TransfersView })
