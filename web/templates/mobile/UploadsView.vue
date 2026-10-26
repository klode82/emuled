<!-- Modello D: schermata Upload. Righe come in Download (chi sta scaricando da te, chi e' in coda); il dettaglio e' un foglio dal basso sul telefono e una colonna a destra sullo schermo largo -->
<script setup lang="ts">
import { computed, onMounted, onUnmounted, ref } from 'vue'
import { useI18n } from 'vue-i18n'
import { useShared } from '@/stores/shared'
import { bytes, speed } from '@/lib/format'
import Icon from '@/components/Icon.vue'
import type { UploadClient } from '@/types'

const { t } = useI18n()
const sh = useShared()
let timer: ReturnType<typeof setInterval>
onMounted(() => { void sh.loadUploads(); timer = setInterval(() => void sh.loadUploads(), 3000) })
onUnmounted(() => clearInterval(timer))
const id = (c: UploadClient) => c.user_hash + c.file_hash
const groups = computed(() => [{ k: 'uploading', list: [...sh.uploads.uploading].sort((a, b) => b.speed - a.speed) }, { k: 'waiting', list: sh.uploads.waiting }] as const)
const selected = ref<string | null>(null)
const current = computed(() => [...sh.uploads.uploading, ...sh.uploads.waiting].find((c) => id(c) === selected.value) ?? null)
const state = computed(() => (current.value && sh.uploads.uploading.some((c) => id(c) === selected.value) ? 'uploading' : 'waiting'))
</script>
<template>
  <div class="flex h-full min-h-0" tabindex="-1" @keydown.esc="selected = null">
    <div class="flex min-h-0 min-w-0 flex-1 flex-col">
      <p class="shrink-0 px-4 pb-2 text-xs text-mute tabular-nums md:px-6">{{ t('shared.uploadSpeed', { speed: speed(sh.uploads.speed) }) }} · {{ t('uploads.serving', sh.uploads.uploading.length) }}</p>
      <div class="min-h-0 flex-1 overflow-y-auto px-2 pb-24 md:px-4 lg:pb-4">
        <section v-for="g in groups" :key="g.k" class="pb-2">
          <h2 class="px-3 pb-1 pt-2 text-xs font-bold uppercase tracking-wide text-mute">{{ t('shared.group.' + g.k) }} <span class="font-normal tabular-nums">{{ g.list.length }}</span></h2>
          <p v-if="!g.list.length" class="px-3 py-4 text-xs text-mute">{{ t('shared.nobody') }}</p>
          <ul v-else role="list">
            <li v-for="c in g.list" :key="id(c)">
              <button class="flex w-full items-center gap-3 rounded-2xl p-3 text-left hover:bg-sunk" :class="selected === id(c) ? 'lg:!bg-soft' : ''" @click="selected = selected === id(c) ? null : id(c)">
                <span class="grid size-12 shrink-0 place-items-center rounded-full" :class="g.k === 'uploading' ? 'bg-soft text-accent' : 'bg-sunk text-mute'"><Icon name="up" class="!size-5" /></span>
                <span class="min-w-0 flex-1"><span class="block truncate text-[15px] font-semibold">{{ c.name || t('detail.unknownUser') }}</span>
                  <span class="line-clamp-1 break-words text-xs text-mute" :title="c.file_name">{{ c.file_name || '—' }}</span>
                  <span class="block text-xs tabular-nums text-mute"><template v-if="g.k === 'uploading'">{{ speed(c.speed) }} · </template>{{ t('cols.sent') }} {{ bytes(c.session_uploaded) }}</span></span>
                <Icon name="chevron" class="!size-4 text-mute" />
              </button>
            </li>
          </ul>
        </section>
      </div>
    </div>
    <template v-if="current">
      <div class="fixed inset-0 z-20 bg-black/40 lg:hidden" @click="selected = null"></div>
      <aside class="fixed inset-x-0 bottom-0 z-30 max-h-[85dvh] overflow-y-auto rounded-t-3xl bg-surface shadow-2xl lg:static lg:z-auto lg:max-h-none lg:w-[24rem] lg:shrink-0 lg:rounded-none lg:border-l lg:border-line lg:shadow-none" :aria-label="t('detail.aria')">
        <header class="flex items-start gap-3 border-b border-line px-5 py-4">
          <div class="min-w-0 flex-1"><h2 class="break-words text-lg font-bold leading-snug">{{ current.name || t('detail.unknownUser') }}</h2>
            <p class="pt-0.5 text-xs font-semibold" :class="state === 'uploading' ? 'text-ok' : 'text-warn'">{{ t('shared.group.' + state) }}</p></div>
          <button class="rounded-full p-2 text-mute hover:bg-sunk hover:text-fg" :aria-label="t('detail.close')" @click="selected = null"><Icon name="x" /></button>
        </header>
        <dl class="grid grid-cols-[auto_minmax(0,1fr)] gap-x-4 gap-y-2 p-5 text-xs">
          <dt class="text-right text-mute">{{ t('cols.software') }}</dt><dd>{{ current.software || '—' }}</dd>
          <dt class="text-right text-mute">{{ t('cols.address') }}</dt><dd class="font-mono">{{ current.ip }}:{{ current.port }}</dd>
          <dt class="text-right text-mute">{{ t('skin.studio.userHash') }}</dt><dd class="break-all font-mono">{{ current.user_hash }}</dd>
          <dt class="text-right text-mute">{{ t('cols.file') }}</dt><dd class="break-words">{{ current.file_name || '—' }}</dd>
          <dt class="text-right text-mute">{{ t('detail.hash') }}</dt><dd class="break-all font-mono">{{ current.file_hash }}</dd>
          <dt class="text-right text-mute">{{ t('detail.speed') }}</dt><dd class="tabular-nums">{{ speed(current.speed) }}</dd>
          <dt class="text-right text-mute">{{ t('cols.sent') }}</dt><dd class="tabular-nums">{{ bytes(current.session_uploaded) }}</dd>
        </dl>
      </aside>
    </template>
  </div>
</template>
