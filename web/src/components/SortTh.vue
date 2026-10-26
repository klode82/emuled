<!-- Intestazione di colonna cliccabile: ordina la tabella (aria-sort per i lettori di schermo) -->
<script setup lang="ts">
import { computed } from 'vue'
import { useI18n } from 'vue-i18n'
import type { Sorter } from '@/lib/useSort'
import Icon from './Icon.vue'
const { t } = useI18n()
const props = defineProps<{ label: string; col: string; sorter: Sorter; right?: boolean }>()
const active = computed(() => props.sorter.key.value === props.col)
</script>
<template>
  <th class="px-3 py-2 font-medium" :class="right ? 'text-right' : ''" :aria-sort="active ? (sorter.dir.value === 1 ? 'ascending' : 'descending') : 'none'">
    <button type="button" class="inline-flex items-center gap-1 uppercase tracking-wide hover:text-fg" :class="[right ? 'flex-row-reverse' : '', active ? 'text-fg' : '']" :title="t('common.sortBy', { col: label })" @click="sorter.toggle(col)">
      <span>{{ label }}</span><Icon v-if="active" :name="sorter.dir.value === 1 ? 'sortup' : 'sortdown'" class="!size-3.5" />
    </button>
  </th>
</template>
