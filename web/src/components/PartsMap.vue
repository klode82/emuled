<!-- Mappa dei blocchi (chunk) di un download: una cella per blocco, piena = presente, a meta' = parziale, vuota = mancante, rossa = da riscaricare -->
<script setup lang="ts">
import { computed } from 'vue'
import { useI18n } from 'vue-i18n'
import { bytes, percent } from '@/lib/format'
import type { Parts } from '@/types'

const { t } = useI18n()
const props = defineProps<{ parts: Parts; active?: number[] }>()
const isActive = (i: number) => !!props.active?.includes(i)
const cells = computed(() => props.parts.parts.map((p) => {
  const done = p.size ? 100 * (1 - p.missing / p.size) : 0
  return { p, done, title: `${t('parts.block', { n: p.index + 1 })} · ${bytes(p.size)} · ${percent(done)} · ${t('parts.sources', p.sources)}` }
}))
// riempimento a gradini di 1/8: classi Tailwind scritte per intero (cosi' il compilatore le genera), niente stile in linea
const FILL = ['h-0', 'h-[12.5%]', 'h-1/4', 'h-[37.5%]', 'h-1/2', 'h-[62.5%]', 'h-3/4', 'h-[87.5%]', 'h-full']
const fill = (done: number) => FILL[done >= 100 ? 8 : done <= 0 ? 0 : Math.max(1, Math.round(done / 12.5))]
const count = (s: string) => props.parts.parts.filter((p) => p.state === s).length
</script>
<template>
  <figure class="m-0">
    <div class="grid grid-cols-[repeat(auto-fill,minmax(10px,18px))] gap-[2px]" role="img" :aria-label="t('parts.aria', { complete: count('complete'), total: parts.count })">
      <span v-for="c in cells" :key="c.p.index" :title="c.title" class="relative aspect-square overflow-hidden rounded-[2px]"
        :class="c.p.state === 'corrupted' ? 'bg-bad' : c.p.sources === 0 && c.p.state !== 'complete' ? 'bg-sunk ring-1 ring-inset ring-line' : 'bg-sunk'">
        <span v-if="c.p.state !== 'corrupted'" class="absolute inset-x-0 bottom-0 bg-accent" :class="fill(c.done)" />
        <span v-if="isActive(c.p.index)" class="absolute inset-0 rounded-[2px] ring-2 ring-inset ring-warn" />
      </span>
    </div>
    <figcaption class="flex flex-wrap gap-x-4 gap-y-1 pt-2 text-xs text-mute">
      <span><i class="mr-1 inline-block size-2 rounded-[2px] bg-accent align-middle" />{{ t('parts.complete', count('complete')) }}</span>
      <span><i class="mr-1 inline-block size-2 rounded-[2px] bg-sunk ring-1 ring-inset ring-line align-middle" />{{ t('parts.missing', count('empty') + count('partial')) }}</span>
      <span v-if="active?.length" class="text-warn"><i class="mr-1 inline-block size-2 rounded-[2px] ring-2 ring-inset ring-warn align-middle" />{{ t('parts.active', active.length) }}</span>
      <span v-if="count('corrupted')" class="text-bad"><i class="mr-1 inline-block size-2 rounded-[2px] bg-bad align-middle" />{{ t('parts.corrupted', count('corrupted')) }}</span>
      <span class="ml-auto">{{ t('parts.size', { size: bytes(parts.part_size) }) }}</span>
    </figcaption>
  </figure>
</template>
