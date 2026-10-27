<script setup lang="ts">
import logo from '@/assets/emuled_logo_transparent.png'
import { computed, onMounted, ref } from 'vue'
import { useI18n } from 'vue-i18n'
import { authStatus } from '@/lib/api'
import { useSession } from '@/stores/session'
import LangSwitch from '@/components/LangSwitch.vue'
const { t } = useI18n()
const s = useSession()
// 'setup' = no account yet (first start); 'account' = username and password; 'token' = the file token (scripts, tools, daemons without accounts)
const mode = ref<'account' | 'setup' | 'token'>('account')
const token = ref(''), username = ref(''), password = ref(''), repeat = ref(''), code = ref('')
const mismatch = ref(false)
const canSubmit = computed(() => !s.busy && (mode.value === 'token' ? !!token.value.trim() : mode.value === 'setup' ? !!(code.value.trim() && username.value.trim() && password.value && repeat.value) : !!(username.value.trim() && password.value)))
onMounted(async () => {
  try { mode.value = (await authStatus()).setup_required ? 'setup' : 'account' } catch { mode.value = 'token' }		// daemon without accounts (or old): the token stays
})
async function submit() {
  mismatch.value = false
  if (mode.value === 'token') await s.login(token.value)
  else if (mode.value === 'setup') { if (password.value !== repeat.value) { mismatch.value = true; return } await s.setupAccount(code.value, username.value, password.value) }
  else await s.loginUser(username.value, password.value)
}
</script>
<template>
  <main class="grid min-h-full place-items-center bg-bg p-4">
    <form :key="mode" class="w-full max-w-md rounded-xl border border-line bg-surface p-6 shadow-sm" @submit.prevent="submit">
      <div class="flex items-center gap-3 pb-5">
        <img :src="logo" alt="" class="size-10 shrink-0 object-contain" />
        <div class="min-w-0 flex-1"><h1 class="text-lg font-semibold leading-tight">emuled</h1><p class="text-sm text-mute">{{ mode === 'setup' ? t('login.setupTitle') : t('login.subtitle') }}</p></div>
      </div>

      <template v-if="mode === 'token'">
        <label class="flex flex-col gap-1.5 text-sm font-medium">{{ t('login.token') }}
          <input v-model="token" type="password" autocomplete="current-password" autofocus required class="rounded-lg border border-line bg-bg px-3 py-2.5 font-mono text-sm font-normal" :placeholder="t('login.placeholder')" />
        </label>
        <p class="mt-3 text-xs leading-relaxed text-mute">{{ t('login.tokenHelp') }}</p>
      </template>
      <template v-else>
        <label v-if="mode === 'setup'" class="mb-3 flex flex-col gap-1.5 text-sm font-medium">{{ t('login.setupCode') }}
          <input v-model="code" type="password" autocomplete="off" autofocus required class="rounded-lg border border-line bg-bg px-3 py-2.5 font-mono text-sm font-normal" />
          <span class="text-xs font-normal leading-relaxed text-mute">{{ t('login.setupCodeHint') }}</span>
        </label>
        <label class="flex flex-col gap-1.5 text-sm font-medium">{{ t('login.username') }}
          <input v-model="username" type="text" autocomplete="username" :autofocus="mode === 'account'" required maxlength="32" class="rounded-lg border border-line bg-bg px-3 py-2.5 text-sm font-normal" />
        </label>
        <label class="mt-3 flex flex-col gap-1.5 text-sm font-medium">{{ mode === 'setup' ? t('login.newPassword') : t('login.password') }}
          <input v-model="password" type="password" :autocomplete="mode === 'setup' ? 'new-password' : 'current-password'" required class="rounded-lg border border-line bg-bg px-3 py-2.5 text-sm font-normal" />
        </label>
        <label v-if="mode === 'setup'" class="mt-3 flex flex-col gap-1.5 text-sm font-medium">{{ t('login.repeatPassword') }}
          <input v-model="repeat" type="password" autocomplete="new-password" required class="rounded-lg border border-line bg-bg px-3 py-2.5 text-sm font-normal" />
        </label>
      </template>

      <p v-if="mismatch" class="mt-3 rounded-lg border border-bad px-3 py-2 text-sm text-bad" role="alert">{{ t('login.passwordsDiffer') }}</p>
      <p v-if="s.error" class="mt-3 rounded-lg border border-bad px-3 py-2 text-sm text-bad" role="alert">{{ s.error }}</p>
      <button class="mt-4 w-full rounded-lg bg-accent px-4 py-2.5 text-sm font-medium text-onaccent" :disabled="!canSubmit">{{ s.busy ? t('login.checking') : mode === 'setup' ? t('login.createAccount') : t('login.enter') }}</button>
      <p class="mt-4 text-xs leading-relaxed text-mute">{{ t('login.help') }}</p>
      <div class="mt-4 flex flex-wrap items-center justify-between gap-2">
        <button v-if="mode !== 'setup'" type="button" class="text-xs text-accent underline" @click="mode = mode === 'token' ? 'account' : 'token'; s.error = ''">{{ mode === 'token' ? t('login.useAccount') : t('login.useToken') }}</button><span v-else></span>
        <LangSwitch />
      </div>
    </form>
  </main>
</template>
