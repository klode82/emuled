<!-- Finestra "scegli una cartella" (Bench 874): mostra le cartelle del computer dove gira il DEMONE (non quelle del PC del browser; con Docker: quelle del contenitore) -->
<script setup lang="ts">
import { computed, ref, watch } from 'vue'
import { useI18n } from 'vue-i18n'
import { useSession } from '@/stores/session'
import { errText } from '@/stores/toasts'
import { bytes } from '@/lib/format'
import type { FsList } from '@/types'
import Modal from './Modal.vue'
import Icon from './Icon.vue'

const props = defineProps<{ open: boolean; start?: string; needWrite?: boolean; showHidden?: boolean }>()		// showHidden: si apre gia' con le cartelle nascoste (es. ~/.emuled.old)
const emit = defineEmits<{ close: []; pick: [path: string] }>()
const { t } = useI18n()
const session = useSession()
const data = ref<FsList | null>(null), error = ref(''), busy = ref(false), hidden = ref(false), newName = ref('')

async function load(path?: string) {
  busy.value = true; error.value = ''
  try { data.value = await session.getApi().fs(path, hidden.value) }
  catch (e) {
    error.value = errText(e)
    if (!data.value) { try { data.value = await session.getApi().fs(undefined, hidden.value) } catch { /* resta l'errore */ } }		// il percorso di partenza non c'e': si parte dalla cartella personale
  } finally { busy.value = false }
}
watch(() => props.open, (v) => { if (v) { newName.value = ''; data.value = null; if (props.showHidden) hidden.value = true; void load(props.start || undefined) } }, { immediate: true })
const crumbs = computed(() => {
  const p = data.value?.path ?? ''; const parts = p.split('/').filter(Boolean); let acc = ''
  return [{ label: '/', path: '/' }, ...parts.map((x) => ({ label: x, path: (acc += '/' + x) }))]
})
const canPick = computed(() => !!data.value && (!props.needWrite || data.value.writable))
async function create() {
  const n = newName.value.trim(); if (!n || !data.value) return
  busy.value = true; error.value = ''
  try { const r = await session.getApi().fsMkdir(data.value.path.replace(/\/$/, '') + '/' + n); newName.value = ''; await load(r.path) }
  catch (e) { error.value = errText(e); busy.value = false }
}
</script>

<template>
  <Modal :open="open" :title="t('picker.title')" wide @close="emit('close')">
    <div class="flex flex-col gap-3">
      <nav class="flex flex-wrap items-center gap-1 text-xs" :aria-label="t('picker.path')">
        <template v-for="(c, i) in crumbs" :key="c.path">
          <span v-if="i > 1" class="text-mute" aria-hidden="true">/</span>
          <button type="button" class="rounded-md px-1.5 py-1 hover:bg-sunk" :class="i === crumbs.length - 1 ? 'font-semibold text-accent' : 'text-mute'" @click="load(c.path)">{{ c.label }}</button>
        </template>
      </nav>
      <div class="flex flex-wrap items-center justify-between gap-2 text-xs text-mute">
        <span v-if="data && !data.writable" class="text-warn">{{ t('picker.notWritable') }}</span>
        <span v-else-if="data?.free_bytes !== undefined" class="tabular-nums">{{ t('picker.free', { size: bytes(data.free_bytes) }) }}</span><span v-else></span>
        <label class="flex items-center gap-1.5"><input v-model="hidden" type="checkbox" class="size-3.5" @change="load(data?.path)" />{{ t('picker.hidden') }}</label>
      </div>
      <ul class="max-h-72 overflow-y-auto rounded-lg border border-line" :aria-busy="busy" role="listbox" :aria-label="t('picker.title')">
        <li v-if="data?.parent"><button type="button" class="flex w-full items-center gap-2 px-3 py-2 text-left text-sm hover:bg-sunk" @click="load(data.parent!)"><Icon name="back" class="!size-4 text-mute" />{{ t('picker.up') }}</button></li>
        <li v-for="e in data?.entries ?? []" :key="e.path">
          <button type="button" class="flex w-full items-center gap-2 px-3 py-2 text-left text-sm hover:bg-sunk disabled:opacity-50" :disabled="!e.readable" @click="load(e.path)">
            <Icon name="folder" class="!size-4 text-mute" /><span class="min-w-0 flex-1 truncate">{{ e.name }}</span>
            <span v-if="!e.readable" class="text-xs text-warn">{{ t('picker.noAccess') }}</span><Icon v-else name="chevron" class="!size-3.5 text-mute" />
          </button>
        </li>
        <li v-if="data && !data.entries.length" class="px-3 py-4 text-sm text-mute">{{ t('picker.empty') }}</li>
        <li v-if="data?.truncated" class="px-3 py-2 text-xs text-warn">{{ t('picker.truncated') }}</li>
      </ul>
      <form class="flex gap-2" @submit.prevent="create">
        <input v-model="newName" type="text" maxlength="100" class="min-w-0 flex-1 rounded-lg border border-line bg-bg px-3 py-2 text-sm" :placeholder="t('picker.newPlaceholder')" :aria-label="t('picker.newFolder')" />
        <button type="submit" class="inline-flex items-center gap-1.5 rounded-lg border border-line px-3 py-2 text-sm hover:bg-sunk disabled:opacity-50" :disabled="busy || !newName.trim() || !data?.writable"><Icon name="plus" class="!size-3.5" />{{ t('picker.create') }}</button>
      </form>
      <p v-if="error" class="rounded-lg border border-bad px-3 py-2 text-sm text-bad" role="alert">{{ error }}</p>
    </div>
    <template #footer>
      <button type="button" class="rounded-lg border border-line px-4 py-2 text-sm hover:bg-sunk" @click="emit('close')">{{ t('common.cancel') }}</button>
      <button type="button" class="rounded-lg bg-accent px-4 py-2 text-sm font-medium text-onaccent disabled:opacity-50" :disabled="!canPick" @click="data && emit('pick', data.path)">{{ t('picker.choose') }}</button>
    </template>
  </Modal>
</template>
