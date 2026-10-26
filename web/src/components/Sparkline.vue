<!-- Andamento della velocita' (area + linea) per le tessere del Cruscotto -->
<script setup lang="ts">
import { computed } from 'vue'
import type { Sample } from '@/stores/core'
const props = defineProps<{ samples: Sample[]; kind: 'down' | 'up' }>()
const W = 120, H = 32
const pts = computed(() => {
  const v = props.samples.map((s) => s[props.kind]); if (v.length < 2) return ''
  const max = Math.max(...v, 1)
  return v.map((x, i) => `${((i * W) / (v.length - 1)).toFixed(1)},${(H - 1 - (x / max) * (H - 3)).toFixed(1)}`).join(' ')
})
</script>
<template>
  <svg :viewBox="`0 0 ${W} ${H}`" class="h-8 w-full" preserveAspectRatio="none" aria-hidden="true">
    <polyline v-if="pts" :points="`${pts} ${W},${H} 0,${H}`" class="fill-soft stroke-none" />
    <polyline v-if="pts" :points="pts" class="fill-none stroke-accent stroke-[1.6]" vector-effect="non-scaling-stroke" />
  </svg>
</template>
