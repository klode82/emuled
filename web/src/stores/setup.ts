// Primo avvio (Bench 873): dopo la creazione del primo account il demone segna "pending" finche' il Wizard non e' finito.
import { defineStore } from 'pinia'
import { ref } from 'vue'
import { useSession } from './session'

export const useSetup = defineStore('setup', () => {
  const pending = ref(false)
  const checked = ref(false)
  async function check() {
    try { pending.value = (await useSession().getApi().setupStatus()).pending } catch { pending.value = false }		// un demone senza questa funzione: niente Wizard
    checked.value = true
  }
  async function finish() { await useSession().getApi().setupDone(); pending.value = false }
  function reset() { pending.value = false; checked.value = false }
  return { pending, checked, check, finish, reset }
})
