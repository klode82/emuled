import { createRouter, createWebHashHistory } from 'vue-router'
import type { Component } from 'vue'

export const SECTIONS = [
  { path: '/transfers', name: 'transfers', icon: 'dl' },
  { path: '/search', name: 'search', icon: 'search' },
  { path: '/servers', name: 'servers', icon: 'server' },
  { path: '/shared', name: 'shared', icon: 'folder' },
  { path: '/kad', name: 'kad', icon: 'globe' },
  { path: '/log', name: 'log', icon: 'list' },
  { path: '/stats', name: 'stats', icon: 'chart' },
  { path: '/prefs', name: 'prefs', icon: 'gear' },
] as const

// Il testo delle voci e' in i18n: nav.<nome> (esteso) e nav.<nome>Short (barra in basso sul telefono)
// La schermata Trasferimenti la porta il template (ogni template ha la sua); le altre sono comuni e prendono i colori del template.
export function createAppRouter(transfers: Component) {
  return createRouter({
    history: createWebHashHistory(),
    scrollBehavior: (to) => (to.hash ? false : { top: 0 }),
    routes: [
      { path: '/', redirect: '/transfers' },
      { path: '/transfers', name: 'transfers', component: transfers },
      { path: '/transfers/uploads', name: 'uploads', component: () => import('@/views/UploadsView.vue') },
      { path: '/search', name: 'search', component: () => import('@/views/SearchView.vue') },
      { path: '/servers', name: 'servers', component: () => import('@/views/ServersView.vue') },
      { path: '/shared', name: 'shared', component: () => import('@/views/SharedView.vue') },
      { path: '/kad', name: 'kad', component: () => import('@/views/KadView.vue') },
      { path: '/log', name: 'log', component: () => import('@/views/LogView.vue') },
      { path: '/stats', name: 'stats', component: () => import('@/views/StatsView.vue') },
      { path: '/prefs', name: 'prefs', component: () => import('@/views/PrefsView.vue') },
      { path: '/:rest(.*)*', redirect: '/transfers' },
    ],
  })
}
