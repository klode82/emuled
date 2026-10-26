<!-- Anello di avanzamento con la percentuale al centro (modelli B e D) -->
<script setup lang="ts">
import { computed } from 'vue'
import { percent } from '@/lib/format'
const props = withDefaults(defineProps<{ value: number; size?: number; tone?: 'accent' | 'ok' }>(), { size: 48, tone: 'accent' })
const r = computed(() => (props.size - 6) / 2)
const c = computed(() => 2 * Math.PI * r.value)
const dash = computed(() => `${((c.value * Math.min(100, Math.max(0, props.value))) / 100).toFixed(1)} ${c.value.toFixed(1)}`)
</script>
<template>
  <span class="relative inline-grid shrink-0 place-items-center" role="img" :aria-label="percent(value)">
    <svg :width="size" :height="size" :viewBox="`0 0 ${size} ${size}`" class="-rotate-90" aria-hidden="true">
      <circle :cx="size / 2" :cy="size / 2" :r="r" fill="none" class="stroke-sunk" stroke-width="5" />
      <circle :cx="size / 2" :cy="size / 2" :r="r" fill="none" :class="tone === 'ok' ? 'stroke-ok' : 'stroke-accent'" stroke-width="5" :stroke-linecap="value > 0 ? 'round' : 'butt'" :stroke-dasharray="dash" />
    </svg>
    <span class="absolute font-bold tabular-nums" :class="size >= 64 ? 'text-sm' : 'text-[10px]'">{{ Math.round(value) }}<template v-if="size >= 64">%</template></span>
  </span>
</template>
