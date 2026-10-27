// Download actions common to models B, C and D (model A has its own, with multiple selection).
import { useI18n } from 'vue-i18n'
import { useDownloads } from '@/stores/downloads'
import { useUi } from '@/stores/ui'
import { bytes, percent } from '@/lib/format'
import { statusInfo } from '@/lib/labels'
import type { Download } from '@/types'

export function useTransferActions() {
  const { t } = useI18n()
  const dl = useDownloads(), ui = useUi()
  // cancel: for completed ones just remove them from the list (does not delete the file), for the others ask for confirmation because the .part is deleted
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
