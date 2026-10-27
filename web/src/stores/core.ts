// General state (ed2k, Kad, speeds) and speed history for the charts
import { defineStore } from 'pinia'
import { ref } from 'vue'
import type { ServerEvent, Status } from '@/types'
import { useSession } from './session'

export interface Sample { t: number; down: number; up: number }
const MAX_SAMPLES = 300

export const useCore = defineStore('core', () => {
  const status = ref<Status | null>(null)
  const samples = ref<Sample[]>([])

  function sample(down: number, up: number) {
    samples.value.push({ t: Date.now(), down, up })
    if (samples.value.length > MAX_SAMPLES) samples.value.splice(0, samples.value.length - MAX_SAMPLES)
  }
  async function refresh() {
    status.value = await useSession().getApi().status()
    sample(status.value.transfers.download_speed, status.value.transfers.upload_speed)
  }
  function apply(e: ServerEvent) {
    const s = status.value
    if (!s) return
    if (e.type === 'stats') { s.transfers = e.data; sample(e.data.download_speed, e.data.upload_speed) }
    else if (e.type === 'server.state') s.ed2k = e.data
    else if (e.type === 'kad.state') s.kad = e.data
  }
  return { status, samples, refresh, apply }
})
