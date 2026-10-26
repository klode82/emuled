// Azioni sui download comuni ai modelli B, C e D (il modello A ha le sue, con la selezione multipla).
import { useI18n } from 'vue-i18n'
import { useDownloads } from '@/stores/downloads'
import { useUi } from '@/stores/ui'
import { bytes, percent } from '@/lib/format'
import { statusInfo } from '@/lib/labels'
import type { Download } from '@/types'

export function useTransferActions() {
  const { t } = useI18n()
  const dl = useDownloads(), ui = useUi()
  // annulla: per i completati basta toglierli dalla lista (non cancella il file), per gli altri si chiede conferma perche' il .part viene cancellato
  async function cancel(items: Download[]) {
    if (!items.length) return
    const hashes = items.map((d) => d.hash)
    if (items.every((d) => statusInfo(d).key === 'done')) { await dl.remove(hashes); return }
    const ok = await ui.ask({ title: t('transfers.cancelTitle', items.length), text: t('transfers.cancelText'), confirm: t('common.cancelDownload'), danger: true,
      list: items.map((d) => ({ label: d.name, info: `${bytes(d.size, 2, true)} · ${percent(d.percent)}` })) })
    if (ok) await dl.remove(hashes)
  }
  const paused = (d: Download) => statusInfo(d).key === 'paused'
  const toggle = (d: Download) => (paused(d) ? dl.resume([d.hash]) : dl.pause([d.hash]))
  return { cancel, toggle, paused }
}
