<!-- Crea o modifica una categoria (da Trasferimenti e dalla Ricerca). Ritorna l'id della categoria salvata. -->
<script setup lang="ts">
import { reactive, ref, watch } from 'vue'
import { useI18n } from 'vue-i18n'
import { usePrefs } from '@/stores/prefs'
import type { Category } from '@/types'
import Modal from './Modal.vue'
const { t } = useI18n()
const props = defineProps<{ open: boolean; category?: Category | null }>()
const emit = defineEmits<{ close: []; saved: [id: number] }>()
const prefs = usePrefs()
const form = reactive({ title: '', comment: '', incoming_path: '', priority: 1 })
const busy = ref(false)
const CAT_PRIO = [4, 0, 1, 2, 3, 5]
watch(() => props.open, (v) => { if (v) Object.assign(form, { title: '', comment: '', incoming_path: '', priority: 1 }, props.category ? { title: props.category.title, comment: props.category.comment, incoming_path: props.category.incoming_path, priority: props.category.priority } : {}) })
async function submit() {
  if (!form.title.trim()) return
  busy.value = true
  const body = { title: form.title.trim(), comment: form.comment, incoming_path: form.incoming_path, priority: Number(form.priority) }
  const id = props.category ? await prefs.editCategory(props.category.id, body) : await prefs.createCategory(body)
  busy.value = false
  if (id !== null) { emit('saved', id); emit('close') }
}
</script>
<template>
  <Modal :open="open" :title="category ? t('cat.editTitle') : t('cat.newTitle')" @close="emit('close')">
    <form id="cat-form" class="grid gap-3" @submit.prevent="submit">
      <label class="flex flex-col gap-1 text-xs text-mute">{{ t('cols.name') }}<input v-model="form.title" required class="rounded-lg border border-line bg-bg px-3 py-2 text-sm text-fg" /></label>
      <label class="flex flex-col gap-1 text-xs text-mute">{{ t('prefs.catPriority') }}<select v-model.number="form.priority" class="rounded-lg border border-line bg-bg px-3 py-2 text-sm text-fg"><option v-for="x in CAT_PRIO" :key="x" :value="x">{{ t('prefs.catPrio.' + x) }}</option></select></label>
      <label class="flex flex-col gap-1 text-xs text-mute">{{ t('prefs.catFolder') }}<input v-model="form.incoming_path" class="rounded-lg border border-line bg-bg px-3 py-2 font-mono text-xs text-fg" placeholder="/home/user/eMule/Books" /></label>
      <label class="flex flex-col gap-1 text-xs text-mute">{{ t('prefs.catComment') }}<input v-model="form.comment" class="rounded-lg border border-line bg-bg px-3 py-2 text-sm text-fg" /></label>
    </form>
    <template #footer>
      <button type="button" class="rounded-lg border border-line px-4 py-2 text-sm hover:bg-sunk" @click="emit('close')">{{ t('common.cancel') }}</button>
      <button form="cat-form" class="rounded-lg bg-accent px-4 py-2 text-sm font-medium text-onaccent disabled:opacity-50" :disabled="!form.title.trim() || busy">{{ t('prefs.saveCategory') }}</button>
    </template>
  </Modal>
</template>
