<!-- Download state as a solid colored icon (noticeable even from afar); the text is in the tooltip and for screen readers -->
<script setup lang="ts">
import { computed } from 'vue'
import { useI18n } from 'vue-i18n'
import { statusInfo } from '@/lib/labels'
import type { Download } from '@/types'
import Icon from './Icon.vue'

const { t } = useI18n()
const props = defineProps<{ download: Download; size?: 'md' | 'lg' }>()
const info = computed(() => statusInfo(props.download))
// one shape and one color per state (classes are written in full so Tailwind generates them)
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
