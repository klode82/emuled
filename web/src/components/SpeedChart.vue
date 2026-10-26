<!-- Grafico delle velocita': area scarico + linea invio, asse verticale con 3 valori reali, orizzontale = tempo trascorso -->
<script setup lang="ts">
import { computed } from 'vue'
import { useI18n } from 'vue-i18n'
import { speed } from '@/lib/format'
import type { Sample } from '@/stores/core'

const { t } = useI18n()
const props = defineProps<{ samples: Sample[] }>()
const W = 600, H = 180, PL = 56, PB = 22, PT = 8, PR = 8
const nice = (v: number) => { if (v <= 0) return 1024; const p = 10 ** Math.floor(Math.log10(v)); const m = v / p; return (m <= 1 ? 1 : m <= 2 ? 2 : m <= 5 ? 5 : 10) * p }
const max = computed(() => nice(Math.max(...props.samples.map((s) => Math.max(s.down, s.up)), 0)))
const x = (i: number) => PL + (props.samples.length < 2 ? 0 : (i / (props.samples.length - 1)) * (W - PL - PR))
const y = (v: number) => PT + (1 - v / max.value) * (H - PT - PB)
const line = (k: 'down' | 'up') => props.samples.map((s, i) => `${x(i).toFixed(1)},${y(s[k]).toFixed(1)}`).join(' ')
const area = computed(() => props.samples.length < 2 ? '' : `${line('down')} ${x(props.samples.length - 1).toFixed(1)},${y(0)} ${x(0).toFixed(1)},${y(0)}`)
const ticks = computed(() => [0, 0.5, 1].map((f) => ({ v: max.value * f, y: y(max.value * f) })))
const span = computed(() => (props.samples.length > 1 ? Math.round((props.samples[props.samples.length - 1].t - props.samples[0].t) / 1000) : 0))
</script>
<template>
  <figure class="m-0">
    <svg :viewBox="`0 0 ${W} ${H}`" class="h-auto w-full" role="img" :aria-label="t('stats.chartAria')">
      <g class="text-mute" font-size="10" fill="currentColor">
        <g v-for="tk in ticks" :key="tk.v"><line :x1="PL" :x2="W - PR" :y1="tk.y" :y2="tk.y" class="stroke-line" stroke-width="1" /><text :x="PL - 6" :y="tk.y + 3" text-anchor="end">{{ tk.v ? speed(tk.v) : '0' }}</text></g>
        <text :x="PL" :y="H - 6">−{{ span }} s</text><text :x="W - PR" :y="H - 6" text-anchor="end">{{ t('stats.now') }}</text>
      </g>
      <polygon v-if="area" :points="area" class="fill-soft" />
      <polyline v-if="samples.length > 1" :points="line('down')" class="fill-none stroke-accent stroke-2" stroke-linejoin="round" />
      <polyline v-if="samples.length > 1" :points="line('up')" class="fill-none stroke-warn stroke-2 [stroke-dasharray:5_3]" stroke-linejoin="round" />
    </svg>
    <figcaption class="flex gap-4 pt-1 text-xs text-mute"><span><i class="mr-1 inline-block h-0.5 w-4 bg-accent align-middle" />{{ t('stats.download') }}</span><span><i class="mr-1 inline-block h-0.5 w-4 bg-warn align-middle" />{{ t('stats.upload') }}</span></figcaption>
  </figure>
</template>
