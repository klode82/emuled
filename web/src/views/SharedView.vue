<script setup lang="ts">
import { onMounted } from 'vue'
import { useI18n } from 'vue-i18n'
import { useShared } from '@/stores/shared'
import { bytes, number } from '@/lib/format'
import { PRIORITIES, priorityKey } from '@/lib/labels'
import { useSort } from '@/lib/useSort'
import SortTh from '@/components/SortTh.vue'
import type { Priority } from '@/types'
import Icon from '@/components/Icon.vue'
import EmptyState from '@/components/EmptyState.vue'

const { t } = useI18n()
const sh = useShared()
const PRIO_RANK: Record<string, number> = { very_low: 0, low: 1, normal: 2, auto: 2, high: 3, release: 4 }
const files = useSort(() => sh.list, {
  name: { get: (f) => f.name }, size: { get: (f) => f.size, desc: true }, requests: { get: (f) => f.requests, desc: true }, sent: { get: (f) => f.transferred, desc: true },
  queued: { get: (f) => f.queued, desc: true }, priority: { get: (f) => PRIO_RANK[f.priority] ?? 2, desc: true },
})
onMounted(() => void sh.load())
</script>
<template>
  <div class="flex flex-col gap-4 p-4">
    <header class="flex flex-wrap items-center justify-between gap-3">
      <h1 class="text-lg font-semibold">{{ t('shared.title') }} <span class="text-sm font-normal tabular-nums text-mute">{{ sh.list.length }}</span></h1>
      <div class="flex flex-wrap gap-2">
        <RouterLink to="/prefs#dirs" class="inline-flex items-center gap-1.5 rounded-lg border border-line px-3.5 py-2 text-sm hover:bg-sunk"><Icon name="folder" />{{ t('shared.manageFolders') }}</RouterLink>
        <button class="inline-flex items-center gap-1.5 rounded-lg border border-line px-3.5 py-2 text-sm hover:bg-sunk" @click="sh.reload()"><Icon name="refresh" />{{ t('shared.recheck') }}</button>
      </div>
    </header>
      <EmptyState v-if="!sh.list.length && !sh.loading" icon="folder" :title="t('shared.emptyTitle')" :text="t('shared.emptyText')" />
      <div v-else class="overflow-x-auto rounded-xl border border-line bg-surface">
        <table class="w-full min-w-[46rem] text-left text-sm">
          <thead class="bg-sunk text-xs uppercase tracking-wide text-mute"><tr><SortTh :label="t('cols.name')" col="name" :sorter="files.sorter" /><SortTh :label="t('cols.size')" col="size" :sorter="files.sorter" right /><SortTh :label="t('cols.requests')" col="requests" :sorter="files.sorter" right /><SortTh :label="t('cols.sent')" col="sent" :sorter="files.sorter" right /><SortTh :label="t('cols.queued')" col="queued" :sorter="files.sorter" right /><SortTh :label="t('common.priority')" col="priority" :sorter="files.sorter" /></tr></thead>
          <tbody>
            <tr v-for="f in files.sorted.value" :key="f.hash" class="border-t border-line hover:bg-sunk">
              <td class="w-[46%] px-4 py-2.5"><p class="line-clamp-2 break-words font-medium leading-snug" :title="f.path">{{ f.name }}</p><p v-if="f.part_file" class="text-xs text-warn">{{ t('shared.downloading') }}</p></td>
              <td class="px-4 py-2.5 text-right tabular-nums">{{ bytes(f.size) }}</td>
              <td class="px-4 py-2.5 text-right tabular-nums">{{ number(f.requests) }} <span class="text-xs text-mute">({{ t('shared.accepted', { n: number(f.accepted) }) }})</span></td>
              <td class="px-4 py-2.5 text-right tabular-nums">{{ bytes(f.transferred) }}</td><td class="px-4 py-2.5 text-right tabular-nums">{{ number(f.queued) }}</td>
              <td class="px-4 py-2.5"><select class="rounded-md border border-line bg-bg px-2 py-1 text-xs" :value="f.priority" :aria-label="t('shared.priorityOf', { name: f.name })" @change="sh.setPriority(f.hash, ($event.target as HTMLSelectElement).value as Priority)"><option v-for="p in PRIORITIES" :key="p" :value="p">{{ t(priorityKey(p)) }}</option></select></td>
            </tr>
          </tbody>
        </table>
      </div>

  </div>
</template>
