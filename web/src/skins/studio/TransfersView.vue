<!-- Modello C: schermata Trasferimenti. Elenco compatto a sinistra, dettaglio a destra (sul telefono si alternano) -->
<script setup lang="ts">
import { onMounted, watch } from 'vue'
import { useI18n } from 'vue-i18n'
import { FILTERS, useDownloads } from '@/stores/downloads'
import { usePrefs } from '@/stores/prefs'
import { useUi } from '@/stores/ui'
import { bytes, percent, speed } from '@/lib/format'
import { statusInfo, toneClass } from '@/lib/labels'
import Icon from '@/components/Icon.vue'
import ProgressBar from '@/components/ProgressBar.vue'
import EmptyState from '@/components/EmptyState.vue'
import DownloadDetail from '@/components/DownloadDetail.vue'
import TransfersSeg from '@/components/TransfersSeg.vue'

const { t } = useI18n()
const dl = useDownloads(), prefs = usePrefs(), ui = useUi()
onMounted(() => { void prefs.loadCategories() })
// sullo schermo largo il dettaglio e' sempre visibile: appena c'e' un download si apre il primo
const wide = () => window.matchMedia('(min-width: 1024px)').matches
let picked = false
watch(() => dl.list, (l) => { if (picked || !l.length) return; picked = true; if (!dl.detail && wide()) dl.detail = l[0].hash }, { immediate: true })
</script>
<template>
  <div class="flex h-full min-h-0" tabindex="-1" @keydown.esc="dl.detail = null">
    <section class="min-h-0 w-full flex-col border-r border-line bg-surface lg:flex lg:w-[26rem] lg:shrink-0" :class="dl.detail ? 'hidden' : 'flex'" :aria-label="t('common.filesList')">
      <div class="flex flex-wrap items-center justify-between gap-2 border-b border-line px-4 py-2">
        <TransfersSeg /><span class="font-mono text-[11px] uppercase tracking-wider text-mute tabular-nums">{{ t('skin.studio.count', dl.all.length) }}</span>
      </div>
      <div class="flex flex-wrap items-center gap-x-1 gap-y-2 border-b border-line px-3 py-2 text-xs">
        <button v-for="f in FILTERS" :key="f.key" class="whitespace-nowrap rounded-md px-2.5 py-1" :class="dl.filter === f.key ? 'bg-soft font-semibold text-fg' : 'text-mute hover:bg-sunk'" @click="dl.filter = f.key">{{ t('filters.' + f.key) }} <span class="font-mono tabular-nums">{{ dl.counts[f.key] }}</span></button>
        <input v-model="dl.query" type="search" class="ml-auto min-w-0 basis-32 rounded-md border border-line bg-bg px-2.5 py-1 text-xs placeholder:text-mute" :placeholder="t('transfers.filterByName')" :aria-label="t('transfers.filterByName')" />
      </div>
      <p v-if="!dl.loaded" class="p-6 text-center text-xs text-mute" role="status">{{ t('transfers.loading') }}</p>
      <EmptyState v-else-if="!dl.all.length" icon="dl" :title="t('transfers.emptyTitle')" :text="t('transfers.emptyText')">
        <button class="inline-flex items-center gap-1.5 rounded-md bg-accent px-4 py-2 text-xs font-semibold text-onaccent" @click="ui.openAdd()"><Icon name="plus" />{{ t('transfers.addLink') }}</button>
      </EmptyState>
      <EmptyState v-else-if="!dl.list.length" icon="search" :title="t('transfers.noMatchTitle')" :text="t('transfers.noMatchText')">
        <button class="rounded-md border border-line px-4 py-2 text-xs hover:bg-sunk" @click="dl.filter = 'all'; dl.category = null; dl.query = ''">{{ t('transfers.showAll') }}</button>
      </EmptyState>
      <ul v-else class="min-h-0 flex-1 overflow-y-auto" role="list">
        <li v-for="d in dl.list" :key="d.hash">
          <button class="flex w-full flex-col gap-1.5 border-b border-line px-4 py-3 text-left hover:bg-sunk" :class="dl.detail === d.hash ? '!bg-soft' : ''" @click="dl.detail = dl.detail === d.hash && !wide() ? null : d.hash">
            <span class="flex items-center justify-between gap-3"><span class="line-clamp-2 break-words text-sm font-medium" :title="d.name">{{ d.name }}</span><span class="shrink-0 font-mono text-xs tabular-nums text-mute">{{ percent(d.percent) }}</span></span>
            <ProgressBar :value="d.percent" :tone="statusInfo(d).key === 'done' ? 'ok' : 'accent'" :label="d.name" />
            <span class="flex justify-between gap-2 font-mono text-[11px] tabular-nums text-mute"><span>{{ bytes(d.size, 2, true) }}</span><span :class="statusInfo(d).key === 'active' ? '' : toneClass(statusInfo(d).tone)">{{ statusInfo(d).key === 'active' ? speed(d.speed) : t(statusInfo(d).labelKey) }} · {{ d.sources_transferring }}/{{ d.sources }}</span></span>
          </button>
        </li>
      </ul>
    </section>
    <section class="min-w-0 flex-1 flex-col bg-surface lg:flex" :class="dl.detail ? 'flex' : 'hidden'" :aria-label="t('detail.aria')">
      <DownloadDetail v-if="dl.detail" :hash="dl.detail" @close="dl.detail = null" />
      <p v-else class="m-auto px-6 text-center text-sm text-mute">{{ t('skin.studio.pick') }}</p>
    </section>
  </div>
</template>
