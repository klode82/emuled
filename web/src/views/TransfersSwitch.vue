<!-- La schermata Trasferimenti cambia con il modello di interfaccia: qui si sceglie quale mostrare -->
<script setup lang="ts">
import { defineAsyncComponent } from 'vue'
import { useUi } from '@/stores/ui'
const ui = useUi()
const VIEWS = {
  console: defineAsyncComponent(() => import('@/skins/console/TransfersView.vue')),
  dashboard: defineAsyncComponent(() => import('@/skins/dashboard/TransfersView.vue')),
  studio: defineAsyncComponent(() => import('@/skins/studio/TransfersView.vue')),
  mobile: defineAsyncComponent(() => import('@/skins/mobile/TransfersView.vue')),
}
</script>
<template><component :is="VIEWS[ui.skin]" /></template>
