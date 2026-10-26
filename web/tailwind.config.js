// I colori sono token CSS (templates/<id>/tokens.css): ogni template ridefinisce gli stessi nomi, quindi i componenti comuni non cambiano.
// Le classi si cercano nel kit (src/) e nel template che si sta costruendo (TEMPLATE=<id>): ogni template ha solo il CSS che usa.
const c = (n) => `var(--${n})`
const id = process.env.TEMPLATE ?? 'console'
export default {
  content: [`./templates/${id}/**/*.{html,vue,ts}`, './src/**/*.{vue,ts}'],
  theme: {
    extend: {
      colors: Object.fromEntries(['bg', 'surface', 'sunk', 'line', 'fg', 'mute', 'accent', 'onaccent', 'soft', 'ok', 'warn', 'bad'].map((n) => [n, c(n)])),
      fontFamily: { sans: ['var(--font-sans)'], mono: ['var(--font-mono)'] },		// il carattere e' del template (--font-sans e --font-mono in tokens.css)
    },
  },
}
