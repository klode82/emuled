<!-- Modello B: schermata Upload. Stesse tessere di riepilogo di Download e una scheda per ogni utente (chi sta scaricando da te, chi e' in coda) -->
<script setup lang="ts">
import { computed, onMounted, onUnmounted } from 'vue'
import { useI18n } from 'vue-i18n'
import { useShared } from '@/stores/shared'
import { bytes, speed } from '@/lib/format'
import Tiles from './Tiles.vue'
import TransfersSeg from '@/components/TransfersSeg.vue'

const { t } = useI18n()
const sh = useShared()
let timer: ReturnType<typeof setInterval>
onMounted(() => { void sh.loadUploads(); timer = setInterval(() => void sh.loadUploads(), 3000) })
onUnmounted(() => clearInterval(timer))
const groups = computed(() => [{ k: 'uploading', list: [...sh.uploads.uploading].sort((a, b) => b.speed - a.speed) }, { k: 'waiting', list: sh.uploads.waiting }] as const)
</script>
<template>
  <div class="flex flex-col gap-5 p-4 md:p-6">
    <Tiles />
    <section class="flex flex-col gap-4">
      <div class="flex flex-wrap items-center gap-x-3 gap-y-2">
        <h2 class="text-base font-bold">{{ t('uploads.title') }}</h2><TransfersSeg />
        <p class="ml-auto text-xs text-mute tabular-nums">{{ t('shared.uploadSpeed', { speed: speed(sh.uploads.speed) }) }} · {{ t('uploads.serving', sh.uploads.uploading.length) }}</p>
      </div>
      <div v-for="g in groups" :key="g.k">
        <h3 class="pb-2 text-xs font-semibold uppercase tracking-wide text-mute">{{ t('shared.group.' + g.k) }} <span class="font-normal tabular-nums">{{ g.list.length }}</span></h3>
        <p v-if="!g.list.length" class="rounded-2xl border border-line bg-surface px-4 py-6 text-center text-xs text-mute">{{ t('shared.nobody') }}</p>
        <div v-else class="grid gap-3 md:grid-cols-2 xl:grid-cols-3">
          <article v-for="c in g.list" :key="c.user_hash + c.file_hash" class="flex flex-col gap-3 rounded-2xl border border-line bg-surface p-4">
            <div class="min-w-0"><h4 class="truncate text-sm font-semibold" :title="c.name">{{ c.name || t('detail.unknownUser') }}</h4>
              <p class="truncate text-xs text-mute">{{ c.software || '—' }} · <span class="font-mono">{{ c.ip }}:{{ c.port }}</span></p></div>
            <p class="line-clamp-2 break-words text-sm" :title="c.file_name">{{ c.file_name || '—' }}</p>
            <div class="grid grid-cols-2 gap-2 rounded-xl bg-sunk p-2 text-center text-xs">
              <div><p class="font-semibold tabular-nums">{{ speed(c.speed) }}</p><p class="text-mute">{{ t('detail.speed') }}</p></div>
              <div><p class="font-semibold tabular-nums">{{ bytes(c.session_uploaded) }}</p><p class="text-mute">{{ t('cols.sent') }}</p></div>
            </div>
            <span class="self-start rounded-full bg-sunk px-2.5 py-1 text-xs font-semibold" :class="g.k === 'uploading' ? 'text-ok' : 'text-warn'">{{ t('shared.group.' + g.k) }}</span>
          </article>
        </div>
      </div>
    </section>
  </div>
</template>
