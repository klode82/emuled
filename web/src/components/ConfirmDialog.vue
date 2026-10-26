<script setup lang="ts">
import { useI18n } from 'vue-i18n'
import { useUi } from '@/stores/ui'
import Modal from './Modal.vue'
const { t } = useI18n()
const ui = useUi()
</script>
<template>
  <Modal :open="!!ui.confirmReq" :title="ui.confirmReq?.title ?? ''" @close="ui.confirmReq?.resolve(false)">
    <p class="text-sm text-mute">{{ ui.confirmReq?.text }}</p>
    <ul v-if="ui.confirmReq?.list?.length" class="mt-3 max-h-56 divide-y divide-line overflow-y-auto rounded-lg border border-line bg-sunk text-xs" :aria-label="t('common.filesList')">
      <li v-for="(it, i) in ui.confirmReq.list" :key="i" class="flex items-baseline justify-between gap-3 px-3 py-1.5"><span class="min-w-0 break-words">{{ it.label }}</span><span v-if="it.info" class="shrink-0 tabular-nums text-mute">{{ it.info }}</span></li>
    </ul>
    <template #footer>
      <button class="rounded-lg border border-line px-4 py-2 text-sm hover:bg-sunk" @click="ui.confirmReq?.resolve(false)">{{ t('common.cancel') }}</button>
      <button class="rounded-lg px-4 py-2 text-sm font-medium" :class="ui.confirmReq?.danger ? 'bg-bad text-white' : 'bg-accent text-onaccent'" @click="ui.confirmReq?.resolve(true)">{{ ui.confirmReq?.confirm }}</button>
    </template>
  </Modal>
</template>
