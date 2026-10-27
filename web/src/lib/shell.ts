// Shared parts of the shells (models A, B, C, D): the "search or paste a link" box and the ed2k and Kad status labels.
import { computed, ref } from 'vue'
import { useI18n } from 'vue-i18n'
import { useRouter } from 'vue-router'
import { useCore } from '@/stores/core'
import { useSearch } from '@/stores/search'
import { useUi } from '@/stores/ui'
import { looksLikeLink } from '@/lib/format'

export function useShellBar() {
  const { t } = useI18n()
  const core = useCore(), ui = useUi(), search = useSearch(), router = useRouter()
  const q = ref('')
  const st = computed(() => core.status)
  const ed2k = computed(() => {
    const e = st.value?.ed2k
    if (!e) return { text: 'ed2k · …', tone: 'text-mute' }
    if (e.connected) return { text: `ed2k · ${e.server?.name || t('shell.connected')} · ${t(e.high_id ? 'shell.highId' : 'shell.lowId')}`, tone: e.high_id ? 'text-accent' : 'text-warn' }
    return { text: `ed2k · ${t(e.connecting ? 'shell.connecting' : 'shell.disconnected')}`, tone: e.connecting ? 'text-warn' : 'text-bad' }
  })
  const kad = computed(() => {
    const k = st.value?.kad
    if (!k?.running) return { text: `Kad · ${t('shell.off')}`, tone: 'text-mute' }
    return k.connected ? { text: `Kad · ${t('shell.connected')}`, tone: 'text-accent' } : { text: `Kad · ${t('shell.connecting')}`, tone: 'text-warn' }
  })
  // enter in the box: an ed2k link opens "Add", other text starts the search
  function go() {
    const x = q.value.trim()
    if (!x) return
    if (looksLikeLink(x)) ui.openAdd(x)
    else { search.form.query = x; void router.push('/search').then(() => search.start()) }
    q.value = ''
  }
  function onPaste(e: ClipboardEvent) {
    const x = e.clipboardData?.getData('text') ?? ''
    if (looksLikeLink(x)) { e.preventDefault(); ui.openAdd(x) }
  }
  return { q, st, ed2k, kad, go, onPaste }
}
