<script setup lang="ts">
import { nextTick, onMounted, ref, watch } from 'vue'
import { useI18n } from 'vue-i18n'
import { useLog, type LevelFilter, type SourceFilter } from '@/stores/log'
import { usePrefs } from '@/stores/prefs'
import { useUi } from '@/stores/ui'
import { number } from '@/lib/format'
import Icon from '@/components/Icon.vue'
import EmptyState from '@/components/EmptyState.vue'

const { t, locale } = useI18n()
const log = useLog(), prefs = usePrefs(), ui = useUi()
const box = ref<HTMLElement | null>(null)
const LEVELS: LevelFilter[] = ['all', 'info', 'warning', 'error']
const SOURCES: SourceFilter[] = ['all', 'general', 'server', 'api']
onMounted(async () => { await Promise.all([log.loaded ? log.sync() : log.load(), prefs.load()]); scroll() })
// "Segui": con la casella attiva la vista resta in fondo quando arrivano righe nuove
function scroll() { void nextTick(() => { if (log.follow && box.value) box.value.scrollTop = box.value.scrollHeight }) }
watch(() => log.shown.length, scroll)
watch(() => log.follow, scroll)
function onScroll() { const el = box.value; if (el && log.follow && el.scrollHeight - el.scrollTop - el.clientHeight > 80) log.follow = false }   // chi sale a leggere ferma il "segui"
const time = (ms: number) => new Date(ms).toLocaleTimeString(locale.value, { hour12: false })
const levelClass = (l: string) => (l === 'error' ? 'text-bad' : l === 'warning' ? 'text-warn' : l === 'debug' ? 'text-mute' : 'text-fg')

async function clear() { if (await ui.ask({ title: t('log.clearTitle'), text: t('log.clearText'), confirm: t('log.clear'), danger: true })) await log.clear() }
function save() {
  const text = log.shown.map((l) => `${new Date(l.time).toISOString()} [${l.level}] [${l.source}] ${l.text}`).join('\n') + '\n'
  const a = document.createElement('a')
  a.href = URL.createObjectURL(new Blob([text], { type: 'text/plain;charset=utf-8' })); a.download = 'emuled-log.txt'; a.click(); URL.revokeObjectURL(a.href)
}
const verbose = () => prefs.saved.verbose_log === true
async function toggleVerbose() { prefs.draft.verbose_log = !verbose(); await prefs.save() }
</script>
<template>
  <div class="flex h-full min-h-0 flex-col">
    <div class="flex flex-wrap items-center gap-x-3 gap-y-2 border-b border-line bg-surface px-4 py-2">
      <h1 class="mr-1 text-lg font-semibold">{{ t('nav.log') }}</h1>
      <div class="flex items-center gap-1 text-sm" role="tablist" :aria-label="t('log.level')">
        <button v-for="l in LEVELS" :key="l" role="tab" :aria-selected="log.level === l" class="whitespace-nowrap rounded-md px-3 py-1.5" :class="log.level === l ? 'bg-sunk font-medium' : 'text-mute hover:bg-sunk'" @click="log.level = l">{{ t('log.levels.' + l) }}</button>
      </div>
      <select v-model="log.source" class="rounded-md border border-line bg-bg px-2 py-1.5 text-sm" :aria-label="t('log.source')"><option v-for="x in SOURCES" :key="x" :value="x">{{ t('log.sources.' + x) }}</option></select>
      <input v-model="log.query" type="search" class="min-w-0 flex-1 basis-40 rounded-md border border-line bg-bg px-3 py-1.5 text-sm placeholder:text-mute lg:max-w-xs" :placeholder="t('log.search')" :aria-label="t('log.search')" />
      <label class="flex items-center gap-1.5 text-sm"><input v-model="log.follow" type="checkbox" class="size-4" />{{ t('log.follow') }}</label>
      <div class="ml-auto flex items-center gap-1">
        <button class="rounded-md p-2 text-mute hover:bg-sunk hover:text-fg" :title="t('log.save')" :aria-label="t('log.save')" :disabled="!log.shown.length" @click="save"><Icon name="download" /></button>
        <button class="rounded-md p-2 text-mute hover:bg-sunk hover:text-bad" :title="t('log.clear')" :aria-label="t('log.clear')" :disabled="!log.lines.length" @click="clear"><Icon name="trash" /></button>
      </div>
    </div>
    <div class="flex flex-wrap items-center justify-between gap-2 border-b border-line bg-sunk px-4 py-1.5 text-xs text-mute">
      <span class="tabular-nums" role="status">{{ t('log.count', { shown: number(log.shown.length), total: number(log.lines.length) }) }}</span>
      <label class="flex items-center gap-1.5" :title="t('log.debugHint')"><input type="checkbox" class="size-3.5" :checked="verbose()" :disabled="prefs.saving" @change="toggleVerbose" />{{ t('log.debug') }}</label>
    </div>
    <div v-if="!log.loaded" class="p-8 text-center text-sm text-mute" role="status">{{ t('log.loading') }}</div>
    <EmptyState v-else-if="!log.lines.length" icon="list" :title="t('log.emptyTitle')" :text="t('log.emptyText')" />
    <EmptyState v-else-if="!log.shown.length" icon="search" :title="t('log.noMatchTitle')" :text="t('log.noMatchText')" />
    <ol v-else ref="box" class="relative min-h-0 flex-1 overflow-y-auto bg-surface py-1 font-mono text-xs leading-5" :aria-label="t('nav.log')" @scroll="onScroll">
      <li v-for="l in log.shown" :key="l.id" class="flex gap-3 px-4 py-px hover:bg-sunk" :class="levelClass(l.level)">
        <time class="shrink-0 text-mute tabular-nums" :datetime="new Date(l.time).toISOString()">{{ time(l.time) }}</time>
        <span v-if="l.source !== 'general'" class="shrink-0 rounded bg-sunk px-1.5 text-[10px] font-semibold uppercase leading-5 text-mute">{{ t('log.sources.' + l.source) }}</span>
        <span class="min-w-0 flex-1 whitespace-pre-wrap break-words">{{ l.text }}</span>
        <span v-if="l.level === 'error' || l.level === 'warning'" class="sr-only">{{ t('log.levels.' + l.level) }}</span>
      </li>
    </ol>
  </div>
</template>
