<!-- Mappa dei blocchi (chunk) di un download: una cella per blocco. Pieno = presente (a meta' tinta = completo ma non verificabile), riempito a meta' = parziale, vuoto = mancante;
     sfondo giallo = raro (1-2 fonti), bordo grigio = senza fonti, rosso = da riscaricare; riquadro arancione = in scarico adesso (pieno = in ricezione, chiaro = richiesto). Tutto spiegato nella legenda. -->
<script setup lang="ts">
import { computed } from 'vue'
import { useI18n } from 'vue-i18n'
import { bytes, percent } from '@/lib/format'
import type { Part, Parts } from '@/types'

const { t } = useI18n()
const props = defineProps<{ parts: Parts; active?: { part: number; receiving: boolean }[] }>()
type Kind = 'complete' | 'unverified' | 'partial' | 'empty' | 'corrupted'
type Avail = 'ok' | 'rare' | 'none'
const kindOf = (p: Part): Kind => (p.state === 'corrupted' ? 'corrupted' : p.state === 'complete' ? (p.verified === false ? 'unverified' : 'complete') : p.state === 'partial' ? 'partial' : 'empty')
// disponibilita' (solo per i blocchi che mancano): nessuna fonte, una o due (raro), altrimenti normale
const availOf = (p: Part): Avail => (p.state === 'complete' || p.state === 'corrupted' ? 'ok' : p.sources === 0 ? 'none' : p.sources <= 2 ? 'rare' : 'ok')
const act = computed(() => new Map((props.active ?? []).map((a) => [a.part, a.receiving])))
const cells = computed(() => props.parts.parts.map((p) => {
  const done = p.size ? 100 * (1 - p.missing / p.size) : 0, kind = kindOf(p), avail = availOf(p), now = act.value.get(p.index)
  const tags = [now === undefined ? '' : t(now ? 'parts.tag.receiving' : 'parts.tag.requested'), kind === 'corrupted' ? t('parts.tag.corrupted') : kind === 'unverified' ? t('parts.tag.unverified') : '', avail === 'none' ? t('parts.tag.noSources') : avail === 'rare' ? t('parts.tag.rare') : ''].filter(Boolean)
  return { p, done, kind, avail, now, title: [`${t('parts.block', { n: p.index + 1 })} · ${bytes(p.size)} · ${percent(done)} · ${t('parts.sources', p.sources)}`, ...tags].join(' · ') }
}))
// riempimento a gradini di 1/8: classi Tailwind scritte per intero (cosi' il compilatore le genera), niente stile in linea
const FILL = ['h-0', 'h-[12.5%]', 'h-1/4', 'h-[37.5%]', 'h-1/2', 'h-[62.5%]', 'h-3/4', 'h-[87.5%]', 'h-full']
const fill = (done: number) => FILL[done >= 100 ? 8 : done <= 0 ? 0 : Math.max(1, Math.round(done / 12.5))]
const n = computed(() => ({
  complete: cells.value.filter((c) => c.kind === 'complete').length, unverified: cells.value.filter((c) => c.kind === 'unverified').length,
  partial: cells.value.filter((c) => c.kind === 'partial').length, empty: cells.value.filter((c) => c.kind === 'empty').length, corrupted: cells.value.filter((c) => c.kind === 'corrupted').length,
  rare: cells.value.filter((c) => c.avail === 'rare').length, none: cells.value.filter((c) => c.avail === 'none').length,
  receiving: cells.value.filter((c) => c.now === true).length, requested: cells.value.filter((c) => c.now === false).length,
}))
</script>
<template>
  <figure class="m-0">
    <div class="grid grid-cols-[repeat(auto-fill,minmax(10px,18px))] gap-[2px]" role="img" :aria-label="t('parts.aria', { complete: n.complete + n.unverified, total: parts.count })">
      <span v-for="c in cells" :key="c.p.index" :title="c.title" class="relative aspect-square overflow-hidden rounded-[2px]"
        :class="c.kind === 'corrupted' ? 'bg-bad' : 'bg-sunk'">
        <!-- i colori sono variabili CSS: la trasparenza si fa con `opacity` su strati sovrapposti, non con /50 -->
        <span v-if="c.avail === 'rare'" class="absolute inset-0 bg-warn opacity-25" />
        <span v-if="c.avail === 'none'" class="absolute inset-0 rounded-[2px] ring-1 ring-inset ring-mute opacity-60" />
        <span v-if="c.kind !== 'corrupted'" class="absolute inset-x-0 bottom-0 bg-accent" :class="[c.kind === 'unverified' ? 'opacity-50' : '', fill(c.done)]" />
        <span v-if="c.now !== undefined" class="absolute inset-0 rounded-[2px] ring-2 ring-inset ring-warn" :class="c.now ? '' : 'opacity-50'" />
      </span>
    </div>
    <figcaption class="flex flex-wrap gap-x-4 gap-y-1 pt-2 text-xs text-mute">
      <span><i class="mr-1 inline-block size-2 rounded-[2px] bg-accent align-middle" />{{ t('parts.complete', n.complete) }}</span>
      <span v-if="n.unverified" :title="t('parts.unverifiedHint')"><i class="relative mr-1 inline-block size-2 rounded-[2px] bg-sunk align-middle"><i class="absolute inset-0 rounded-[2px] bg-accent opacity-50" /></i>{{ t('parts.unverified', n.unverified) }}</span>
      <span v-if="n.partial"><i class="relative mr-1 inline-block size-2 overflow-hidden rounded-[2px] bg-sunk align-middle"><i class="absolute inset-x-0 bottom-0 h-1/2 bg-accent" /></i>{{ t('parts.partial', n.partial) }}</span>
      <span><i class="mr-1 inline-block size-2 rounded-[2px] bg-sunk align-middle" />{{ t('parts.missing', n.empty) }}</span>
      <span v-if="n.rare" :title="t('parts.rareHint')"><i class="relative mr-1 inline-block size-2 rounded-[2px] bg-sunk align-middle"><i class="absolute inset-0 rounded-[2px] bg-warn opacity-25" /></i>{{ t('parts.rare', n.rare) }}</span>
      <span v-if="n.none"><i class="relative mr-1 inline-block size-2 rounded-[2px] bg-sunk align-middle"><i class="absolute inset-0 rounded-[2px] ring-1 ring-inset ring-mute opacity-60" /></i>{{ t('parts.noSources', n.none) }}</span>
      <span v-if="n.corrupted" class="text-bad"><i class="mr-1 inline-block size-2 rounded-[2px] bg-bad align-middle" />{{ t('parts.corrupted', n.corrupted) }}</span>
      <span v-if="n.receiving" class="text-warn"><i class="mr-1 inline-block size-2 rounded-[2px] ring-2 ring-inset ring-warn align-middle" />{{ t('parts.receiving', n.receiving) }}</span>
      <span v-if="n.requested" class="text-warn"><i class="relative mr-1 inline-block size-2 rounded-[2px] align-middle"><i class="absolute inset-0 rounded-[2px] ring-2 ring-inset ring-warn opacity-50" /></i>{{ t('parts.requested', n.requested) }}</span>
      <span class="ml-auto">{{ t('parts.size', { size: bytes(parts.part_size) }) }}</span>
    </figcaption>
  </figure>
</template>
