<script setup lang="ts">
import { computed, onMounted, ref } from 'vue'
import { useI18n } from 'vue-i18n'
import { useCore } from '@/stores/core'
import { useSession } from '@/stores/session'
import { errText, useToasts } from '@/stores/toasts'
import { number } from '@/lib/format'
import Icon from '@/components/Icon.vue'
import { usePrefs } from '@/stores/prefs'

const { t } = useI18n()
const core = useCore(), session = useSession(), toasts = useToasts(), prefs = usePrefs()
onMounted(() => void core.refresh())
const upnp = computed(() => core.status?.upnp)
// repopulate Kad from a nodes.dat (web address) or by asking a node for the first contacts
const nodesUrl = ref(''), nodeAddr = ref(''), boot = ref(false)
async function bootstrap(b: { url?: string; address?: string }) {
  boot.value = true
  try {
    const api = session.getApi(); let r = await api.kadBootstrap(b)
    for (let i = 0; i < 90 && r.state === 'running'; i++) { await new Promise((ok) => setTimeout(ok, 1000)); r = await api.kadBootstrapState() }
    if (r.state === 'failed') toasts.error(t('kad.bootstrapFailed', { why: r.error ?? '' }))
    else toasts.ok(r.kind === 'url' ? t('kad.bootstrapUrlDone', { n: r.added ?? 0, total: r.contacts ?? 0 }) : t('kad.bootstrapNodeDone'))
    setTimeout(() => void core.refresh(), 2000)
  } catch (e) { toasts.error(errText(e)) } finally { boot.value = false }
}
async function toggleUpnp() { prefs.draft.upnp_enabled = !upnp.value?.enabled; await prefs.save(); setTimeout(() => void core.refresh(), 2000) }
const k = computed(() => core.status?.kad)
async function run(what: 'kadConnect' | 'kadDisconnect' | 'kadRecheck', ok: string) {
  try { await session.getApi()[what](); toasts.ok(t(ok)); setTimeout(() => void core.refresh(), 1000) } catch (e) { toasts.error(errText(e)) }
}
const fw = (v?: boolean, verified = true) => (v === undefined ? '—' : !verified ? t('kad.verifying') : v ? t('kad.closed') : t('kad.open'))
</script>
<template>
  <div class="flex max-w-3xl flex-col gap-5 p-4">
    <header class="flex flex-wrap items-center justify-between gap-3">
      <div><h1 class="text-lg font-semibold">Kad</h1><p class="text-sm text-mute" role="status">{{ !k?.running ? t('kad.isOff') : k.connected ? t('kad.isOn') : t('kad.isConnecting') }}</p></div>
      <div class="flex flex-wrap gap-2">
        <button v-if="!k?.running" class="inline-flex items-center gap-1.5 rounded-lg bg-accent px-3.5 py-2 text-sm font-medium text-onaccent" @click="run('kadConnect', 'kad.starting')"><Icon name="globe" />{{ t('kad.connect') }}</button>
        <template v-else>
          <button class="inline-flex items-center gap-1.5 rounded-lg border border-line px-3.5 py-2 text-sm hover:bg-sunk" @click="run('kadRecheck', 'kad.rechecking')"><Icon name="shield" />{{ t('kad.recheck') }}</button>
          <button class="rounded-lg border border-line px-3.5 py-2 text-sm hover:bg-sunk" @click="run('kadDisconnect', 'kad.stopped')">{{ t('kad.disconnect') }}</button>
        </template>
      </div>
    </header>
    <dl v-if="k?.running" class="grid gap-3 sm:grid-cols-2">
      <div class="rounded-xl border border-line bg-surface p-4"><dt class="text-xs uppercase tracking-wide text-mute">{{ t('kad.contacts') }}</dt><dd class="pt-1 text-2xl font-semibold tabular-nums">{{ number(k.contacts ?? 0) }}</dd></div>
      <div class="rounded-xl border border-line bg-surface p-4"><dt class="text-xs uppercase tracking-wide text-mute">{{ t('kad.users') }}</dt><dd class="pt-1 text-2xl font-semibold tabular-nums">{{ number(k.users ?? 0) }}</dd></div>
      <div class="rounded-xl border border-line bg-surface p-4"><dt class="text-xs uppercase tracking-wide text-mute">{{ t('kad.tcpPort') }}</dt><dd class="pt-1 text-lg font-medium" :class="k.tcp_firewalled ? 'text-warn' : 'text-ok'">{{ fw(k.tcp_firewalled) }}</dd></div>
      <div class="rounded-xl border border-line bg-surface p-4"><dt class="text-xs uppercase tracking-wide text-mute">{{ t('kad.udpPort') }}</dt><dd class="pt-1 text-lg font-medium" :class="k.udp_firewalled ? 'text-warn' : 'text-ok'">{{ fw(k.udp_firewalled, k.udp_verified) }}</dd></div>
    </dl>
    <p v-if="k?.running" class="text-sm text-mute">{{ t('kad.firewallHint') }}</p>
    <dl v-if="k?.running && (k.published || k.indexed)" class="grid gap-3 sm:grid-cols-2">
      <div v-if="k.published" class="rounded-xl border border-line bg-surface p-4"><dt class="text-xs uppercase tracking-wide text-mute">{{ t('kad.published') }}</dt><dd class="pt-1 text-lg font-medium tabular-nums">{{ number(k.published.sources) }} / {{ number(k.published.files) }}</dd><p class="text-xs text-mute">{{ t('kad.publishedHint', { notes: number(k.published.notes) }) }}</p></div>
      <div v-if="k.indexed" class="rounded-xl border border-line bg-surface p-4"><dt class="text-xs uppercase tracking-wide text-mute">{{ t('kad.indexed') }}</dt><dd class="pt-1 text-lg font-medium tabular-nums">{{ number(k.indexed.keywords) }} · {{ number(k.indexed.sources) }} · {{ number(k.indexed.notes) }}</dd><p class="text-xs text-mute">{{ t('kad.indexedHint') }}</p></div>
    </dl>
    <section class="flex flex-col gap-3 rounded-xl border border-line bg-surface p-4">
      <h2 class="text-xs uppercase tracking-wide text-mute">{{ t('kad.bootstrapTitle') }}</h2>
      <form class="flex flex-wrap items-end gap-2" @submit.prevent="bootstrap({ url: nodesUrl.trim() })">
        <label class="min-w-0 flex-1 basis-72 text-xs text-mute">{{ t('kad.nodesUrl') }}<input v-model="nodesUrl" type="url" class="mt-1 w-full rounded-lg border border-line bg-bg px-3 py-2 font-mono text-xs text-fg" placeholder="https://…/nodes.dat" /></label>
        <button class="rounded-lg border border-line px-3.5 py-2 text-sm hover:bg-sunk disabled:opacity-50" :disabled="!nodesUrl.trim() || boot">{{ t('kad.download') }}</button>
      </form>
      <form class="flex flex-wrap items-end gap-2" @submit.prevent="bootstrap({ address: nodeAddr.trim() })">
        <label class="min-w-0 flex-1 basis-72 text-xs text-mute">{{ t('kad.nodeAddr') }}<input v-model="nodeAddr" class="mt-1 w-full rounded-lg border border-line bg-bg px-3 py-2 font-mono text-xs text-fg" placeholder="1.2.3.4:4672" /></label>
        <button class="rounded-lg border border-line px-3.5 py-2 text-sm hover:bg-sunk disabled:opacity-50" :disabled="!nodeAddr.trim() || boot">{{ t('kad.askNode') }}</button>
      </form>
      <p class="text-xs text-mute">{{ t('kad.bootstrapHint') }}</p>
    </section>
    <section v-if="upnp" class="flex flex-wrap items-center gap-3 rounded-xl border border-line bg-surface p-4">
      <div class="min-w-0 flex-1"><h2 class="text-xs uppercase tracking-wide text-mute">UPnP</h2>
        <p class="pt-1 text-sm font-medium" :class="upnp.state === 'forwarded' ? 'text-ok' : upnp.state === 'failed' ? 'text-warn' : ''">{{ t('upnp.' + upnp.state) }}</p></div>
      <button class="rounded-lg border border-line px-3.5 py-2 text-sm hover:bg-sunk" @click="toggleUpnp">{{ upnp.enabled ? t('upnp.turnOff') : t('upnp.turnOn') }}</button>
    </section>
  </div>
</template>
