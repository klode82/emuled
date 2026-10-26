<!-- Stato di un download come icona piena e colorata (si nota subito anche da lontano); il testo e' nel suggerimento e per i lettori di schermo -->
<script setup lang="ts">
import { computed } from 'vue'
import { useI18n } from 'vue-i18n'
import { statusInfo } from '@/lib/labels'
import type { Download } from '@/types'
import Icon from './Icon.vue'

const { t } = useI18n()
const props = defineProps<{ download: Download; size?: 'md' | 'lg' }>()
const info = computed(() => statusInfo(props.download))
// una forma e un colore per ogni stato (le classi sono scritte per intero: cosi' Tailwind le genera)
const LOOK: Record<string, { icon: string; cls: string; spin?: boolean }> = {
  active: { icon: 'dl', cls: 'bg-accent text-onaccent' },
  waiting: { icon: 'hourglass', cls: 'bg-warn text-bg' },
  paused: { icon: 'pause', cls: 'bg-fg text-bg' },
  done: { icon: 'check', cls: 'bg-ok text-bg' },
  error: { icon: 'warn', cls: 'bg-bad text-bg' },
  busy: { icon: 'refresh', cls: 'bg-sunk text-fg ring-1 ring-inset ring-line', spin: true },
}
const look = computed(() => LOOK[info.value.key])
const stopped = computed(() => info.value.key === 'paused' && props.download.stopped)
</script>
<template>
  <span class="inline-grid shrink-0 place-items-center rounded-full" :class="[look.cls, size === 'lg' ? 'size-9' : 'size-7']" role="img" :aria-label="t(info.labelKey)" :title="t(info.labelKey)">
    <Icon :name="stopped ? 'stop' : look.icon" :class="[size === 'lg' ? '!size-5' : '!size-4', look.spin ? 'animate-spin motion-reduce:animate-none' : '']" />
  </span>
</template>
