<script setup lang="ts">
import { onMounted, ref, type Component } from 'vue'
import { useI18n } from 'vue-i18n'
import { useSession } from '@/stores/session'
import LoginView from '@/views/LoginView.vue'
import ToastHost from '@/components/ToastHost.vue'
import ConfirmDialog from '@/components/ConfirmDialog.vue'
import AddDialog from '@/components/AddDialog.vue'

defineProps<{ shell: Component }>()
const { t } = useI18n()
const session = useSession()
const starting = ref(!!session.token)
onMounted(async () => { await session.resume(); starting.value = false })
</script>
<template>
  <div v-if="starting" class="grid h-full place-items-center text-sm text-mute" role="status">{{ t('app.connecting') }}</div>
  <template v-else>
    <component :is="shell" v-if="session.ready" />
    <LoginView v-else />
    <AddDialog /><ConfirmDialog /><ToastHost />
  </template>
</template>
