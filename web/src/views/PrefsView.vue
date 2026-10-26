<script setup lang="ts">
import { computed, nextTick, onMounted, reactive, ref, watch } from 'vue'
import { useI18n } from 'vue-i18n'
import { useRoute } from 'vue-router'
import { usePrefs } from '@/stores/prefs'
import { useUi, type Theme } from '@/stores/ui'
import { PREFS, PREF_GROUPS, type PrefMeta } from '@/lib/prefsMeta'
import { LANGS, setLang, type Lang } from '@/i18n'
import type { Category, Directories } from '@/types'
import Icon from '@/components/Icon.vue'

const { t, te, locale } = useI18n()
const route = useRoute()
const p = usePrefs(), ui = useUi()
onMounted(async () => { await Promise.all([p.load(), p.loadDirs()]); await nextTick(); if (route.hash) document.querySelector(route.hash)?.scrollIntoView() })
const known = new Set(PREFS.map((m) => m.key))
const groups = computed(() => PREF_GROUPS.map((g) => ({ name: g, items: PREFS.filter((m) => m.group === g && m.key in p.draft) })).filter((g) => g.items.length))
const others = computed(() => Object.keys(p.draft).filter((k) => !known.has(k)))
const hint = (m: PrefMeta) => (te(`prefs.items.${m.key}.hint`) ? t(`prefs.items.${m.key}.hint`) : '')
const invalid = (m: PrefMeta) => {
  const v = p.draft[m.key]
  if (m.type === 'int') return typeof v !== 'number' || !Number.isInteger(v) || v < (m.min ?? -Infinity) || v > (m.max ?? Infinity)
  if (m.type === 'string') return typeof v !== 'string' || (!m.optional && !v.trim())
  return false
}
const anyInvalid = computed(() => PREFS.some((m) => m.key in p.draft && invalid(m)))
const needRestart = computed(() => (p.restart.length ? PREFS.filter((m) => p.restart.includes(m.key)).map((m) => t(`prefs.items.${m.key}.label`)) : []))
const THEMES: Theme[] = ['system', 'light', 'dark']

// ---- cartelle: si lavora su una copia e si manda al demone solo cio' che e' cambiato
const dd = reactive<Directories>({ incoming: '', temp: [], shared: [] })
const missing = (d: string) => !!p.dirs?.shared_missing?.includes(d)
const dirsError = ref(''), dirsBusy = ref(false)
watch(() => p.dirs, (d) => { if (d) { dd.incoming = d.incoming; dd.temp = [...d.temp]; dd.shared = [...d.shared] } }, { immediate: true })
const same = (a: string[], b: string[]) => a.length === b.length && a.every((x, i) => x === b[i])
const dirsChanges = computed(() => {
  const d = p.dirs, c: Partial<Directories> = {}
  if (!d) return c
  if (dd.incoming.trim() !== d.incoming) c.incoming = dd.incoming.trim()
  if (!same(dd.temp.map((x) => x.trim()), d.temp)) c.temp = dd.temp.map((x) => x.trim())
  if (!same(dd.shared.map((x) => x.trim()), d.shared)) c.shared = dd.shared.map((x) => x.trim()).filter(Boolean)
  return c
})
const dirsDirty = computed(() => Object.keys(dirsChanges.value).length > 0)
async function applyDirs() { dirsBusy.value = true; dirsError.value = await p.saveDirs(dirsChanges.value); dirsBusy.value = false }
function resetDirs() { const d = p.dirs; if (d) { dd.incoming = d.incoming; dd.temp = [...d.temp]; dd.shared = [...d.shared] } dirsError.value = '' }

// ---- categorie
const editing = ref<number | null>(null)
const form = reactive<Partial<Category>>({})
function edit(c?: Category) { editing.value = c ? c.id : -1; Object.assign(form, { title: '', comment: '', incoming_path: '', priority: 1 }, c ?? {}) }
async function saveCat() {
  if (!form.title?.trim()) return
  const body = { title: form.title.trim(), comment: form.comment ?? '', incoming_path: form.incoming_path ?? '', priority: Number(form.priority) }
  if (editing.value === -1) await p.addCategory(body); else await p.editCategory(editing.value as number, body)
  editing.value = null
}
async function delCat(c: Category) {
  if (await ui.ask({ title: t('prefs.catDeleteTitle'), text: t('prefs.catDeleteText', { name: c.title }), confirm: t('common.remove'), danger: true })) await p.removeCategory(c.id)
}
const CAT_PRIO = [4, 0, 1, 2, 3, 5]
const nChanges = computed(() => Object.keys(p.changes).length)
</script>
<template>
  <div class="flex max-w-3xl flex-col gap-6 p-4 pb-24">
    <h1 class="text-lg font-semibold">{{ t('nav.prefs') }}</h1>
    <p v-if="p.persisted === false" class="rounded-lg border border-line bg-soft px-4 py-3 text-sm text-warn" role="status">{{ t('prefs.tempIdentity') }}</p>
    <p v-if="needRestart.length" class="rounded-lg border border-line bg-soft px-4 py-3 text-sm text-warn" role="status">{{ t('prefs.needRestart', { list: needRestart.join(', ') }) }}</p>

    <section v-for="g in groups" :key="g.name" class="rounded-xl border border-line bg-surface">
      <h2 class="border-b border-line px-5 py-3 text-sm font-semibold">{{ t('prefs.groups.' + g.name) }}</h2>
      <div class="divide-y divide-line">
        <div v-for="m in g.items" :key="m.key" class="flex flex-col gap-1.5 px-5 py-3 sm:flex-row sm:items-center sm:justify-between sm:gap-6">
          <div class="min-w-0 sm:max-w-md"><label :for="'p-' + m.key" class="text-sm font-medium">{{ t(`prefs.items.${m.key}.label`) }}<span v-if="m.restart" class="ml-2 rounded-full bg-sunk px-2 py-0.5 text-[10px] font-semibold uppercase text-mute">{{ t('prefs.fromRestart') }}</span></label><p v-if="hint(m)" class="text-xs text-mute">{{ hint(m) }}</p></div>
          <div class="flex shrink-0 items-center gap-2">
            <input v-if="m.type === 'bool'" :id="'p-' + m.key" v-model="p.draft[m.key]" type="checkbox" class="size-5 accent-[var(--accent)]" />
            <template v-else>
              <input :id="'p-' + m.key" v-model="p.draft[m.key]" :type="m.type === 'int' ? 'number' : 'text'" :min="m.min" :max="m.max" :aria-invalid="invalid(m)" class="w-full rounded-lg border bg-bg px-3 py-2 text-sm sm:w-40" :class="invalid(m) ? 'border-bad' : 'border-line'" @input="m.type === 'int' && (p.draft[m.key] = ($event.target as HTMLInputElement).value === '' ? NaN : Number(($event.target as HTMLInputElement).value))" />
              <span v-if="m.unit" class="text-xs text-mute">{{ m.unit }}</span>
            </template>
          </div>
        </div>
      </div>
    </section>
    <section v-if="others.length" class="rounded-xl border border-line bg-surface">
      <h2 class="border-b border-line px-5 py-3 text-sm font-semibold">{{ t('prefs.others') }}</h2>
      <div class="divide-y divide-line"><div v-for="k in others" :key="k" class="flex items-center justify-between gap-4 px-5 py-3 text-sm"><code class="font-mono text-xs">{{ k }}</code><span class="font-mono text-xs">{{ p.draft[k] }}</span></div></div>
    </section>

    <section id="dirs" class="rounded-xl border border-line bg-surface">
      <h2 class="border-b border-line px-5 py-3 text-sm font-semibold">{{ t('dirs.title') }}</h2>
      <div v-if="p.dirs" class="flex flex-col gap-5 px-5 py-4">
        <label class="flex flex-col gap-1 text-sm font-medium">{{ t('dirs.incoming') }}
          <input v-model="dd.incoming" class="rounded-lg border border-line bg-bg px-3 py-2 font-mono text-xs font-normal" /><span class="text-xs font-normal text-mute">{{ t('dirs.incomingHint') }}</span></label>
        <fieldset class="flex flex-col gap-2"><legend class="pb-1 text-sm font-medium">{{ t('dirs.temp') }}</legend>
          <div v-for="(_, i) in dd.temp" :key="i" class="flex items-center gap-2"><input v-model="dd.temp[i]" class="min-w-0 flex-1 rounded-lg border border-line bg-bg px-3 py-2 font-mono text-xs" :aria-label="t('dirs.tempN', { n: i + 1 })" />
            <button class="rounded-md p-2 text-mute hover:bg-sunk hover:text-bad disabled:opacity-40" :disabled="dd.temp.length <= 1" :aria-label="t('common.remove')" @click="dd.temp.splice(i, 1)"><Icon name="trash" /></button></div>
          <button class="inline-flex w-fit items-center gap-1.5 rounded-lg border border-line px-3 py-1.5 text-xs hover:bg-sunk" @click="dd.temp.push('')"><Icon name="plus" class="!size-3.5" />{{ t('dirs.addTemp') }}</button>
          <span class="text-xs text-mute">{{ t('dirs.tempHint') }}</span></fieldset>
        <fieldset class="flex flex-col gap-2"><legend class="pb-1 text-sm font-medium">{{ t('dirs.shared') }}</legend>
          <p v-if="!dd.shared.length" class="text-sm text-mute">{{ t('dirs.noShared') }}</p>
          <div v-for="(_, i) in dd.shared" :key="i" class="flex items-center gap-2"><input v-model="dd.shared[i]" class="min-w-0 flex-1 rounded-lg border bg-bg px-3 py-2 font-mono text-xs" :class="missing(dd.shared[i]) ? 'border-warn' : 'border-line'" :aria-label="t('dirs.sharedN', { n: i + 1 })" /><span v-if="missing(dd.shared[i])" class="whitespace-nowrap text-xs text-warn">{{ t('dirs.missing') }}</span>
            <button class="rounded-md p-2 text-mute hover:bg-sunk hover:text-bad" :aria-label="t('common.remove')" @click="dd.shared.splice(i, 1)"><Icon name="trash" /></button></div>
          <button class="inline-flex w-fit items-center gap-1.5 rounded-lg border border-line px-3 py-1.5 text-xs hover:bg-sunk" @click="dd.shared.push('')"><Icon name="plus" class="!size-3.5" />{{ t('dirs.addShared') }}</button>
          <span class="text-xs text-mute">{{ t('dirs.sharedHint') }}</span></fieldset>
        <p v-if="dirsError" class="rounded-lg border border-bad px-3 py-2 text-sm text-bad" role="alert">{{ dirsError }}</p>
        <p v-if="p.dirsSaved === false" class="text-xs text-warn">{{ t('dirs.tempIdentity') }}</p>
        <div class="flex justify-end gap-2"><button class="rounded-lg border border-line px-4 py-2 text-sm hover:bg-sunk" :disabled="!dirsDirty" @click="resetDirs">{{ t('common.cancel') }}</button>
          <button class="rounded-lg bg-accent px-4 py-2 text-sm font-medium text-onaccent" :disabled="!dirsDirty || dirsBusy" @click="applyDirs">{{ dirsBusy ? t('common.saving') : t('dirs.apply') }}</button></div>
      </div>
    </section>

    <section class="rounded-xl border border-line bg-surface">
      <header class="flex items-center justify-between border-b border-line px-5 py-3"><h2 class="text-sm font-semibold">{{ t('prefs.categories') }}</h2><button class="inline-flex items-center gap-1.5 rounded-lg border border-line px-3 py-1.5 text-xs hover:bg-sunk" @click="edit()"><Icon name="plus" class="!size-3.5" />{{ t('common.new') }}</button></header>
      <ul class="divide-y divide-line">
        <li v-for="c in p.categories" :key="c.id" class="flex items-center justify-between gap-3 px-5 py-3 text-sm">
          <div class="min-w-0"><p class="font-medium">{{ c.id === 0 ? t('prefs.defaultCategory') : c.title }}</p><p v-if="c.comment || c.incoming_path" class="truncate text-xs text-mute">{{ c.comment }}{{ c.comment && c.incoming_path ? ' · ' : '' }}{{ c.incoming_path }}</p></div>
          <div v-if="c.id > 0" class="flex gap-1"><button class="rounded-md p-2 text-mute hover:bg-sunk hover:text-fg" :aria-label="t('prefs.editCat', { name: c.title })" @click="edit(c)"><Icon name="gear" /></button><button class="rounded-md p-2 text-mute hover:bg-sunk hover:text-bad" :aria-label="t('prefs.deleteCat', { name: c.title })" @click="delCat(c)"><Icon name="trash" /></button></div>
        </li>
      </ul>
      <form v-if="editing !== null" class="grid gap-3 border-t border-line bg-sunk px-5 py-4 sm:grid-cols-2" @submit.prevent="saveCat">
        <label class="flex flex-col gap-1 text-xs text-mute">{{ t('cols.name') }}<input v-model="form.title" required class="rounded-lg border border-line bg-bg px-3 py-2 text-sm text-fg" /></label>
        <label class="flex flex-col gap-1 text-xs text-mute">{{ t('prefs.catPriority') }}<select v-model.number="form.priority" class="rounded-lg border border-line bg-bg px-3 py-2 text-sm text-fg"><option v-for="x in CAT_PRIO" :key="x" :value="x">{{ t('prefs.catPrio.' + x) }}</option></select></label>
        <label class="flex flex-col gap-1 text-xs text-mute sm:col-span-2">{{ t('prefs.catFolder') }}<input v-model="form.incoming_path" class="rounded-lg border border-line bg-bg px-3 py-2 font-mono text-xs text-fg" placeholder="/home/user/eMule/Books" /></label>
        <label class="flex flex-col gap-1 text-xs text-mute sm:col-span-2">{{ t('prefs.catComment') }}<input v-model="form.comment" class="rounded-lg border border-line bg-bg px-3 py-2 text-sm text-fg" /></label>
        <div class="flex justify-end gap-2 sm:col-span-2"><button type="button" class="rounded-lg border border-line px-4 py-2 text-sm hover:bg-surface" @click="editing = null">{{ t('common.cancel') }}</button><button class="rounded-lg bg-accent px-4 py-2 text-sm font-medium text-onaccent" :disabled="!form.title?.trim()">{{ t('prefs.saveCategory') }}</button></div>
      </form>
    </section>

    <section class="rounded-xl border border-line bg-surface">
      <h2 class="border-b border-line px-5 py-3 text-sm font-semibold">{{ t('prefs.interface') }}</h2>
      <div class="divide-y divide-line">
        <div class="flex flex-col gap-1.5 px-5 py-3 sm:flex-row sm:items-center sm:justify-between"><div><label for="theme" class="text-sm font-medium">{{ t('prefs.theme') }}</label><p class="text-xs text-mute">{{ t('prefs.rememberedHere') }}</p></div>
          <select id="theme" v-model="ui.theme" class="rounded-lg border border-line bg-bg px-3 py-2 text-sm"><option v-for="x in THEMES" :key="x" :value="x">{{ t('prefs.themes.' + x) }}</option></select></div>
        <div class="flex flex-col gap-1.5 px-5 py-3 sm:flex-row sm:items-center sm:justify-between"><div><label for="lang" class="text-sm font-medium">{{ t('lang.label') }}</label><p class="text-xs text-mute">{{ t('prefs.rememberedHere') }}</p></div>
          <select id="lang" class="rounded-lg border border-line bg-bg px-3 py-2 text-sm" :value="locale" @change="setLang(($event.target as HTMLSelectElement).value as Lang)"><option v-for="l in LANGS" :key="l.code" :value="l.code">{{ l.name }}</option></select></div>
      </div>
    </section>

    <div v-if="p.dirty" class="fixed inset-x-0 bottom-14 z-20 border-t border-line bg-surface px-4 py-3 shadow-lg md:bottom-0 md:left-16 lg:left-56">
      <div class="mx-auto flex max-w-3xl items-center justify-between gap-3"><p class="text-sm" role="status">{{ t('prefs.unsaved', nChanges) }}</p>
        <div class="flex gap-2"><button class="rounded-lg border border-line px-4 py-2 text-sm hover:bg-sunk" @click="p.reset()">{{ t('common.cancel') }}</button><button class="rounded-lg bg-accent px-4 py-2 text-sm font-medium text-onaccent" :disabled="p.saving || anyInvalid" @click="p.save()">{{ p.saving ? t('common.saving') : t('common.save') }}</button></div></div>
    </div>
  </div>
</template>
