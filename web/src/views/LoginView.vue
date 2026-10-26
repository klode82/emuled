<script setup lang="ts">
import logo from '@/assets/emuled_logo_transparent.png'
import { ref } from 'vue'
import { useI18n } from 'vue-i18n'
import { useSession } from '@/stores/session'
import LangSwitch from '@/components/LangSwitch.vue'
const { t } = useI18n()
const s = useSession()
const token = ref('')
</script>
<template>
  <main class="grid min-h-full place-items-center bg-bg p-4">
    <form class="w-full max-w-md rounded-xl border border-line bg-surface p-6 shadow-sm" @submit.prevent="s.login(token)">
      <div class="flex items-center gap-3 pb-5">
        <img :src="logo" alt="" class="size-10 shrink-0 object-contain" />
        <div class="min-w-0 flex-1"><h1 class="text-lg font-semibold leading-tight">emuled</h1><p class="text-sm text-mute">{{ t('login.subtitle') }}</p></div>
      </div>
      <label class="flex flex-col gap-1.5 text-sm font-medium">{{ t('login.token') }}
        <input v-model="token" type="password" autocomplete="current-password" autofocus required class="rounded-lg border border-line bg-bg px-3 py-2.5 font-mono text-sm font-normal" :placeholder="t('login.placeholder')" />
      </label>
      <p v-if="s.error" class="mt-3 rounded-lg border border-bad px-3 py-2 text-sm text-bad" role="alert">{{ s.error }}</p>
      <button class="mt-4 w-full rounded-lg bg-accent px-4 py-2.5 text-sm font-medium text-onaccent" :disabled="s.busy || !token.trim()">{{ s.busy ? t('login.checking') : t('login.enter') }}</button>
      <p class="mt-4 text-xs leading-relaxed text-mute">{{ t('login.help') }}</p>
      <div class="mt-4 flex justify-end"><LangSwitch /></div>
    </form>
  </main>
</template>
