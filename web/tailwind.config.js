// I colori sono token CSS (src/style.css): ogni modello di interfaccia ridefinisce gli stessi nomi, quindi i componenti non cambiano.
const c = (n) => `var(--${n})`
export default {
  content: ['./index.html', './src/**/*.{vue,ts}'],
  theme: {
    extend: {
      colors: Object.fromEntries(['bg', 'surface', 'sunk', 'line', 'fg', 'mute', 'accent', 'onaccent', 'soft', 'ok', 'warn', 'bad'].map((n) => [n, c(n)])),
      fontFamily: { sans: ['IBM Plex Sans', 'ui-sans-serif', 'system-ui', 'sans-serif'], mono: ['IBM Plex Mono', 'ui-monospace', 'monospace'] },
    },
  },
}
