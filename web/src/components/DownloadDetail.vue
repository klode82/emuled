<!-- Download detail: progress, block map, sources, category, priority, hash and ed2k link, actions. A completed one shows where the file is. -->
<script setup lang="ts">
import { computed, onMounted, onUnmounted, ref, watch } from 'vue'
import { useI18n } from 'vue-i18n'
import { useDownloads } from '@/stores/downloads'
import { usePrefs } from '@/stores/prefs'
import { useSession } from '@/stores/session'
import { errText, useToasts } from '@/stores/toasts'
import { useUi } from '@/stores/ui'
import { bytes, dateTime, duration, ed2kLink, eta, number, percent, speed } from '@/lib/format'
import { useCore } from '@/stores/core'
import { PRIORITIES, priorityKey, statusInfo, toneClass } from '@/lib/labels'
import type { Blocks, Content, ContentTrack, Details, FileComments, FileNames, Parts, Priority, Source } from '@/types'
import Icon from './Icon.vue'
import ProgressBar from './ProgressBar.vue'
import PartsMap from './PartsMap.vue'
import StatusIcon from './StatusIcon.vue'

const { t } = useI18n()
const props = defineProps<{ hash: string }>()
const emit = defineEmits<{ close: [] }>()
const dl = useDownloads(), prefs = usePrefs(), toasts = useToasts(), ui = useUi(), session = useSession()
const d = computed(() => dl.items[props.hash])
const info = computed(() => (d.value ? statusInfo(d.value) : null))
const paused = computed(() => info.value?.key === 'paused')
const done = computed(() => info.value?.key === 'done')

// blocks and sources: re-read every 3 seconds while the panel is open (the API does not send them as events)
const parts = ref<Parts | null>(null), sources = ref<Source[]>([]), det = ref<Details | null>(null), blocks = ref<Blocks | null>(null)
const activeParts = computed(() => blocks.value?.active.map((a) => ({ part: a.part, receiving: a.receiving !== false })) ?? [])
type TabKey = 'general' | 'progress' | 'sources' | 'names' | 'comments' | 'content' | 'ed2k'
const TABS: readonly TabKey[] = ['general', 'progress', 'sources', 'names', 'comments', 'content', 'ed2k']
const tab = ref<TabKey>('general')
const tabs = computed<readonly TabKey[]>(() => (done.value ? TABS.filter((x) => x !== 'sources' && x !== 'progress' && x !== 'names' && x !== 'comments') : TABS))
watch(tabs, (v) => { if (!v.includes(tab.value)) tab.value = 'general' })

// Content tab: mediainfo reads the file (even the .part, if the header is there); reading starts when the tab is opened
const content = ref<Content | null>(null)
let contentTimer: ReturnType<typeof setTimeout> | undefined
async function loadContent() {
  clearTimeout(contentTimer)
  const api = session.getApi(), h = props.hash
  try {
    let r = await api.contentStart(h)
    for (let i = 0; i < 40 && r.state === 'running' && props.hash === h; i++) { await new Promise((ok) => { contentTimer = setTimeout(ok, 500) }); r = await api.content(h) }
    if (props.hash === h) content.value = r
  } catch (e) { if (props.hash === h) content.value = { state: 'failed', reason: 'api' } }
}
watch([tab, () => props.hash], ([x]) => { if (x === 'content' && !content.value) void loadContent() })
watch(() => props.hash, () => { content.value = null; clearTimeout(contentTimer) })
onUnmounted(() => clearTimeout(contentTimer))
const hms = (s: number) => `${Math.floor(s / 3600)}:${String(Math.floor((s % 3600) / 60)).padStart(2, '0')}:${String(Math.round(s % 60)).padStart(2, '0')}`
const kbps = (b: number | null | undefined) => (b ? `${number(Math.round(b / 1000))} kbps` : '—')
const trackLine = (tr: ContentTrack) => [tr.title, tr.lang].filter(Boolean).join(' · ')
let timer: ReturnType<typeof setInterval> | undefined
async function refresh() {
  if (!d.value || done.value) { parts.value = null; sources.value = []; det.value = null; blocks.value = null; return }
  const api = session.getApi()
  // each piece of data loads on its own: if a call fails (the download just vanished, or the daemon is an old version that does not know it) the others still show
  const h = props.hash
  const [pa, so, de, bl] = await Promise.allSettled([api.parts(h), api.sources(h), api.details(h), api.blocks(h)])
  if (props.hash !== h) return
  if (pa.status === 'fulfilled') parts.value = pa.value
  if (so.status === 'fulfilled') sources.value = so.value
  if (de.status === 'fulfilled') det.value = de.value
  blocks.value = bl.status === 'fulfilled' ? bl.value : null
}
// Name and Comments tabs: data is read only while the tab is open
const names = ref<FileNames | null>(null), comments = ref<FileComments | null>(null)
const newName = ref(''), nameTouched = ref(false), renaming = ref(false)
const filterOpen = ref(false), filterText = ref(''), kadBusy = ref(false)
async function refreshExtra() {
  if (!d.value || done.value) return
  const api = session.getApi(), h = props.hash
  try {
    if (tab.value === 'names') { const r = await api.names(h); if (props.hash === h) { names.value = r; if (!nameTouched.value) newName.value = r.current } }
    else if (tab.value === 'comments') { const r = await api.comments(h); if (props.hash === h) comments.value = r }
  } catch { /* the download may have just vanished */ }
}
watch(tab, () => void refreshExtra())
const canRename = computed(() => !renaming.value && !!newName.value.trim() && newName.value.trim() !== d.value?.name)
async function rename() {
  if (!canRename.value) return
  renaming.value = true
  try { dl.items[props.hash] = await session.getApi().patchDownload(props.hash, { name: newName.value.trim() }); nameTouched.value = false; toasts.ok(t('fd.renamed')); await refreshExtra() } catch (e) { toasts.error(errText(e)) } finally { renaming.value = false }
}
function clearName() { newName.value = ''; nameTouched.value = true }
function pickName(n: string) { newName.value = n; nameTouched.value = true }
async function searchKad() {
  kadBusy.value = true
  try { await session.getApi().searchKadComments(props.hash); toasts.ok(t('fd.kadStarted')); await refreshExtra() } catch (e) { toasts.error(errText(e)) } finally { kadBusy.value = false }
}
function openFilter() { filterText.value = comments.value?.filter ?? ''; filterOpen.value = !filterOpen.value }
async function saveFilter() {
  try { await session.getApi().patchPreferences({ comment_filter: filterText.value }); filterOpen.value = false; toasts.ok(t('fd.filterSaved')); await refreshExtra() } catch (e) { toasts.error(errText(e)) }
}
const stars = (n: number) => (n >= 1 && n <= 5 ? t('fd.rating.' + n) : '—')
watch(() => props.hash, () => { names.value = null; comments.value = null; nameTouched.value = false; filterOpen.value = false; void refreshExtra() })
onMounted(() => { void refresh(); timer = setInterval(() => { void refresh(); void refreshExtra() }, 3000) })
onUnmounted(() => clearInterval(timer))
watch(() => props.hash, () => { parts.value = null; sources.value = []; det.value = null; blocks.value = null; void refresh() })

// ed2k link with eMule options: own address as a source (needs a high ID: the ID is the IP address) and the HTML tag
const withSource = ref(false), asHtml = ref(false)
const core = useCore()
const myIp = computed(() => { const e = core.status?.ed2k; if (!e?.connected || !e.high_id || !e.id) return ''; const n = e.id >>> 0; return `${n & 255}.${(n >>> 8) & 255}.${(n >>> 16) & 255}.${n >>> 24}` })
const link = computed(() => {
  if (!d.value) return ''
  let l = ed2kLink(d.value.name, d.value.size, d.value.hash)
  if (withSource.value && myIp.value) l = l.replace(/\|\/$/, `|/|sources,${myIp.value}:${core.status?.ports.tcp}|/`)
  return asHtml.value ? `<a href="${l}">${d.value.name.replace(/&/g, '&amp;').replace(/</g, '&lt;')}</a>` : l
})
async function copy() {
  if (!d.value) return
  try { await navigator.clipboard.writeText(link.value); toasts.ok(t('detail.copied')) } catch { toasts.error(t('detail.copyFailed')) }
}
const ago = (unix: number) => (unix > 0 ? t('detail.ago', { d: duration(Math.max(0, Date.now() / 1000 - unix)) }) : '')
const dash = (unix: number) => (unix > 0 ? `${dateTime(unix)}  (${ago(unix)})` : t('detail.never'))
const pct1 = (n: number) => `${n.toLocaleString(undefined, { minimumFractionDigits: 1, maximumFractionDigits: 1 })}%`
// streaming: a temporary link (without the token) that a local player opens; an .m3u playlist is downloaded and the system opens it with the default player
async function streamUrl() { const r = await session.getApi().streamLink(props.hash); return { url: location.origin + r.path, name: r.name } }
async function openPlayer() {
  try {
    const { url, name } = await streamUrl()
    const a = document.createElement('a')
    a.href = URL.createObjectURL(new Blob([`#EXTM3U\n#EXTINF:-1,${name}\n${url}\n`], { type: 'audio/x-mpegurl' })); a.download = name.replace(/[\\/]/g, '_') + '.m3u'; a.click(); URL.revokeObjectURL(a.href)
    toasts.ok(t('detail.playlistSaved'))
  } catch (e) { toasts.error(errText(e)) }
}
async function copyStream() {
  try { await navigator.clipboard.writeText((await streamUrl()).url); toasts.ok(t('detail.streamCopied')) } catch (e) { toasts.error(errText(e)) }
}
async function cancel() {
  if (!d.value) return
  if (done.value) { await dl.remove([d.value.hash]); return }
  if (await ui.ask({ title: t('detail.cancelTitle'), text: t('detail.cancelText', { name: d.value.name }), confirm: t('detail.cancelConfirm'), danger: true, list: [{ label: d.value.name, info: `${bytes(d.value.size, 2, true)} · ${percent(d.value.percent)}` }] })) await dl.remove([d.value.hash])
}
const stateOrder = (s: Source) => (s.state === 'downloading' ? 0 : s.state === 'on_queue' ? 1 : 2)
const own = computed(() => sources.value.filter((s) => !s.other_file))
const other = computed(() => sources.value.filter((s) => s.other_file !== undefined))
const sortedSources = computed(() => [...own.value].sort((a, b) => stateOrder(a) - stateOrder(b) || b.speed - a.speed))
</script>
<template>
  <section v-if="d && info" class="flex h-full flex-col" :aria-label="t('detail.aria')">
    <header class="flex items-start gap-3 border-b border-line px-5 py-4">
      <StatusIcon :download="d" size="lg" />
      <div class="min-w-0 flex-1"><h2 class="break-words text-base font-semibold leading-snug">{{ d.name }}</h2><p class="pt-0.5 text-xs font-medium" :class="toneClass(info.tone)">{{ t(info.labelKey) }}</p>
        <div v-if="!done" class="flex flex-wrap items-center gap-x-4 gap-y-2 pt-2 text-xs text-mute">
          <label class="flex items-center gap-1.5">{{ t('common.priority') }}
            <select class="rounded-md border border-line bg-bg px-2 py-1 text-xs text-fg" :value="d.priority" @change="dl.setPriority(d.hash, ($event.target as HTMLSelectElement).value as Priority)"><option v-for="p in PRIORITIES" :key="p" :value="p">{{ t(priorityKey(p)) }}</option></select></label>
          <label v-if="prefs.categories.length > 1" class="flex items-center gap-1.5">{{ t('common.category') }}
            <select class="rounded-md border border-line bg-bg px-2 py-1 text-xs text-fg" :value="d.category" @change="dl.setCategory(d.hash, Number(($event.target as HTMLSelectElement).value))"><option v-for="c in prefs.categories" :key="c.id" :value="c.id">{{ c.id === 0 ? t('common.noCategory') : c.title }}</option></select></label>
        </div></div>
      <button class="rounded-md p-2 text-mute hover:bg-sunk hover:text-fg" :aria-label="t('detail.close')" @click="emit('close')"><Icon name="x" /></button>
    </header>
    <nav class="flex gap-1 overflow-x-auto border-b border-line px-3 text-xs" role="tablist" :aria-label="t('fd.tabs')">
      <button v-for="x in tabs" :key="x" role="tab" :aria-selected="tab === x" class="whitespace-nowrap border-b-2 px-2 py-2" :class="tab === x ? 'border-accent font-medium' : 'border-transparent text-mute hover:text-fg'" @click="tab = x">
        {{ t('fd.tab.' + x) }}<span v-if="x === 'sources' && !done" class="ml-1 tabular-nums text-mute">{{ own.length }}</span></button>
    </nav>
    <div class="flex flex-1 flex-col gap-5 overflow-y-auto p-5">
      <template v-if="tab === 'progress'">
        <div>
          <div class="flex items-baseline justify-between pb-1.5 text-sm"><span class="font-medium tabular-nums">{{ percent(d.percent) }}</span><span class="text-xs text-mute tabular-nums">{{ t('detail.of', { done: bytes(d.completed, 2, true), size: bytes(d.size, 2, true) }) }}</span></div>
          <ProgressBar :value="d.percent" tone="accent" :label="d.name" />
        </div>
        <div v-if="parts"><h3 class="pb-2 text-xs font-semibold uppercase tracking-wide text-mute">{{ t('detail.blocks') }}</h3><PartsMap :parts="parts" :active="activeParts" /></div>
        <section class="rounded-lg border border-line" :aria-label="t('blocks.title')">
          <h3 class="flex items-baseline justify-between border-b border-line px-3 py-1.5 text-xs font-semibold uppercase tracking-wide text-mute"><span>{{ t('blocks.title') }}</span><span class="font-normal tabular-nums">{{ blocks?.active.length ?? 0 }}</span></h3>
          <p v-if="!blocks?.active.length" class="px-3 py-4 text-center text-xs text-mute">{{ t('blocks.none') }}</p>
          <ul v-else class="divide-y divide-line text-xs">
            <li v-for="a in blocks.active" :key="a.part" class="flex flex-col gap-1.5 px-3 py-2.5">
              <div class="flex items-baseline justify-between gap-3"><span class="text-sm font-medium">{{ t('parts.block', { n: a.part + 1 }) }} <span class="text-xs font-normal text-warn">{{ t(a.receiving === false ? 'parts.tag.requested' : 'parts.tag.receiving') }}</span></span>
                <span class="tabular-nums text-mute">{{ bytes(a.size - a.missing, 2, true) }} / {{ bytes(a.size, 2, true) }} · {{ percent(a.size ? 100 * (1 - a.missing / a.size) : 0) }}</span></div>
              <ProgressBar :value="a.size ? 100 * (1 - a.missing / a.size) : 0" tone="warn" :label="t('parts.block', { n: a.part + 1 })" />
              <ul class="flex flex-col gap-0.5">
                <li v-for="(s, i) in a.sources" :key="s.ip + ':' + s.port + i" class="flex items-baseline justify-between gap-3"><span class="min-w-0 truncate">{{ s.name || t('detail.unknownUser') }} <span class="text-mute">· {{ s.software || '—' }}</span></span>
                  <span class="shrink-0 tabular-nums text-mute">{{ speed(s.speed) }} · {{ t('blocks.pieces', s.blocks.length) }}: {{ bytes(s.blocks.reduce((n, b) => n + b.transferred, 0), 1, true) }} / {{ bytes(s.blocks.reduce((n, b) => n + b.end - b.start + 1, 0), 1, true) }}</span></li>
              </ul>
            </li>
          </ul>
          <p class="border-t border-line px-3 py-1.5 text-[11px] text-mute">{{ t('blocks.hint', { size: bytes(blocks?.block_size ?? 184320, 0, true) }) }}</p>
        </section>
      </template>

      <template v-else-if="tab === 'general'">
        <template v-if="det && !done">
          <section class="rounded-lg border border-line"><h3 class="border-b border-line px-3 py-1.5 text-xs font-semibold uppercase tracking-wide text-mute">{{ t('fd.general') }}</h3>
            <dl class="grid grid-cols-[auto_minmax(0,1fr)] gap-x-4 gap-y-1.5 p-3 text-xs">
              <dt class="text-right text-mute">{{ t('fd.partMet') }}</dt><dd class="break-all font-mono">{{ det.part_met_path }}</dd>
              <dt class="text-right text-mute">{{ t('fd.hash') }}</dt><dd class="break-all font-mono">{{ det.hash }}</dd>
              <dt class="text-right text-mute">{{ t('fd.aichHash') }}</dt><dd class="break-all"><span class="font-mono">{{ det.aich_hash ?? '—' }}</span> <span class="text-mute">· {{ t('fd.aichStatus.' + det.aich_status) }}</span></dd>
              <dt class="text-right text-mute">{{ t('fd.size') }}</dt><dd class="tabular-nums">{{ bytes(det.size, 2, true) }} ({{ t('detail.bytes', { n: number(det.size) }) }}); {{ t('fd.onDisk') }}: {{ det.size_on_disk >= 0 ? bytes(det.size_on_disk, 2, true) : '—' }}</dd>
              <dt class="text-right text-mute">{{ t('fd.type') }}</dt><dd>{{ det.type || '—' }}</dd>
              <dt class="text-right text-mute">{{ t('fd.parts') }}</dt><dd class="tabular-nums">{{ det.parts.total }}; {{ t('fd.available') }}: {{ det.parts.available }} ({{ pct1(det.parts.available_pct) }})</dd>
              <dt class="text-right text-mute">{{ t('fd.state') }}</dt><dd>{{ t(info.labelKey) }}<template v-if="det.sources.transferring"> ({{ t('fd.fromSources', det.sources.transferring) }})</template></dd>
              <dt class="text-right text-mute">{{ t('fd.hashset') }}</dt><dd>{{ det.hashset.ed2k ? t('fd.yes') + (det.hashset.aich ? ' (eD2K + AICH)' : ' (eD2K)') : det.hashset.aich ? t('fd.yes') + ' (AICH)' : t('fd.no') }}</dd>
            </dl></section>
          <section class="rounded-lg border border-line"><h3 class="border-b border-line px-3 py-1.5 text-xs font-semibold uppercase tracking-wide text-mute">{{ t('fd.transfers') }}</h3>
            <dl class="grid grid-cols-[auto_minmax(0,1fr)] gap-x-4 gap-y-1.5 p-3 text-xs">
              <dt class="text-right text-mute">{{ t('detail.sources') }}</dt><dd class="tabular-nums">{{ number(det.sources.total) }} ({{ t('fd.valid') }}: {{ det.sources.valid }}, {{ t('fd.noNeeded') }}: {{ det.sources.no_needed_parts }}, {{ t('fd.otherFile') }}: {{ det.sources.other_file }})</dd>
              <dt class="text-right text-mute">{{ t('detail.speed') }}</dt><dd class="tabular-nums">{{ speed(det.speed) }}</dd>
              <dt class="text-right text-mute">{{ t('fd.transferred') }}</dt><dd class="tabular-nums">{{ bytes(det.transferred, 2, true) }}</dd>
              <dt class="text-right text-mute">{{ t('fd.compression') }}</dt><dd class="tabular-nums">{{ bytes(det.compression_gain.bytes, 2, true) }} ({{ pct1(det.compression_gain.pct) }})</dd>
              <dt class="text-right text-mute">{{ t('fd.completed') }}</dt><dd class="tabular-nums">{{ bytes(det.completed.bytes, 2, true) }} ({{ pct1(det.completed.pct) }})</dd>
              <dt class="text-right text-mute">{{ t('fd.corrupted') }}</dt><dd class="tabular-nums">{{ bytes(det.corrupted.bytes, 2, true) }} ({{ pct1(det.corrupted.pct) }})</dd>
              <dt class="text-right text-mute">{{ t('fd.remaining') }}</dt><dd class="tabular-nums">{{ bytes(det.remaining.bytes, 2, true) }} ({{ pct1(det.remaining.pct) }}) · {{ eta(d.size, d.completed, d.speed) }}</dd>
              <dt class="text-right text-mute">{{ t('fd.recovered') }}</dt><dd class="tabular-nums">{{ t('fd.nParts', det.recovered_parts) }}</dd>
            </dl></section>
          <section class="rounded-lg border border-line"><h3 class="border-b border-line px-3 py-1.5 text-xs font-semibold uppercase tracking-wide text-mute">{{ t('fd.dates') }}</h3>
            <dl class="grid grid-cols-[auto_minmax(0,1fr)] gap-x-4 gap-y-1.5 p-3 text-xs">
              <dt class="text-right text-mute">{{ t('fd.added') }}</dt><dd>{{ dash(det.added) }}</dd>
              <dt class="text-right text-mute">{{ t('fd.activeTime') }}</dt><dd class="tabular-nums">{{ det.active_seconds ? duration(det.active_seconds) : '—' }}</dd>
              <dt class="text-right text-mute">{{ t('fd.lastComplete') }}</dt><dd>{{ dash(det.last_seen_complete) }}</dd>
              <dt class="text-right text-mute">{{ t('fd.lastReceived') }}</dt><dd>{{ dash(det.last_received) }}</dd>
            </dl></section>
        </template>
        <dl v-else-if="done" class="grid grid-cols-1 gap-3 text-sm">
          <div><dt class="text-xs text-mute">{{ t('detail.size') }}</dt><dd class="tabular-nums">{{ t('detail.bytes', { n: number(d.size) }) }}</dd></div>
          <div v-if="d.completed_at"><dt class="text-xs text-mute">{{ t('detail.completedAt') }}</dt><dd>{{ dateTime(d.completed_at) }}</dd></div>
          <div v-if="d.path"><dt class="text-xs text-mute">{{ t('detail.file') }}</dt><dd class="break-all font-mono text-xs">{{ d.path }}</dd></div>
          <p class="text-xs text-mute">{{ t('detail.doneHint') }}</p>
        </dl>
      </template>

      <template v-else-if="tab === 'sources'">
        <div class="flex items-center justify-between"><h3 class="text-xs font-semibold uppercase tracking-wide text-mute">{{ t('detail.sourcesTitle') }} <span class="font-normal tabular-nums">{{ own.length }}</span><span v-if="other.length" class="ml-1 font-normal normal-case tabular-nums text-accent" :title="t('detail.otherHint')">+{{ other.length }} {{ t('detail.forOther') }}</span></h3>
          <button class="inline-flex items-center gap-1 rounded-lg border border-line px-2.5 py-1 text-xs hover:bg-sunk" :title="t('detail.findSourcesHint')" @click="dl.findSources(d.hash)"><Icon name="refresh" class="!size-3.5" />{{ t('detail.findSources') }}</button></div>
        <p v-if="!own.length" class="rounded-lg border border-line px-3 py-4 text-center text-sm text-mute">{{ other.length ? t('detail.onlyOther', { n: other.length }) : t('detail.noSources') }}</p>
        <ul v-else class="divide-y divide-line rounded-lg border border-line text-sm">
          <li v-for="(s, i) in sortedSources" :key="s.ip + ':' + s.port + i" class="flex items-start justify-between gap-3 px-3 py-2">
            <div class="min-w-0"><p class="truncate font-medium">{{ s.name || t('detail.unknownUser') }}</p><p class="truncate text-xs text-mute">{{ s.software || '—' }} · {{ s.ip }}:{{ s.port }}<template v-if="s.low_id"> · <span :title="t('detail.lowIdHint')">{{ t('detail.lowId') }}</span></template> · {{ t('src.from.' + s.from) }}</p></div>
            <div class="shrink-0 text-right text-xs"><p :class="s.state === 'downloading' ? 'font-medium text-ok' : 'text-mute'">{{ t('src.state.' + s.state) }}</p><p class="tabular-nums text-mute"><template v-if="s.state === 'downloading'">{{ speed(s.speed) }}</template><template v-else-if="s.queue_rank">{{ t('detail.queuePos', { n: s.queue_rank }) }}</template><span v-else-if="s.state === 'on_queue'" :title="t('detail.queuePosUnknownHint')">{{ t('detail.queuePosUnknown') }}</span></p></div>
          </li>
        </ul>
        <ul v-if="other.length" class="divide-y divide-line rounded-lg border border-dashed border-line text-sm" :aria-label="t('detail.forOther')">
          <li v-for="(s, i) in other" :key="'o' + s.ip + ':' + s.port + i" class="px-3 py-2"><p class="truncate font-medium">{{ s.name || t('detail.unknownUser') }} <span class="text-xs font-normal text-mute">{{ s.software || '—' }} · {{ s.ip }}:{{ s.port }}</span></p>
            <p class="truncate text-xs text-accent">{{ t('detail.usedFor', { file: s.other_file }) }}</p></li>
        </ul>
      </template>

      <template v-else-if="tab === 'names'">
        <h3 class="text-xs font-semibold uppercase tracking-wide text-mute">{{ t('fd.namesTitle') }}</h3>
        <p v-if="!names || !names.names.length" class="rounded-lg border border-line px-3 py-4 text-center text-sm text-mute">{{ t('fd.noNames') }}</p>
        <ul v-else class="divide-y divide-line rounded-lg border border-line text-sm">
          <li v-for="n in names.names" :key="n.name"><button class="flex w-full items-center justify-between gap-3 px-3 py-2 text-left hover:bg-sunk" :title="t('fd.useName')" @click="pickName(n.name)"><span class="min-w-0 break-words">{{ n.name }}</span><span class="shrink-0 text-xs tabular-nums text-mute">{{ t('fd.nSources', n.count) }}</span></button></li>
        </ul>
        <label class="flex flex-col gap-1 text-xs text-mute">{{ t('fd.newName') }}
          <span class="flex gap-2"><input v-model="newName" type="text" maxlength="255" class="min-w-0 flex-1 rounded-lg border border-line bg-bg px-3 py-2 text-sm text-fg" @input="nameTouched = true" @keydown.enter="rename" />
            <button class="rounded-lg border border-line px-3 py-2 text-sm text-fg hover:bg-sunk" @click="clearName">{{ t('fd.clear') }}</button>
            <button class="rounded-lg bg-accent px-3 py-2 text-sm font-medium text-onaccent disabled:opacity-50" :disabled="!canRename" @click="rename">{{ t('fd.rename') }}</button></span></label>
        <p class="text-xs text-mute">{{ t('fd.renameHint') }}</p>
      </template>

      <template v-else-if="tab === 'comments'">
        <div class="flex flex-wrap items-center justify-between gap-2"><h3 class="text-xs font-semibold uppercase tracking-wide text-mute">{{ t('fd.tab.comments') }} <span class="font-normal tabular-nums">{{ comments?.comments.length ?? 0 }}</span></h3>
          <span class="flex flex-wrap gap-2">
            <button class="rounded-lg border border-line px-2.5 py-1 text-xs hover:bg-sunk" @click="openFilter">{{ t('fd.editFilter') }}</button>
            <button class="rounded-lg border border-line px-2.5 py-1 text-xs hover:bg-sunk disabled:opacity-50" :disabled="!comments?.kad_connected || comments.kad_searching || kadBusy" :title="comments && !comments.kad_connected ? t('fd.kadOff') : ''" @click="searchKad">{{ comments?.kad_searching ? t('fd.kadSearching') : t('fd.searchKad') }}</button></span></div>
        <div v-if="filterOpen" class="flex flex-col gap-2 rounded-lg border border-line p-3 text-xs">
          <label class="flex flex-col gap-1 text-mute">{{ t('fd.filterLabel') }}<input v-model="filterText" type="text" class="rounded-lg border border-line bg-bg px-3 py-2 font-mono text-xs text-fg" @keydown.enter="saveFilter" /></label>
          <p class="text-mute">{{ t('fd.filterHint') }}</p>
          <span class="flex justify-end gap-2"><button class="rounded-lg border border-line px-3 py-1.5 hover:bg-sunk" @click="filterOpen = false">{{ t('common.cancel') }}</button><button class="rounded-lg bg-accent px-3 py-1.5 font-medium text-onaccent" @click="saveFilter">{{ t('fd.saveFilter') }}</button></span></div>
        <p v-if="!comments || !comments.comments.length" class="rounded-lg border border-line px-3 py-4 text-center text-sm text-mute">{{ t('fd.noComments') }}</p>
        <ul v-else class="divide-y divide-line rounded-lg border border-line text-sm">
          <li v-for="(c, i) in comments.comments" :key="i" class="flex flex-col gap-0.5 px-3 py-2"><p class="break-words">{{ c.comment || '—' }}</p>
            <p class="break-words text-xs text-mute"><span class="font-medium text-fg">{{ stars(c.rating) }}</span> · {{ c.network === 'kad' ? 'Kad' : 'eD2K' }}<template v-if="c.user"> · {{ c.user }}</template><template v-if="c.file_name"> · {{ c.file_name }}</template></p></li>
        </ul>
      </template>

      <template v-else-if="tab === 'content'">
        <div class="flex items-center justify-between"><h3 class="text-xs font-semibold uppercase tracking-wide text-mute">{{ t('fd.tab.content') }}</h3>
          <button class="inline-flex items-center gap-1 rounded-lg border border-line px-2.5 py-1 text-xs hover:bg-sunk" @click="loadContent"><Icon name="refresh" class="!size-3.5" />{{ t('fd.reread') }}</button></div>
        <p v-if="!content || content.state === 'running' || content.state === 'idle'" class="rounded-lg border border-line px-3 py-4 text-center text-sm text-mute" role="status">{{ t('fd.reading') }}</p>
        <p v-else-if="content.state === 'unavailable'" class="rounded-lg border border-line px-3 py-4 text-sm text-mute">{{ t('fd.noMediainfo') }}</p>
        <p v-else-if="content.state === 'failed'" class="rounded-lg border border-line px-3 py-4 text-sm text-warn">{{ t('fd.contentFailed', { why: content.reason ?? '' }) }}</p>
        <p v-else-if="!content.recognized" class="rounded-lg border border-line px-3 py-4 text-sm text-mute">{{ t('fd.notRecognized') }}</p>
        <template v-else>
          <section class="rounded-lg border border-line"><h3 class="border-b border-line px-3 py-1.5 text-xs font-semibold uppercase tracking-wide text-mute">{{ t('fd.general') }}</h3>
            <dl class="grid grid-cols-[auto_minmax(0,1fr)] gap-x-4 gap-y-1.5 p-3 text-xs">
              <dt class="text-right text-mute">{{ t('fd.size') }}</dt><dd class="tabular-nums">{{ bytes(d.size, 2, true) }}</dd>
              <template v-if="content.general?.duration_s"><dt class="text-right text-mute">{{ t('fd.duration') }}</dt><dd class="tabular-nums">{{ hms(content.general.duration_s) }}</dd></template>
              <dt class="text-right text-mute">{{ t('fd.container') }}</dt><dd>{{ content.general?.format ?? '—' }}</dd>
              <template v-if="content.general?.movie || content.general?.title"><dt class="text-right text-mute">{{ t('fd.titleLabel') }}</dt><dd>{{ content.general.movie || content.general.title }}</dd></template>
              <template v-if="content.general?.date"><dt class="text-right text-mute">{{ t('fd.date') }}</dt><dd>{{ content.general.date }}</dd></template>
            </dl></section>
          <section v-for="(v, i) in content.video" :key="'v' + i" class="rounded-lg border border-line"><h3 class="border-b border-line px-3 py-1.5 text-xs font-semibold uppercase tracking-wide text-mute">{{ t('fd.video') }}<span v-if="content.video!.length > 1"> #{{ i + 1 }}</span></h3>
            <dl class="grid grid-cols-[auto_minmax(0,1fr)] gap-x-4 gap-y-1.5 p-3 text-xs">
              <dt class="text-right text-mute">{{ t('fd.codec') }}</dt><dd>{{ [v.codec, v.profile].filter(Boolean).join(' ') || '—' }}</dd>
              <dt class="text-right text-mute">{{ t('fd.bitrate') }}</dt><dd class="tabular-nums">{{ kbps(v.bitrate) }}</dd>
              <dt class="text-right text-mute">{{ t('fd.resolution') }}</dt><dd class="tabular-nums">{{ v.width && v.height ? `${v.width} × ${v.height}` : '—' }}</dd>
              <dt class="text-right text-mute">{{ t('fd.aspect') }}</dt><dd class="tabular-nums">{{ v.aspect ? v.aspect.toLocaleString(undefined, { minimumFractionDigits: 3, maximumFractionDigits: 3 }) : '—' }}</dd>
              <dt class="text-right text-mute">{{ t('fd.fps') }}</dt><dd class="tabular-nums">{{ v.fps ? v.fps.toLocaleString(undefined, { maximumFractionDigits: 3 }) : '—' }}</dd>
            </dl></section>
          <section v-for="(a, i) in content.audio" :key="'a' + i" class="rounded-lg border border-line"><h3 class="border-b border-line px-3 py-1.5 text-xs font-semibold uppercase tracking-wide text-mute">{{ t('fd.audio') }}<span v-if="content.audio!.length > 1"> #{{ i + 1 }}</span><span v-if="a.default" class="ml-2 font-normal normal-case text-accent">{{ t('fd.default') }}</span></h3>
            <dl class="grid grid-cols-[auto_minmax(0,1fr)] gap-x-4 gap-y-1.5 p-3 text-xs">
              <dt class="text-right text-mute">{{ t('fd.codec') }}</dt><dd>{{ a.codec ?? '—' }}<template v-if="a.commercial"> ({{ a.commercial }})</template></dd>
              <dt class="text-right text-mute">{{ t('fd.bitrate') }}</dt><dd class="tabular-nums">{{ kbps(a.bitrate) }}</dd>
              <dt class="text-right text-mute">{{ t('fd.channels') }}</dt><dd class="tabular-nums">{{ a.channels ?? '—' }}</dd>
              <dt class="text-right text-mute">{{ t('fd.sampling') }}</dt><dd class="tabular-nums">{{ a.sampling ? `${(a.sampling / 1000).toLocaleString(undefined, { maximumFractionDigits: 1 })} kHz` : '—' }}</dd>
              <dt class="text-right text-mute">{{ t('fd.language') }}</dt><dd>{{ trackLine(a) || '—' }}</dd>
            </dl></section>
          <section v-if="content.text?.length" class="rounded-lg border border-line"><h3 class="border-b border-line px-3 py-1.5 text-xs font-semibold uppercase tracking-wide text-mute">{{ t('fd.subtitles') }} <span class="font-normal tabular-nums">{{ content.text.length }}</span></h3>
            <ul class="divide-y divide-line text-xs"><li v-for="(x, i) in content.text" :key="'t' + i" class="flex items-baseline justify-between gap-3 px-3 py-1.5"><span class="min-w-0 break-words">{{ trackLine(x) || '—' }}</span><span class="shrink-0 text-mute">{{ x.codec }}<template v-if="x.forced"> · {{ t('fd.forced') }}</template><template v-if="x.default"> · {{ t('fd.default') }}</template></span></li></ul></section>
        </template>
      </template>

      <template v-else>
        <label class="flex flex-col gap-1 text-xs text-mute">{{ t('fd.tab.ed2k') }}
          <textarea readonly rows="5" class="w-full resize-none rounded-lg border border-line bg-sunk px-3 py-2 font-mono text-xs text-fg" :value="link" @focus="($event.target as HTMLTextAreaElement).select()"></textarea></label>
        <div class="flex justify-end"><button class="inline-flex items-center gap-1.5 rounded-lg border border-line px-3.5 py-2 text-sm hover:bg-sunk" @click="copy"><Icon name="copy" />{{ t('fd.copyClipboard') }}</button></div>
        <fieldset class="flex flex-col gap-2 rounded-lg border border-line p-3 text-xs"><legend class="px-1 text-mute">{{ t('fd.linkOptions') }}</legend>
          <label class="flex items-center gap-2" :class="myIp ? '' : 'opacity-50'"><input v-model="withSource" type="checkbox" :disabled="!myIp" />{{ t('fd.withSource') }}<span v-if="!myIp" class="text-mute">({{ t('fd.needHighId') }})</span></label>
          <label class="flex items-center gap-2"><input v-model="asHtml" type="checkbox" />{{ t('fd.asHtml') }}</label>
        </fieldset>
      </template>
    </div>
    <footer class="flex flex-wrap gap-2 border-t border-line px-5 py-3">
      <button v-if="!done" class="inline-flex items-center gap-1.5 rounded-lg border border-line px-3.5 py-2 text-sm hover:bg-sunk" @click="paused ? dl.resume([d.hash]) : dl.pause([d.hash])"><Icon :name="paused ? 'play' : 'pause'" />{{ paused ? t('common.resume') : t('common.pause') }}</button>
      <button v-if="!done && !paused" class="inline-flex items-center gap-1.5 rounded-lg border border-line px-3.5 py-2 text-sm hover:bg-sunk" :title="t('detail.stopHint')" @click="dl.stop([d.hash])"><Icon name="stop" />{{ t('common.stop') }}</button>
      <button class="inline-flex items-center gap-1.5 rounded-lg border border-line px-3.5 py-2 text-sm hover:bg-sunk" :title="t('detail.openPlayerHint')" @click="openPlayer"><Icon name="play" />{{ t('detail.openPlayer') }}</button>
      <button class="inline-flex items-center gap-1.5 rounded-lg border border-line px-3.5 py-2 text-sm hover:bg-sunk" :title="t('detail.copyStreamHint')" @click="copyStream"><Icon name="copy" />{{ t('detail.copyStream') }}</button>
      <button class="ml-auto inline-flex items-center gap-1.5 rounded-lg border border-line px-3.5 py-2 text-sm hover:bg-sunk" :class="done ? '' : 'text-bad'" @click="cancel"><Icon :name="done ? 'x' : 'trash'" />{{ done ? t('detail.removeFromList') : t('common.cancelDownload') }}</button>
    </footer>
  </section>
</template>
