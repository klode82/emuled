<!-- Wizard di primo avvio (Bench 873): nome sulla rete, porte, offuscamento, limiti, cartelle. Compare dopo la creazione del primo account, a schermo intero, e sparisce quando si finisce. -->
<script setup lang="ts">
import logo from '@/assets/emuled_logo_transparent.png'
import { computed, onMounted, ref } from 'vue'
import { useI18n } from 'vue-i18n'
import { useSession } from '@/stores/session'
import { useSetup } from '@/stores/setup'
import { errText } from '@/stores/toasts'
import type { ImportRule, ImportScan, PortsCheck } from '@/types'
import { bytes } from '@/lib/format'
import LangSwitch from '@/components/LangSwitch.vue'
import FolderPicker from '@/components/FolderPicker.vue'

const { t } = useI18n()
const session = useSession(), setup = useSetup()
const candidates = ref<string[]>([])		// cartelle del computer che sembrano un eMule (anche nascoste, come ~/.emuled.old)
const importAvailable = ref(false)		// primo avvio e niente ancora importato: si offre di portare il profilo di un eMule esistente (Bench 875)
const STEPS = computed(() => (importAvailable.value ? ['welcome', 'import', 'nick', 'ports', 'obfuscation', 'limits', 'folders', 'done'] : ['welcome', 'nick', 'ports', 'obfuscation', 'limits', 'folders', 'done']) as string[])
const step = ref(0)
const loading = ref(true), saving = ref(false), error = ref('')

const nick = ref(''), tcp = ref(55662), udp = ref(55672), upnp = ref(false), obfuscation = ref(true)
const down = ref(0), up = ref(0), downUnit = ref<'kB' | 'MB'>('kB'), upUnit = ref<'kB' | 'MB'>('kB')
const incoming = ref(''), temp = ref('')
const ports = ref<PortsCheck | null>(null), checking = ref(false)
const restart = ref<string[]>([])
const pickFor = ref<'incoming' | 'temp' | 'src' | number | null>(null)		// number = indice della regola dei percorsi Windows
function useCandidate(path: string) { src.value = path; scan.value = null; void look() }
function picked(path: string) {
  if (pickFor.value === 'incoming') incoming.value = path; else if (pickFor.value === 'temp') temp.value = path
  else if (pickFor.value === 'src') { src.value = path; scan.value = null } else if (typeof pickFor.value === 'number') { rules.value[pickFor.value].linux = path; void look() }
  pickFor.value = null
}
const pickStart = computed(() => (pickFor.value === 'incoming' ? incoming.value : pickFor.value === 'temp' ? temp.value : pickFor.value === 'src' ? src.value : typeof pickFor.value === 'number' ? rules.value[pickFor.value]?.linux : ''))

// importazione di un profilo eMule esistente
const importYes = ref(false), src = ref(''), scan = ref<ImportScan | null>(null), rules = ref<ImportRule[]>([]), scanning = ref(false), importing = ref(false)
async function look() {
  if (!src.value.trim().startsWith('/')) return
  scanning.value = true; error.value = ''
  try {
    const sc = await session.getApi().importScan(src.value.trim(), rules.value.length ? rules.value : undefined)
    scan.value = sc; rules.value = sc.rules.map((r) => ({ ...r }))
  } catch (e) { scan.value = null; error.value = errText(e) } finally { scanning.value = false }
}
async function runImport() {
  importing.value = true; error.value = ''
  try {
    const api = session.getApi()
    await api.importRun(src.value.trim(), rules.value)
    const t0 = Date.now()
    await new Promise((r) => setTimeout(r, 2500))		// il demone si riavvia dopo meno di un secondo
    for (;;) {
      try { if ((await api.importStatus()).imported) { location.reload(); return } } catch { /* sta ripartendo */ }
      if (Date.now() - t0 > 90000) { error.value = t('wizard.import.timeout'); break }
      await new Promise((r) => setTimeout(r, 1500))
    }
  } catch (e) { error.value = t('wizard.import.error', { msg: errText(e) }) }
  importing.value = false
}

onMounted(async () => {
  try {
    const api = session.getApi()
    try { const st = await api.importStatus(); importAvailable.value = st.available; candidates.value = st.candidates ?? [] } catch { importAvailable.value = false }		// un demone senza questa funzione: niente passo
    const [p, d] = await Promise.all([api.preferences(), api.directories()])
    nick.value = String(p.nick ?? ''); tcp.value = Number(p.tcp_port ?? 55662); udp.value = Number(p.udp_port ?? 55672); upnp.value = !!p.upnp_enabled
    obfuscation.value = p.crypt_supported !== false
    const split = (kb: number): [number, 'kB' | 'MB'] => (kb >= 1024 && kb % 1024 === 0 ? [kb / 1024, 'MB'] : [kb, 'kB'])
    const dl = split(Number(p.max_download_kbps ?? 0)), ul = split(Number(p.max_upload_kbps ?? 0))
    down.value = dl[0]; downUnit.value = dl[1]; up.value = ul[0]; upUnit.value = ul[1]
    incoming.value = d.incoming; temp.value = d.temp[0] ?? ''
  } catch (e) { error.value = errText(e) }
  loading.value = false
})

const kbps = (v: number, unit: 'kB' | 'MB') => Math.max(0, Math.round(Number(v) || 0) * (unit === 'MB' ? 1024 : 1))
const last = computed(() => STEPS.value.length - 1)
const name = computed(() => STEPS.value[step.value])
const nickOk = computed(() => nick.value.trim().length > 0)
const portsValid = computed(() => Number.isInteger(tcp.value) && tcp.value >= 1 && tcp.value <= 65535 && Number.isInteger(udp.value) && udp.value >= 1 && udp.value <= 65535)
const foldersOk = computed(() => incoming.value.trim().startsWith('/') && temp.value.trim().startsWith('/'))
const canNext = computed(() => (name.value === 'import' && importYes.value ? !!scan.value : name.value === 'nick' ? nickOk.value : name.value === 'ports' ? portsValid.value : name.value === 'folders' ? foldersOk.value : true))

async function checkPorts() {
  if (!portsValid.value) return
  checking.value = true
  try { ports.value = await session.getApi().portsCheck(tcp.value, udp.value) } catch { ports.value = null } finally { checking.value = false }
}
function standardPorts() { tcp.value = 55662; udp.value = 55672; void checkPorts() }
const portState = (p?: { valid: boolean; free: boolean; own: boolean }) => (!p ? '' : !p.valid ? 'invalid' : p.own ? 'own' : p.free ? 'free' : 'busy')
const toneOf = (s: string) => (s === 'busy' || s === 'invalid' ? 'text-bad' : 'text-ok')

async function next() {
  error.value = ''
  if (name.value === 'ports' && !ports.value) await checkPorts()		// l'avviso non blocca: serve solo a far sapere
  if (name.value === 'folders') { await save(); return }
  if (name.value === 'import' && importYes.value) { await runImport(); return }
  step.value = Math.min(step.value + 1, last.value)
}
function back() { error.value = ''; step.value = Math.max(0, step.value - 1) }

async function save() {
  saving.value = true; error.value = ''
  try {
    const api = session.getApi()
    const r = await api.patchPreferences({
      nick: nick.value.trim(), tcp_port: tcp.value, udp_port: udp.value, upnp_enabled: upnp.value,
      crypt_supported: obfuscation.value, crypt_requested: obfuscation.value,
      max_download_kbps: kbps(down.value, downUnit.value), max_upload_kbps: kbps(up.value, upUnit.value),
    })
    restart.value = r.restart_required
    await api.putDirectories({ incoming: incoming.value.trim(), temp: [temp.value.trim()] })
    step.value = last.value
  } catch (e) { error.value = t('wizard.error', { msg: errText(e) }) } finally { saving.value = false }
}
async function finish() {
  saving.value = true
  try { await setup.finish() } catch (e) { error.value = errText(e) } finally { saving.value = false }
}
async function finishAndRestart() {		// le porte nuove valgono solo dopo il riavvio: si chiude il Wizard (cosi' non riparte) e si riavvia
  saving.value = true; error.value = ''
  try { await setup.finish() } catch (e) { error.value = errText(e); saving.value = false; return }
  error.value = await session.restartDaemon()
  saving.value = false
}
const speedText = (v: number, u: string) => (kbps(v, u as 'kB' | 'MB') === 0 ? t('wizard.unlimitedShort') : `${v} ${u}/s`)
</script>

<template>
  <main class="grid min-h-full place-items-center overflow-y-auto bg-bg p-4">
    <section class="w-full max-w-xl rounded-xl border border-line bg-surface p-6 shadow-sm" :aria-label="t('wizard.title')">
      <header class="flex items-center gap-3 pb-4">
        <img :src="logo" alt="" class="size-10 shrink-0 object-contain" />
        <div class="min-w-0 flex-1"><h1 class="text-lg font-semibold leading-tight">{{ t('wizard.title') }}</h1><p class="text-sm text-mute">{{ t('wizard.step', { n: step + 1, total: STEPS.length }) }}</p></div>
        <LangSwitch />
      </header>
      <div class="mb-5 flex gap-1.5" role="presentation"><span v-for="(s, i) in STEPS" :key="s" class="h-1.5 flex-1 rounded-full" :class="i <= step ? 'bg-accent' : 'bg-sunk'"></span></div>

      <p v-if="loading" class="py-8 text-center text-sm text-mute" role="status">{{ t('app.connecting') }}</p>
      <template v-else>
        <!-- 1. benvenuto -->
        <div v-if="name === 'welcome'" class="flex flex-col gap-2">
          <h2 class="text-base font-semibold">{{ t('wizard.welcomeTitle') }}</h2>
          <p class="text-sm leading-relaxed text-mute">{{ t('wizard.welcomeText') }}</p>
        </div>

        <!-- 1b. importazione di un eMule esistente (solo se offerta) -->
        <div v-else-if="name === 'import'" class="flex flex-col gap-3">
          <h2 class="text-base font-semibold">{{ t('wizard.import.title') }}</h2>
          <p class="text-sm leading-relaxed text-mute">{{ t('wizard.import.text') }}</p>
          <label class="flex items-center gap-2 text-sm"><input v-model="importYes" type="radio" :value="false" name="imp" class="size-4" :disabled="importing" />{{ t('wizard.import.no') }}</label>
          <label class="flex items-center gap-2 text-sm"><input v-model="importYes" type="radio" :value="true" name="imp" class="size-4" :disabled="importing" />{{ t('wizard.import.yes') }}</label>
          <template v-if="importYes">
            <div v-if="candidates.length" class="flex flex-col gap-1.5 text-sm"><span class="font-medium">{{ t('wizard.import.candidates') }}</span>
              <button v-for="c in candidates" :key="c" type="button" class="break-all rounded-lg border border-line px-3 py-2 text-left font-mono text-xs hover:bg-sunk" :class="src === c ? 'border-accent' : ''" :disabled="importing" @click="useCandidate(c)">{{ c }}</button></div>
            <div class="flex flex-col gap-1.5 text-sm font-medium"><label for="wz-src">{{ t('wizard.import.srcLabel') }}</label>
              <div class="flex gap-2"><input id="wz-src" v-model="src" type="text" class="min-w-0 flex-1 rounded-lg border border-line bg-bg px-3 py-2.5 font-mono text-sm font-normal" :disabled="importing" @input="scan = null" @keydown.enter.prevent="look" />
                <button type="button" class="rounded-lg border border-line px-3 text-sm font-normal hover:bg-sunk" :disabled="importing" @click="pickFor = 'src'">{{ t('picker.browse') }}</button>
                <button type="button" class="rounded-lg border border-line px-3 text-sm font-normal hover:bg-sunk disabled:opacity-50" :disabled="scanning || importing || !src.trim().startsWith('/')" @click="look">{{ scanning ? t('login.checking') : t('wizard.import.look') }}</button></div></div>
            <template v-if="scan">
              <p class="rounded-lg bg-sunk px-3 py-2 text-sm" role="status">{{ t('wizard.import.found', { files: scan.files.length, size: bytes(scan.total_bytes), nick: scan.nick || '—' }) }}</p>
              <div v-if="scan.dirs.length" class="flex flex-col gap-2">
                <h3 class="text-sm font-semibold">{{ t('wizard.import.rulesTitle') }}</h3>
                <p class="text-xs text-mute">{{ rules.length ? t('wizard.import.rulesText') : t('wizard.import.rulesNone') }}</p>
                <div v-for="(r, i) in rules" :key="r.win" class="flex items-center gap-2 text-sm">
                  <span class="shrink-0 font-mono text-xs">{{ r.win }}</span><span aria-hidden="true">→</span>
                  <input v-model="r.linux" type="text" class="min-w-0 flex-1 rounded-lg border border-line bg-bg px-2 py-1.5 font-mono text-xs" :aria-label="r.win" :disabled="importing" @change="look" />
                  <button type="button" class="rounded-lg border border-line px-2 py-1.5 text-xs hover:bg-sunk" :disabled="importing" @click="pickFor = i">{{ t('picker.browse') }}</button>
                </div>
                <h3 class="pt-1 text-sm font-semibold">{{ t('wizard.import.dirsTitle') }}</h3>
                <ul class="flex flex-col gap-1 text-xs">
                  <li v-for="d in scan.dirs" :key="d.windows" class="flex flex-wrap items-baseline gap-x-2"><span class="break-all font-mono">{{ d.windows }}</span>
                    <span :class="d.exists ? 'text-ok' : d.mapped ? 'text-warn' : 'text-mute'">{{ d.exists ? t('wizard.import.dirOk') : d.mapped ? t('wizard.import.dirMissing') : t('wizard.import.dirUnmapped') }}</span></li>
                </ul>
              </div>
              <p class="text-xs text-mute">{{ t('wizard.import.copyNote') }}</p>
            </template>
            <p v-if="importing" class="rounded-lg border border-line px-3 py-2 text-sm" role="status">{{ t('wizard.import.running') }}</p>
          </template>
        </div>

        <!-- 2. nome sulla rete -->
        <div v-else-if="name === 'nick'" class="flex flex-col gap-3">
          <h2 class="text-base font-semibold">{{ t('wizard.nickTitle') }}</h2>
          <p class="text-sm leading-relaxed text-mute">{{ t('wizard.nickText') }}</p>
          <label class="flex flex-col gap-1.5 text-sm font-medium">{{ t('wizard.nickLabel') }}
            <input v-model="nick" type="text" maxlength="50" autofocus class="rounded-lg border border-line bg-bg px-3 py-2.5 text-sm font-normal" @keydown.enter.prevent="canNext && next()" />
          </label>
        </div>

        <!-- 3. porte -->
        <div v-else-if="name === 'ports'" class="flex flex-col gap-3">
          <h2 class="text-base font-semibold">{{ t('wizard.portsTitle') }}</h2>
          <p class="text-sm leading-relaxed text-mute">{{ t('wizard.portsText') }}</p>
          <div class="grid gap-3 sm:grid-cols-2">
            <label class="flex flex-col gap-1.5 text-sm font-medium">{{ t('wizard.tcpLabel') }}
              <input v-model.number="tcp" type="number" min="1" max="65535" class="rounded-lg border border-line bg-bg px-3 py-2.5 text-sm font-normal tabular-nums" @change="checkPorts" />
              <span v-if="ports?.tcp" class="text-xs font-normal" :class="toneOf(portState(ports.tcp))">{{ t('wizard.port.' + portState(ports.tcp)) }}</span>
            </label>
            <label class="flex flex-col gap-1.5 text-sm font-medium">{{ t('wizard.udpLabel') }}
              <input v-model.number="udp" type="number" min="1" max="65535" class="rounded-lg border border-line bg-bg px-3 py-2.5 text-sm font-normal tabular-nums" @change="checkPorts" />
              <span v-if="ports?.udp" class="text-xs font-normal" :class="toneOf(portState(ports.udp))">{{ t('wizard.port.' + portState(ports.udp)) }}</span>
            </label>
          </div>
          <div class="flex flex-wrap gap-2">
            <button type="button" class="rounded-lg border border-line px-3 py-1.5 text-xs hover:bg-sunk" @click="standardPorts">{{ t('wizard.useStandard') }}</button>
            <button type="button" class="rounded-lg border border-line px-3 py-1.5 text-xs hover:bg-sunk disabled:opacity-50" :disabled="checking || !portsValid" @click="checkPorts">{{ checking ? t('login.checking') : t('wizard.check') }}</button>
          </div>
          <label class="flex items-start gap-2 text-sm"><input v-model="upnp" type="checkbox" class="mt-0.5 size-4" /><span>{{ t('wizard.upnp') }}</span></label>
        </div>

        <!-- 4. offuscamento -->
        <div v-else-if="name === 'obfuscation'" class="flex flex-col gap-3">
          <h2 class="text-base font-semibold">{{ t('wizard.obfTitle') }}</h2>
          <p class="text-sm leading-relaxed text-mute">{{ t('wizard.obfText') }}</p>
          <label class="flex items-center gap-2 text-sm"><input v-model="obfuscation" type="radio" :value="true" name="obf" class="size-4" />{{ t('wizard.obfYes') }}</label>
          <label class="flex items-center gap-2 text-sm"><input v-model="obfuscation" type="radio" :value="false" name="obf" class="size-4" />{{ t('wizard.obfNo') }}</label>
        </div>

        <!-- 5. limiti -->
        <div v-else-if="name === 'limits'" class="flex flex-col gap-3">
          <h2 class="text-base font-semibold">{{ t('wizard.limitsTitle') }}</h2>
          <p class="text-sm leading-relaxed text-mute">{{ t('wizard.limitsText') }}</p>
          <div class="grid gap-3 sm:grid-cols-2">
            <div class="flex flex-col gap-1.5 text-sm font-medium"><label for="wz-down">{{ t('wizard.downLabel') }}</label>
              <div class="flex gap-2"><input id="wz-down" v-model.number="down" type="number" min="0" class="min-w-0 flex-1 rounded-lg border border-line bg-bg px-3 py-2.5 text-sm font-normal tabular-nums" />
                <select v-model="downUnit" class="rounded-lg border border-line bg-bg px-2 text-sm font-normal" :aria-label="t('wizard.downLabel')"><option value="kB">kB/s</option><option value="MB">MB/s</option></select></div></div>
            <div class="flex flex-col gap-1.5 text-sm font-medium"><label for="wz-up">{{ t('wizard.upLabel') }}</label>
              <div class="flex gap-2"><input id="wz-up" v-model.number="up" type="number" min="0" class="min-w-0 flex-1 rounded-lg border border-line bg-bg px-3 py-2.5 text-sm font-normal tabular-nums" />
                <select v-model="upUnit" class="rounded-lg border border-line bg-bg px-2 text-sm font-normal" :aria-label="t('wizard.upLabel')"><option value="kB">kB/s</option><option value="MB">MB/s</option></select></div></div>
          </div>
          <p class="text-xs text-mute">{{ t('wizard.unlimited') }}</p>
        </div>

        <!-- 6. cartelle -->
        <div v-else-if="name === 'folders'" class="flex flex-col gap-3">
          <h2 class="text-base font-semibold">{{ t('wizard.foldersTitle') }}</h2>
          <p class="text-sm leading-relaxed text-mute">{{ t('wizard.foldersText') }}</p>
          <div class="flex flex-col gap-1.5 text-sm font-medium"><label for="wz-in">{{ t('wizard.incomingLabel') }}</label>
            <div class="flex gap-2"><input id="wz-in" v-model="incoming" type="text" class="min-w-0 flex-1 rounded-lg border border-line bg-bg px-3 py-2.5 font-mono text-sm font-normal" />
              <button type="button" class="rounded-lg border border-line px-3 text-sm font-normal hover:bg-sunk" @click="pickFor = 'incoming'">{{ t('picker.browse') }}</button></div></div>
          <div class="flex flex-col gap-1.5 text-sm font-medium"><label for="wz-tmp">{{ t('wizard.tempLabel') }}</label>
            <div class="flex gap-2"><input id="wz-tmp" v-model="temp" type="text" class="min-w-0 flex-1 rounded-lg border border-line bg-bg px-3 py-2.5 font-mono text-sm font-normal" />
              <button type="button" class="rounded-lg border border-line px-3 text-sm font-normal hover:bg-sunk" @click="pickFor = 'temp'">{{ t('picker.browse') }}</button></div></div>
          <p class="text-xs text-mute">{{ t('picker.note') }}</p>
          <p v-if="!foldersOk" class="text-xs text-warn">{{ t('wizard.absolute') }}</p>
        </div>

        <!-- 7. fine -->
        <div v-else class="flex flex-col gap-3">
          <h2 class="text-base font-semibold">{{ t('wizard.doneTitle') }}</h2>
          <p class="text-sm leading-relaxed text-mute">{{ t('wizard.doneText') }}</p>
          <dl class="grid gap-x-4 gap-y-1 rounded-lg bg-sunk p-3 text-sm sm:grid-cols-[auto_1fr]" :aria-label="t('wizard.summary')">
            <dt class="text-mute">{{ t('wizard.nickLabel') }}</dt><dd>{{ nick }}</dd>
            <dt class="text-mute">TCP / UDP</dt><dd class="tabular-nums">{{ tcp }} / {{ udp }}{{ upnp ? ' · UPnP' : '' }}</dd>
            <dt class="text-mute">{{ t('wizard.obfTitle') }}</dt><dd>{{ obfuscation ? t('wizard.obfYes') : t('wizard.obfNo') }}</dd>
            <dt class="text-mute">{{ t('wizard.downLabel') }} / {{ t('wizard.upLabel') }}</dt><dd class="tabular-nums">{{ speedText(down, downUnit) }} / {{ speedText(up, upUnit) }}</dd>
            <dt class="text-mute">Incoming</dt><dd class="break-all font-mono text-xs">{{ incoming }}</dd>
            <dt class="text-mute">Temp</dt><dd class="break-all font-mono text-xs">{{ temp }}</dd>
          </dl>
          <p v-if="restart.length" class="rounded-lg border border-warn px-3 py-2 text-sm text-warn" role="status">{{ t('wizard.restart') }}</p>
        </div>

        <p v-if="error" class="mt-4 rounded-lg border border-bad px-3 py-2 text-sm text-bad" role="alert">{{ error }}</p>
        <div class="mt-6 flex items-center justify-between gap-2">
          <button v-if="step > 0 && name !== 'done'" type="button" class="rounded-lg border border-line px-4 py-2 text-sm hover:bg-sunk" :disabled="saving || importing" @click="back">{{ t('wizard.back') }}</button><span v-else></span>
          <div v-if="name === 'done'" class="flex flex-wrap justify-end gap-2">
            <button v-if="restart.length" type="button" class="rounded-lg border border-line px-4 py-2.5 text-sm hover:bg-sunk disabled:opacity-50" :disabled="saving" @click="finish">{{ t('wizard.finish') }}</button>
            <button v-if="restart.length" type="button" class="rounded-lg bg-accent px-5 py-2.5 text-sm font-medium text-onaccent disabled:opacity-50" :disabled="saving" @click="finishAndRestart">{{ saving ? t('prefs.daemon.restarting') : t('wizard.finishRestart') }}</button>
            <button v-else type="button" class="rounded-lg bg-accent px-5 py-2.5 text-sm font-medium text-onaccent disabled:opacity-50" :disabled="saving" @click="finish">{{ t('wizard.finish') }}</button>
          </div>
          <button v-else type="button" class="rounded-lg bg-accent px-5 py-2.5 text-sm font-medium text-onaccent disabled:opacity-50" :disabled="!canNext || saving || importing" @click="next">{{ saving ? t('wizard.saving') : name === 'folders' ? t('wizard.save') : name === 'import' && importYes ? t('wizard.import.run') : t('wizard.next') }}</button>
        </div>
      </template>
    </section>
    <FolderPicker :open="pickFor !== null" :start="pickStart" :need-write="pickFor === 'incoming' || pickFor === 'temp'" :show-hidden="pickFor === 'src'" @close="pickFor = null" @pick="picked" />
  </main>
</template>
