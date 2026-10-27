<script setup lang="ts">
import { onMounted, ref, watch, type Component } from 'vue'
import { useI18n } from 'vue-i18n'
import { useSession } from '@/stores/session'
import { useSetup } from '@/stores/setup'
import LoginView from '@/views/LoginView.vue'
import WizardView from '@/views/WizardView.vue'
import ToastHost from '@/components/ToastHost.vue'
import ConfirmDialog from '@/components/ConfirmDialog.vue'
import AddDialog from '@/components/AddDialog.vue'

defineProps<{ shell: Component }>()
const { t } = useI18n()
const session = useSession()
const setup = useSetup()
const starting = ref(!!session.token)
// dopo l'accesso si chiede al demone se il primo avvio e' da completare (Wizard); all'uscita si dimentica
watch(() => session.ready, (r) => { if (r) void setup.check(); else setup.reset() }, { immediate: true })
onMounted(async () => { await session.resume(); starting.value = false })
</script>
<template>
  <div v-if="starting" class="grid h-full place-items-center text-sm text-mute" role="status">{{ t('app.connecting') }}</div>
  <template v-else>
    <div v-if="session.ready && !setup.checked" class="grid h-full place-items-center text-sm text-mute" role="status">{{ t('app.connecting') }}</div>
    <WizardView v-else-if="session.ready && setup.pending" />
    <component :is="shell" v-else-if="session.ready" />
    <LoginView v-else />
    <AddDialog /><ConfirmDialog /><ToastHost />
  </template>
</template>
