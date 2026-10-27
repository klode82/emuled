<script setup lang="ts">
import { computed, onMounted, onUnmounted, reactive, ref } from 'vue'
import { useI18n } from 'vue-i18n'
import { useServers } from '@/stores/servers'
import { useCore } from '@/stores/core'
import { useUi } from '@/stores/ui'
import { number } from '@/lib/format'
import { useSort } from '@/lib/useSort'
import SortTh from '@/components/SortTh.vue'
import type { Server, ServerPriority } from '@/types'
import Icon from '@/components/Icon.vue'
import EmptyState from '@/components/EmptyState.vue'
import Modal from '@/components/Modal.vue'

const { t } = useI18n()
const sv = useServers(), core = useCore(), ui = useUi()
let timer: ReturnType<typeof setInterval>
onMounted(() => { void sv.load(); timer = setInterval(() => void sv.load(), 10000) })
onUnmounted(() => clearInterval(timer))
const cur = computed(() => core.status?.ed2k.server)
const isCurrent = (s: Server) => !!cur.value && core.status?.ed2k.connected && cur.value.ip === s.ip && cur.value.port === s.port
const addr = (s: Server) => `${s.ip}:${s.port}`
const PRIOS: ServerPriority[] = ['low', 'normal', 'high']
const PRIO_RANK = { low: 0, normal: 1, high: 2 }
const { sorted, sorter } = useSort(() => sv.list, {
  name: { get: (s) => s.name || s.address }, address: { get: (s) => s.ip + ':' + String(s.port).padStart(5, '0') }, users: { get: (s) => s.users, desc: true }, files: { get: (s) => s.files, desc: true },
  ping: { get: (s) => s.ping_ms || 1e9 }, errors: { get: (s) => s.failed_count, desc: true }, priority: { get: (s) => PRIO_RANK[s.priority], desc: true },
}, '', 1, 'servers')

// add a server
const dialog = ref(false), busy = ref(false)
const form = reactive({ address: '', name: '', priority: 'normal' as ServerPriority, static: false })
const addrOk = computed(() => /^[^\s:/|]+:\d{1,5}$/.test(form.address.trim()) && Number(form.address.trim().split(':').pop()) <= 65535 && Number(form.address.trim().split(':').pop()) > 0)
function openAdd() { Object.assign(form, { address: '', name: '', priority: 'normal', static: false }); dialog.value = true }
async function submit() {
  busy.value = true
  const ok = await sv.add({ address: form.address.trim(), name: form.name.trim() || undefined, priority: form.priority, static: form.static })
  busy.value = false
  if (ok) dialog.value = false
}
const updateUrl = ref('')
onMounted(() => void sv.loadUrls())
async function del(s: Server) {
  if (await ui.ask({ title: t('servers.removeTitle'), text: t('servers.removeText', { name: s.name || addr(s) }), confirm: t('common.remove'), danger: true })) await sv.remove(addr(s))
}
</script>
<template>
  <div class="flex flex-col gap-4 p-4">
    <header class="flex flex-wrap items-center justify-between gap-3">
      <div><h1 class="text-lg font-semibold">{{ t('servers.title') }}</h1>
        <p class="text-sm text-mute" role="status">{{ core.status?.ed2k.connected ? t('servers.connectedTo', { name: cur?.name ?? '', id: t(core.status.ed2k.high_id ? 'shell.highId' : 'shell.lowId') }) : core.status?.ed2k.connecting ? t('servers.connecting') : t('servers.notConnected') }}</p></div>
      <div class="flex flex-wrap gap-2">
        <button class="inline-flex items-center gap-1.5 rounded-lg bg-accent px-3.5 py-2 text-sm font-medium text-onaccent" @click="sv.connect()"><Icon name="wifi" />{{ t('servers.connectAny') }}</button>
        <button class="inline-flex items-center gap-1.5 rounded-lg border border-line px-3.5 py-2 text-sm hover:bg-sunk" @click="openAdd"><Icon name="plus" />{{ t('servers.add') }}</button>
        <button class="inline-flex items-center gap-1.5 rounded-lg border border-line px-3.5 py-2 text-sm hover:bg-sunk" :disabled="!core.status?.ed2k.connected && !core.status?.ed2k.connecting" @click="sv.disconnect()">{{ t('servers.disconnect') }}</button>
      </div>
    </header>
    <form class="flex flex-wrap items-end gap-2 rounded-xl border border-line bg-surface p-4" @submit.prevent="sv.updateFromUrl(updateUrl)">
      <label class="min-w-0 flex-1 basis-72 text-xs text-mute">{{ t('servers.updateFrom') }}
        <input v-model="updateUrl" type="url" list="server-urls" class="mt-1 w-full rounded-lg border border-line bg-bg px-3 py-2 font-mono text-xs text-fg" placeholder="https://…/server.met" /></label>
      <datalist id="server-urls"><option v-for="u in sv.urls" :key="u" :value="u" /></datalist>
      <button class="inline-flex items-center gap-1.5 rounded-lg border border-line px-3.5 py-2 text-sm hover:bg-sunk disabled:opacity-50" :disabled="!updateUrl.trim() || sv.updating"><Icon name="refresh" />{{ sv.updating ? t('servers.updating') : t('servers.update') }}</button>
      <p class="basis-full text-xs text-mute">{{ t('servers.updateHint') }}</p>
    </form>
    <EmptyState v-if="!sv.list.length && !sv.loading" icon="server" :title="t('servers.emptyTitle')" :text="t('servers.emptyText')"><button class="inline-flex items-center gap-1.5 rounded-lg bg-accent px-4 py-2 text-sm font-medium text-onaccent" @click="openAdd"><Icon name="plus" />{{ t('servers.add') }}</button></EmptyState>
    <div v-else class="overflow-x-auto rounded-xl border border-line bg-surface">
      <table class="w-full min-w-[44rem] text-left text-sm">
        <thead class="bg-sunk text-xs uppercase tracking-wide text-mute"><tr>
          <SortTh :label="t('cols.name')" col="name" :sorter="sorter" /><SortTh :label="t('cols.address')" col="address" :sorter="sorter" /><SortTh :label="t('cols.users')" col="users" :sorter="sorter" right />
          <SortTh :label="t('cols.files')" col="files" :sorter="sorter" right class="hidden xl:table-cell" /><SortTh :label="t('cols.ping')" col="ping" :sorter="sorter" right />
          <SortTh :label="t('cols.errors')" col="errors" :sorter="sorter" right class="hidden xl:table-cell" /><SortTh :label="t('common.priority')" col="priority" :sorter="sorter" /><th class="px-4 py-2"><span class="sr-only">{{ t('common.actions') }}</span></th></tr></thead>
        <tbody>
          <tr v-for="s in sorted" :key="addr(s)" class="border-t border-line" :class="isCurrent(s) ? 'bg-soft' : ''">
            <td class="px-4 py-2.5"><p class="break-words font-medium">{{ s.name || s.address }} <span v-if="isCurrent(s)" class="ml-1 rounded-full bg-accent px-2 py-0.5 text-[10px] font-semibold text-onaccent">{{ t('servers.badgeConnected') }}</span></p><p v-if="s.description" class="max-w-xs truncate text-xs text-mute">{{ s.description }}</p></td>
            <td class="px-4 py-2.5 font-mono text-xs tabular-nums">{{ addr(s) }}</td>
            <td class="px-4 py-2.5 text-right tabular-nums">{{ number(s.users) }}</td><td class="hidden px-4 py-2.5 text-right tabular-nums xl:table-cell">{{ number(s.files) }}</td>
            <td class="whitespace-nowrap px-4 py-2.5 text-right tabular-nums">{{ s.ping_ms ? s.ping_ms + ' ms' : '—' }}</td><td class="hidden px-4 py-2.5 text-right tabular-nums xl:table-cell" :class="s.failed_count ? 'text-warn' : 'text-mute'">{{ s.failed_count }}</td>
            <td class="px-4 py-2.5"><div class="flex items-center gap-2">
              <select class="rounded-md border border-line bg-bg px-2 py-1 text-xs" :value="s.priority" :aria-label="t('servers.priorityOf', { name: s.name || addr(s) })" @change="sv.edit(addr(s), { priority: ($event.target as HTMLSelectElement).value as ServerPriority })"><option v-for="p in PRIOS" :key="p" :value="p">{{ t('servers.prio.' + p) }}</option></select>
              <label class="flex items-center gap-1 text-xs text-mute" :title="t('servers.staticHint')"><input type="checkbox" :checked="s.static" @change="sv.edit(addr(s), { static: ($event.target as HTMLInputElement).checked })" />{{ t('servers.static') }}</label></div></td>
            <td class="whitespace-nowrap px-4 py-2.5 text-right"><button v-if="isCurrent(s)" class="rounded-lg border border-line px-3 py-1.5 text-xs hover:bg-sunk" :title="t('servers.reconnectHint')" @click="sv.reconnect(addr(s))">{{ t('servers.reconnect') }}</button>
              <button v-else class="rounded-lg border border-line px-3 py-1.5 text-xs hover:bg-sunk" @click="sv.connect(addr(s))">{{ t('servers.connectTo') }}</button>
              <button class="ml-1 rounded-md p-1.5 text-mute hover:bg-sunk hover:text-bad" :aria-label="t('servers.removeAria', { name: s.name || addr(s) })" @click="del(s)"><Icon name="trash" /></button></td>
          </tr>
        </tbody>
      </table>
    </div>

    <Modal :open="dialog" :title="t('servers.addTitle')" @close="dialog = false">
      <form class="flex flex-col gap-4" @submit.prevent="addrOk && submit()">
        <label class="flex flex-col gap-1.5 text-sm font-medium">{{ t('servers.address') }}
          <input v-model="form.address" class="rounded-lg border bg-bg px-3 py-2 font-mono text-sm font-normal" :class="form.address && !addrOk ? 'border-bad' : 'border-line'" placeholder="176.123.5.89:4725" autofocus />
          <span class="text-xs font-normal" :class="form.address && !addrOk ? 'text-bad' : 'text-mute'">{{ t('servers.addressHint') }}</span></label>
        <label class="flex flex-col gap-1.5 text-sm font-medium">{{ t('servers.name') }}<input v-model="form.name" class="rounded-lg border border-line bg-bg px-3 py-2 text-sm font-normal" :placeholder="t('servers.nameHint')" /></label>
        <div class="flex flex-wrap items-center gap-4">
          <label class="flex flex-col gap-1.5 text-sm font-medium">{{ t('common.priority') }}<select v-model="form.priority" class="rounded-lg border border-line bg-bg px-3 py-2 text-sm font-normal"><option v-for="p in PRIOS" :key="p" :value="p">{{ t('servers.prio.' + p) }}</option></select></label>
          <label class="mt-5 flex items-center gap-2 text-sm"><input v-model="form.static" type="checkbox" class="size-4" />{{ t('servers.staticLabel') }}</label>
        </div>
      </form>
      <template #footer>
        <button class="rounded-lg border border-line px-4 py-2 text-sm hover:bg-sunk" @click="dialog = false">{{ t('common.cancel') }}</button>
        <button class="rounded-lg bg-accent px-4 py-2 text-sm font-medium text-onaccent" :disabled="!addrOk || busy" @click="submit">{{ busy ? t('add.adding') : t('common.add') }}</button>
      </template>
    </Modal>
  </div>
</template>
