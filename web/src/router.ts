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

// The item text is in i18n: nav.<name> (long) and nav.<name>Short (bottom bar on the phone)
// The Transfers and Upload screens come from the template (each template has its own; Upload is the shared one if the template has none); the others are shared and take the template colors.
export function createAppRouter(transfers: Component, uploads?: Component) {
  return createRouter({
    history: createWebHashHistory(),
    scrollBehavior: (to) => (to.hash ? false : { top: 0 }),
    routes: [
      { path: '/', redirect: '/transfers' },
      { path: '/transfers', name: 'transfers', component: transfers },
      { path: '/transfers/uploads', name: 'uploads', component: uploads ?? (() => import('@/views/UploadsView.vue')) },
      { path: '/search', name: 'search', component: () => import('@/views/SearchView.vue') },
      { path: '/servers', name: 'servers', component: () => import('@/views/ServersView.vue') },
      { path: '/shared', name: 'shared', component: () => import('@/views/SharedView.vue') },
      { path: '/kad', name: 'kad', component: () => import('@/views/KadView.vue') },
      { path: '/log', name: 'log', component: () => import('@/views/LogView.vue') },
      { path: '/stats', name: 'stats', component: () => import('@/views/StatsView.vue') },
      { path: '/prefs/:page?', name: 'prefs', component: () => import('@/views/PrefsView.vue') },
      { path: '/:rest(.*)*', redirect: '/transfers' },
    ],
  })
}
