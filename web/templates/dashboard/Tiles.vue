<!-- Modello B: le tessere di riepilogo (download, upload, server ed2k, Kad) in cima alle schermate Download e Upload -->
<script setup lang="ts">
import { computed } from 'vue'
import { useI18n } from 'vue-i18n'
import { useCore } from '@/stores/core'
import { useDownloads } from '@/stores/downloads'
import { number, speed } from '@/lib/format'
import Sparkline from '@/components/Sparkline.vue'

const { t } = useI18n()
const core = useCore(), dl = useDownloads()
const st = computed(() => core.status)
const ed2kValue = computed(() => { const e = st.value?.ed2k; return !e ? '…' : e.connected ? (e.server?.name || t('shell.connected')) : t(e.connecting ? 'shell.connecting' : 'shell.disconnected') })
const ed2kSub = computed(() => { const e = st.value?.ed2k; return e?.connected ? [t(e.high_id ? 'shell.highId' : 'shell.lowId'), e.server?.users ? t('skin.dashboard.users', { n: number(e.server.users) }) : ''].filter(Boolean).join(' · ') : '' })
const kadValue = computed(() => { const k = st.value?.kad; return !k?.running ? t('shell.off') : t(k.connected ? 'shell.connected' : 'shell.connecting') })
const kadSub = computed(() => { const k = st.value?.kad; return k?.running ? [k.contacts ? t('skin.dashboard.contacts', { n: number(k.contacts) }) : '', k.udp_verified ? t('skin.dashboard.udpOpen') : ''].filter(Boolean).join(' · ') : '' })
</script>
<template>
<section class="grid grid-cols-2 gap-3 lg:grid-cols-4" :aria-label="t('skin.dashboard.summary')">
  <div class="rounded-2xl border border-line bg-surface p-4">
    <p class="text-xs font-semibold uppercase tracking-wide text-mute">{{ t('skin.dashboard.download') }}</p>
    <p class="pt-1 text-2xl font-bold tracking-tight tabular-nums">{{ speed(st?.transfers.download_speed ?? 0) }}</p>
    <p class="text-xs text-mute">{{ t('skin.dashboard.activeFiles', dl.counts.active) }}</p><Sparkline :samples="core.samples" kind="down" />
  </div>
  <div class="rounded-2xl border border-line bg-surface p-4">
    <p class="text-xs font-semibold uppercase tracking-wide text-mute">{{ t('skin.dashboard.upload') }}</p>
    <p class="pt-1 text-2xl font-bold tracking-tight tabular-nums">{{ speed(st?.transfers.upload_speed ?? 0) }}</p>
    <p class="text-xs text-mute">{{ t('skin.dashboard.served', st?.transfers.uploading ?? 0) }}</p><Sparkline :samples="core.samples" kind="up" />
  </div>
  <div class="rounded-2xl border border-line bg-surface p-4">
    <p class="text-xs font-semibold uppercase tracking-wide text-mute">{{ t('skin.dashboard.ed2k') }}</p>
    <p class="truncate pt-1 text-2xl font-bold tracking-tight" :title="ed2kValue">{{ ed2kValue }}</p><p class="truncate text-xs text-mute">{{ ed2kSub || '—' }}</p>
  </div>
  <div class="rounded-2xl border border-line bg-surface p-4">
    <p class="text-xs font-semibold uppercase tracking-wide text-mute">Kad</p>
    <p class="truncate pt-1 text-2xl font-bold tracking-tight">{{ kadValue }}</p><p class="truncate text-xs text-mute">{{ kadSub || '—' }}</p>
  </div>
</section>
</template>
