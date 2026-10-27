// First start (Bench 873): after the first account is created the daemon marks "pending" until the Wizard is finished.
import { defineStore } from 'pinia'
import { ref } from 'vue'
import { useSession } from './session'

export const useSetup = defineStore('setup', () => {
  const pending = ref(false)
  const checked = ref(false)
  async function check() {
    try { pending.value = (await useSession().getApi().setupStatus()).pending } catch { pending.value = false }		// a daemon without this feature: no Wizard
    checked.value = true
  }
  async function finish() { await useSession().getApi().setupDone(); pending.value = false }
  function reset() { pending.value = false; checked.value = false }
  return { pending, checked, check, finish, reset }
})
