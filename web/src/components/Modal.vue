<!-- Finestra modale con <dialog> nativo: focus intrappolato, Esc e clic fuori la chiudono -->
<script setup lang="ts">
import { nextTick, ref, watch } from 'vue'
import { useI18n } from 'vue-i18n'
import Icon from './Icon.vue'
const { t } = useI18n()
const props = defineProps<{ open: boolean; title: string; wide?: boolean }>()
const emit = defineEmits<{ close: [] }>()
const el = ref<HTMLDialogElement | null>(null)
watch(() => props.open, async (v) => {
  await nextTick()
  if (!el.value) return
  if (v && !el.value.open) el.value.showModal()
  else if (!v && el.value.open) el.value.close()
}, { immediate: true, flush: 'post' })
function backdrop(e: MouseEvent) { if (e.target === el.value) emit('close') }
</script>
<template>
  <dialog ref="el" class="m-auto w-[calc(100%-2rem)] rounded-xl border border-line bg-surface p-0 text-fg shadow-2xl backdrop:bg-black/50" :class="wide ? 'max-w-2xl' : 'max-w-md'"
    :aria-label="title" @cancel.prevent="emit('close')" @click="backdrop" @close="open && emit('close')">
    <div class="flex items-center justify-between border-b border-line px-5 py-3">
      <h2 class="text-base font-semibold">{{ title }}</h2>
      <button class="rounded-md p-1.5 text-mute hover:bg-sunk hover:text-fg" :aria-label="t('common.close')" @click="emit('close')"><Icon name="x" /></button>
    </div>
    <div class="p-5"><slot /></div>
    <div v-if="$slots.footer" class="flex flex-wrap justify-end gap-2 border-t border-line px-5 py-3"><slot name="footer" /></div>
  </dialog>
</template>
