<!-- Modello C: schermata Upload. Elenco degli utenti a sinistra (chi sta scaricando da te e chi e' in coda), dettaglio a destra (sul telefono si alternano) -->
<script setup lang="ts">
import { computed, onMounted, onUnmounted, ref, watch } from 'vue'
import { useI18n } from 'vue-i18n'
import { useShared } from '@/stores/shared'
import { bytes, speed } from '@/lib/format'
import Icon from '@/components/Icon.vue'
import TransfersSeg from '@/components/TransfersSeg.vue'
import type { UploadClient } from '@/types'

const { t } = useI18n()
const sh = useShared()
let timer: ReturnType<typeof setInterval>
onMounted(() => { void sh.loadUploads(); timer = setInterval(() => void sh.loadUploads(), 3000) })
onUnmounted(() => clearInterval(timer))
const id = (c: UploadClient) => c.user_hash + c.file_hash
const groups = computed(() => [{ k: 'uploading', list: [...sh.uploads.uploading].sort((a, b) => b.speed - a.speed) }, { k: 'waiting', list: sh.uploads.waiting }] as const)
const all = computed(() => [...groups.value[0].list, ...groups.value[1].list])
const selected = ref<string | null>(null)
const current = computed(() => all.value.find((c) => id(c) === selected.value) ?? null)
const state = computed(() => (current.value && sh.uploads.uploading.some((c) => id(c) === selected.value) ? 'uploading' : 'waiting'))
// sullo schermo largo il dettaglio e' sempre visibile: appena c'e' qualcuno si apre il primo
const wide = () => window.matchMedia('(min-width: 1024px)').matches
let picked = false
watch(all, (l) => { if (picked || !l.length) return; picked = true; if (!selected.value && wide()) selected.value = id(l[0]) }, { immediate: true })
</script>
<template>
  <div class="flex h-full min-h-0" tabindex="-1" @keydown.esc="selected = null">
    <section class="min-h-0 w-full flex-col border-r border-line bg-surface lg:flex lg:w-[26rem] lg:shrink-0" :class="current ? 'hidden' : 'flex'" :aria-label="t('uploads.title')">
      <div class="flex flex-wrap items-center justify-between gap-2 border-b border-line px-4 py-2">
        <TransfersSeg /><span class="font-mono text-[11px] uppercase tracking-wider text-mute tabular-nums">{{ speed(sh.uploads.speed) }} · {{ t('uploads.serving', sh.uploads.uploading.length) }}</span>
      </div>
      <div class="min-h-0 flex-1 overflow-y-auto">
        <div v-for="g in groups" :key="g.k">
          <h3 class="border-b border-line bg-sunk px-4 py-1.5 font-mono text-[11px] uppercase tracking-wider text-mute">{{ t('shared.group.' + g.k) }} <span class="tabular-nums">{{ g.list.length }}</span></h3>
          <p v-if="!g.list.length" class="border-b border-line px-4 py-4 text-center text-xs text-mute">{{ t('shared.nobody') }}</p>
          <ul v-else role="list">
            <li v-for="c in g.list" :key="id(c)">
              <button class="flex w-full flex-col gap-1 border-b border-line px-4 py-3 text-left hover:bg-sunk" :class="selected === id(c) ? '!bg-soft' : ''" @click="selected = selected === id(c) && !wide() ? null : id(c)">
                <span class="flex items-center justify-between gap-3"><span class="truncate text-sm font-medium">{{ c.name || t('detail.unknownUser') }}</span><span class="shrink-0 font-mono text-xs tabular-nums text-mute">{{ speed(c.speed) }}</span></span>
                <span class="line-clamp-1 break-words font-mono text-[11px] text-mute" :title="c.file_name">{{ c.file_name || '—' }}</span>
              </button>
            </li>
          </ul>
        </div>
      </div>
    </section>
    <section class="min-w-0 flex-1 flex-col overflow-y-auto bg-surface lg:flex" :class="current ? 'flex' : 'hidden'" :aria-label="t('detail.aria')">
      <template v-if="current">
        <header class="flex items-start gap-3 border-b border-line px-5 py-4">
          <div class="min-w-0 flex-1"><h2 class="break-words text-base font-semibold leading-snug">{{ current.name || t('detail.unknownUser') }}</h2>
            <p class="pt-0.5 text-xs font-medium" :class="state === 'uploading' ? 'text-ok' : 'text-warn'">{{ t('shared.group.' + state) }}</p></div>
          <button class="rounded-md p-2 text-mute hover:bg-sunk hover:text-fg lg:hidden" :aria-label="t('detail.close')" @click="selected = null"><Icon name="x" /></button>
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
      </template>
      <p v-else class="m-auto px-6 text-center text-sm text-mute">{{ t('skin.studio.pickClient') }}</p>
    </section>
  </div>
</template>
