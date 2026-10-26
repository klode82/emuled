<script setup lang="ts">
import { onMounted, onUnmounted } from 'vue'
import { useI18n } from 'vue-i18n'
import { useShared } from '@/stores/shared'
import { bytes, speed } from '@/lib/format'
import { useSort } from '@/lib/useSort'
import SortTh from '@/components/SortTh.vue'
import TransfersTabs from '@/components/TransfersTabs.vue'

const { t } = useI18n()
const sh = useShared()
const clientCols = {
  user: { get: (c: { name: string }) => c.name || '' }, software: { get: (c: { software: string }) => c.software || '' }, file: { get: (c: { file_name?: string }) => c.file_name || '' },
  speed: { get: (c: { speed: number }) => c.speed, desc: true }, sent: { get: (c: { session_uploaded: number }) => c.session_uploaded, desc: true },
}
const up = useSort(() => sh.uploads.uploading, clientCols, '', 1, 'uploads-active')
const wait = useSort(() => sh.uploads.waiting, clientCols, '', 1, 'uploads-waiting')
let timer: ReturnType<typeof setInterval>
onMounted(() => { void sh.loadUploads(); timer = setInterval(() => void sh.loadUploads(), 3000) })
onUnmounted(() => clearInterval(timer))
</script>
<template>
  <div class="flex h-full min-h-0 flex-col">
    <TransfersTabs />
    <div class="flex min-h-0 flex-1 flex-col gap-4 overflow-y-auto p-4">
      <header class="flex flex-wrap items-baseline justify-between gap-3">
        <h1 class="text-base font-semibold">{{ t('uploads.title') }}</h1>
        <p class="text-xs text-mute tabular-nums">{{ t('shared.uploadSpeed', { speed: speed(sh.uploads.speed) }) }} · {{ t('uploads.serving', sh.uploads.uploading.length) }}</p>
      </header>
      <section v-for="g in [{ k: 'uploading', list: up.sorted.value, so: up.sorter }, { k: 'waiting', list: wait.sorted.value, so: wait.sorter }]" :key="g.k">
        <h2 class="pb-2 text-xs font-semibold">{{ t('shared.group.' + g.k) }} <span class="tabular-nums font-normal text-mute">{{ g.list.length }}</span></h2>
        <p v-if="!g.list.length" class="rounded-xl border border-line bg-surface px-4 py-6 text-center text-xs text-mute">{{ t('shared.nobody') }}</p>
        <div v-else class="overflow-x-auto rounded-xl border border-line bg-surface">
          <table class="w-full min-w-[40rem] text-left text-xs">
            <thead class="bg-sunk text-xs uppercase tracking-wide text-mute"><tr><SortTh :label="t('cols.user')" col="user" :sorter="g.so" /><SortTh :label="t('cols.software')" col="software" :sorter="g.so" /><SortTh :label="t('cols.file')" col="file" :sorter="g.so" /><SortTh :label="t('cols.speed')" col="speed" :sorter="g.so" right /><SortTh :label="t('cols.sent')" col="sent" :sorter="g.so" right /></tr></thead>
            <tbody><tr v-for="c in g.list" :key="c.user_hash + c.file_hash" class="border-t border-line hover:bg-sunk">
              <td class="px-4 py-2">{{ c.name || t('detail.unknownUser') }}<p class="font-mono text-xs text-mute">{{ c.ip }}:{{ c.port }}</p></td><td class="px-4 py-2 text-mute">{{ c.software || '—' }}</td>
              <td class="w-[40%] px-4 py-2"><p class="line-clamp-2 break-words">{{ c.file_name || '—' }}</p></td><td class="px-4 py-2 text-right tabular-nums">{{ speed(c.speed) }}</td><td class="px-4 py-2 text-right tabular-nums">{{ bytes(c.session_uploaded) }}</td></tr></tbody>
          </table>
        </div>
      </section>
    </div>
  </div>
</template>
