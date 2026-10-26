<script setup lang="ts">
const P: Record<string, string> = {
  dl: 'M12 4v12m0 0l-4-4m4 4l4-4M5 20h14', up: 'M12 20V8m0 0l-4 4m4-4l4 4M5 4h14', search: 'M21 21l-4.3-4.3M10.5 18a7.5 7.5 0 110-15 7.5 7.5 0 010 15z',
  server: 'M4 6h16v5H4zM4 13h16v5H4zM8 8.5h.01M8 15.5h.01', folder: 'M3 7a2 2 0 012-2h4l2 2h8a2 2 0 012 2v8a2 2 0 01-2 2H5a2 2 0 01-2-2z',
  globe: 'M12 21a9 9 0 100-18 9 9 0 000 18zM3 12h18M12 3c3 3.3 3 14.7 0 18M12 3c-3 3.3-3 14.7 0 18', chart: 'M4 20V10M10 20V4M16 20v-7M22 20H2',
  gear: 'M12 15a3 3 0 100-6 3 3 0 000 6zM4 12h2M18 12h2M12 4v2M12 18v2M6.3 6.3l1.4 1.4M16.3 16.3l1.4 1.4M6.3 17.7l1.4-1.4M16.3 7.7l1.4-1.4',
  plus: 'M12 5v14M5 12h14', pause: 'M9 5v14M15 5v14', play: 'M7 4l13 8-13 8z', x: 'M6 6l12 12M18 6L6 18', stop: 'M6 6h12v12H6z',
  link: 'M10 14a4 4 0 005.7 0l3-3a4 4 0 00-5.7-5.7l-1 1M14 10a4 4 0 00-5.7 0l-3 3a4 4 0 005.7 5.7l1-1',
  trash: 'M4 7h16M10 11v6M14 11v6M6 7l1 13h10l1-13M9 7V4h6v3', refresh: 'M20 12a8 8 0 10-2.3 5.7M20 5v7h-7', check: 'M5 13l4 4L19 7',
  back: 'M15 18l-6-6 6-6', chevron: 'M9 6l6 6-6 6', copy: 'M9 9h10v11H9zM5 15V4h10', logout: 'M10 4H5v16h5M16 8l4 4-4 4M20 12H9', sortup: 'M7 14l5-5 5 5', sortdown: 'M7 10l5 5 5-5',
  shield: 'M12 3l8 3v6c0 5-3.5 8-8 9-4.5-1-8-4-8-9V6z', info: 'M12 21a9 9 0 100-18 9 9 0 000 18zM12 11v6M12 7.5h.01', wifi: 'M5 12a10 10 0 0114 0M8 15a6 6 0 018 0M12 19h.01',
  warn: 'M12 4l9 16H3zM12 10v4M12 17h.01', clock: 'M12 21a9 9 0 100-18 9 9 0 000 18zM12 7v5l3 2', hourglass: 'M6 3h12M6 21h12M7 3c0 5 5 6 5 9s-5 4-5 9M17 3c0 5-5 6-5 9s5 4 5 9', list: 'M8 6h13M8 12h13M8 18h13M3 6h.01M3 12h.01M3 18h.01', download: 'M12 4v11m0 0l-4-4m4 4l4-4M5 20h14',
}
defineProps<{ name: string }>()
</script>
<template>
  <svg class="size-4 shrink-0" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path :d="P[name] ?? ''" /></svg>
</template>
