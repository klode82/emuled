<!-- Aggiungi download: uno o piu' link ed2k (uno per riga), categoria -->
<script setup lang="ts">
import { computed, ref, watch } from 'vue'
import { useI18n } from 'vue-i18n'
import { useUi } from '@/stores/ui'
import { useDownloads } from '@/stores/downloads'
import { usePrefs } from '@/stores/prefs'
import { extractLinks } from '@/lib/format'
import Modal from './Modal.vue'
const { t } = useI18n()
const ui = useUi(), dl = useDownloads(), prefs = usePrefs()
const text = ref(''), cat = ref(0), busy = ref(false)
watch(() => ui.addOpen, (v) => { if (v) { text.value = ui.addText; cat.value = 0; if (!prefs.categories.length) void prefs.loadCategories() } })
const links = computed(() => extractLinks(text.value))
async function submit() {
  if (!links.value.length) return
  busy.value = true
  const n = await dl.addLinks(links.value, cat.value)
  busy.value = false
  if (n) ui.addOpen = false
}
</script>
<template>
  <Modal :open="ui.addOpen" :title="t('add.title')" wide @close="ui.addOpen = false">
    <form class="flex flex-col gap-4" @submit.prevent="submit">
      <label class="flex flex-col gap-1.5 text-sm font-medium">{{ t('add.links') }}
        <textarea v-model="text" rows="5" class="rounded-lg border border-line bg-bg px-3 py-2 font-mono text-xs font-normal" placeholder="ed2k://|file|name.pdf|7743600|B2C072C57F57A9A6AEECB50390DC27F8|/" autofocus />
      </label>
      <p class="text-xs" :class="links.length || !text.trim() ? 'text-mute' : 'text-bad'">
        {{ !text.trim() ? t('add.hint') : links.length ? t('add.recognized', links.length) : t('add.none') }}
      </p>
      <label v-if="prefs.categories.length > 1" class="flex flex-col gap-1.5 text-sm font-medium">{{ t('common.category') }}
        <select v-model.number="cat" class="rounded-lg border border-line bg-bg px-3 py-2 text-sm font-normal"><option v-for="c in prefs.categories" :key="c.id" :value="c.id">{{ c.id === 0 ? t('common.noCategory') : c.title }}</option></select>
      </label>
    </form>
    <template #footer>
      <button class="rounded-lg border border-line px-4 py-2 text-sm hover:bg-sunk" @click="ui.addOpen = false">{{ t('common.cancel') }}</button>
      <button class="rounded-lg bg-accent px-4 py-2 text-sm font-medium text-onaccent" :disabled="!links.length || busy" @click="submit">{{ busy ? t('add.adding') : t('common.add') }}</button>
    </template>
  </Modal>
</template>
