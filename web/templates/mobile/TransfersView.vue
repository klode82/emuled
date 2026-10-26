<!-- Modello D: schermata Trasferimenti. Righe con anello; il dettaglio e' un foglio dal basso sul telefono e una colonna a destra sullo schermo largo -->
<script setup lang="ts">
import { onMounted } from 'vue'
import { useI18n } from 'vue-i18n'
import { FILTERS, useDownloads } from '@/stores/downloads'
import { usePrefs } from '@/stores/prefs'
import { useUi } from '@/stores/ui'
import { bytes, speed } from '@/lib/format'
import { statusInfo, toneClass } from '@/lib/labels'
import Icon from '@/components/Icon.vue'
import Ring from '@/components/Ring.vue'
import EmptyState from '@/components/EmptyState.vue'
import DownloadDetail from '@/components/DownloadDetail.vue'

const { t } = useI18n()
const dl = useDownloads(), prefs = usePrefs(), ui = useUi()
onMounted(() => { void prefs.loadCategories() })
</script>
<template>
  <div class="flex h-full min-h-0" tabindex="-1" @keydown.esc="dl.detail = null">
    <div class="flex min-h-0 min-w-0 flex-1 flex-col">
      <div class="flex shrink-0 items-center gap-2 overflow-x-auto px-4 pb-2 text-xs md:px-6" role="tablist" :aria-label="t('transfers.filter')">
        <button v-for="f in FILTERS" :key="f.key" role="tab" :aria-selected="dl.filter === f.key" class="whitespace-nowrap rounded-full px-3 py-1.5 font-semibold" :class="dl.filter === f.key ? 'bg-soft text-accent' : 'text-mute hover:bg-sunk'" @click="dl.filter = f.key">{{ t('filters.' + f.key) }} <span class="tabular-nums">{{ dl.counts[f.key] }}</span></button>
      </div>
      <p v-if="!dl.loaded" class="p-8 text-center text-xs text-mute" role="status">{{ t('transfers.loading') }}</p>
      <EmptyState v-else-if="!dl.all.length" icon="dl" :title="t('transfers.emptyTitle')" :text="t('transfers.emptyText')">
        <button class="inline-flex items-center gap-1.5 rounded-full bg-accent px-4 py-2 text-xs font-bold text-onaccent" @click="ui.openAdd()"><Icon name="plus" />{{ t('transfers.addLink') }}</button>
      </EmptyState>
      <EmptyState v-else-if="!dl.list.length" icon="search" :title="t('transfers.noMatchTitle')" :text="t('transfers.noMatchText')">
        <button class="rounded-full border border-line px-4 py-2 text-xs hover:bg-sunk" @click="dl.filter = 'all'; dl.category = null; dl.query = ''">{{ t('transfers.showAll') }}</button>
      </EmptyState>
      <ul v-else class="min-h-0 flex-1 overflow-y-auto px-2 pb-24 md:px-4 lg:pb-4" role="list">
        <li v-for="d in dl.list" :key="d.hash">
          <button class="flex w-full items-center gap-3 rounded-2xl p-3 text-left hover:bg-sunk" :class="dl.detail === d.hash ? 'lg:!bg-soft' : ''" @click="dl.detail = dl.detail === d.hash ? null : d.hash">
            <Ring :value="d.percent" :size="48" :tone="statusInfo(d).key === 'done' ? 'ok' : 'accent'" />
            <span class="min-w-0 flex-1"><span class="line-clamp-2 break-words text-[15px] font-semibold leading-snug" :title="d.name">{{ d.name }}</span>
              <span class="block text-xs tabular-nums text-mute"><template v-if="statusInfo(d).key === 'active'">{{ speed(d.speed) }} · </template><span :class="toneClass(statusInfo(d).tone)">{{ t(statusInfo(d).labelKey) }}</span> · {{ bytes(d.size, 2, true) }}<template v-if="d.completed < d.size && statusInfo(d).key !== 'done'"> · <span :title="t('transfers.left', { size: bytes(d.size - d.completed, 2, true) })">−{{ bytes(d.size - d.completed, 2, true) }}</span></template></span></span>
            <Icon name="chevron" class="!size-4 text-mute" />
          </button>
        </li>
      </ul>
    </div>
    <template v-if="dl.detail">
      <div class="fixed inset-0 z-20 bg-black/40 lg:hidden" @click="dl.detail = null"></div>
      <aside class="fixed inset-x-0 bottom-0 z-30 h-[85dvh] overflow-hidden rounded-t-3xl bg-surface shadow-2xl lg:static lg:z-auto lg:h-auto lg:w-[24rem] lg:shrink-0 lg:rounded-none lg:border-l lg:border-line lg:shadow-none" :aria-label="t('detail.aria')">
        <DownloadDetail :hash="dl.detail" @close="dl.detail = null" />
      </aside>
    </template>
  </div>
</template>
