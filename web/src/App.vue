<script setup lang="ts">
import { onMounted, ref } from 'vue'
import { useI18n } from 'vue-i18n'
import { useSession } from '@/stores/session'
import LoginView from '@/views/LoginView.vue'
import { useUi } from '@/stores/ui'
import { defineAsyncComponent } from 'vue'
import ToastHost from '@/components/ToastHost.vue'
import ConfirmDialog from '@/components/ConfirmDialog.vue'
import AddDialog from '@/components/AddDialog.vue'

const { t } = useI18n()
const session = useSession()
const ui = useUi()
// il guscio cambia con il modello di interfaccia scelto (A Console, B Cruscotto, C Studio, D Mobile first)
const SHELLS = {
  console: defineAsyncComponent(() => import('@/skins/console/ConsoleShell.vue')),
  dashboard: defineAsyncComponent(() => import('@/skins/dashboard/DashboardShell.vue')),
  studio: defineAsyncComponent(() => import('@/skins/studio/StudioShell.vue')),
  mobile: defineAsyncComponent(() => import('@/skins/mobile/MobileShell.vue')),
}
const starting = ref(!!session.token)
onMounted(async () => { await session.resume(); starting.value = false })
</script>
<template>
  <div v-if="starting" class="grid h-full place-items-center text-sm text-mute" role="status">{{ t('app.connecting') }}</div>
  <template v-else>
    <component :is="SHELLS[ui.skin]" v-if="session.ready" />
    <LoginView v-else />
    <AddDialog /><ConfirmDialog /><ToastHost />
  </template>
</template>
