import { defineConfig } from 'vitest/config'
import vue from '@vitejs/plugin-vue'
import { fileURLToPath, URL } from 'node:url'

// In sviluppo le chiamate /api (e il WebSocket) vanno al demone: EMULED_API=http://127.0.0.1:8765
const api = process.env.EMULED_API ?? 'http://127.0.0.1:8765'
export default defineConfig({
  plugins: [vue()],
  resolve: { alias: { '@': fileURLToPath(new URL('./src', import.meta.url)) } },
  server: { proxy: { '/api': { target: api, ws: true } } },
  build: { outDir: 'dist', sourcemap: false },
  test: { environment: 'jsdom', globals: true },
})
