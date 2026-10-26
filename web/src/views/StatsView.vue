<script setup lang="ts">
import { computed } from 'vue'
import { useI18n } from 'vue-i18n'
import { useCore } from '@/stores/core'
import { duration, number, speed } from '@/lib/format'
import SpeedChart from '@/components/SpeedChart.vue'
const { t } = useI18n()
const core = useCore()
const tr = computed(() => core.status?.transfers)
const peak = computed(() => ({ down: Math.max(0, ...core.samples.map((s) => s.down)), up: Math.max(0, ...core.samples.map((s) => s.up)) }))
const cards = computed(() => [
  { l: t('stats.downloadNow'), v: speed(tr.value?.download_speed ?? 0) }, { l: t('stats.uploadNow'), v: speed(tr.value?.upload_speed ?? 0) },
  { l: t('stats.peakDown'), v: speed(peak.value.down) }, { l: t('stats.peakUp'), v: speed(peak.value.up) },
  { l: t('stats.inList'), v: number(tr.value?.downloads ?? 0) }, { l: t('stats.uploading'), v: number(tr.value?.uploading ?? 0) },
  { l: t('stats.waiting'), v: number(tr.value?.waiting ?? 0) }, { l: t('stats.uptime'), v: duration(core.status?.uptime_s ?? NaN) },
])
</script>
<template>
  <div class="flex max-w-4xl flex-col gap-5 p-4">
    <h1 class="text-lg font-semibold">{{ t('nav.stats') }}</h1>
    <section class="rounded-xl border border-line bg-surface p-4"><SpeedChart :samples="core.samples" /></section>
    <dl class="grid grid-cols-2 gap-3 lg:grid-cols-4">
      <div v-for="c in cards" :key="c.l" class="rounded-xl border border-line bg-surface p-4"><dt class="text-xs text-mute">{{ c.l }}</dt><dd class="pt-1 text-lg font-semibold tabular-nums">{{ c.v }}</dd></div>
    </dl>
    <p class="text-xs text-mute">{{ t('stats.note', core.samples.length) }}</p>
  </div>
</template>
