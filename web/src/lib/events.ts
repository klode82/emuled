// Events WebSocket with reconnection (wait 1 s, doubling up to 15 s). The token goes in the address: the browser cannot set headers.
import type { ServerEvent } from '@/types'

export type LinkState = 'connecting' | 'open' | 'closed'

export class EventsClient {
  private ws: WebSocket | null = null
  private timer: ReturnType<typeof setTimeout> | null = null
  private wait = 1000
  private stopped = false
  constructor(
    private token: string,
    private onEvent: (e: ServerEvent) => void,
    private onState: (s: LinkState) => void,
    private makeSocket: (url: string) => WebSocket = (u) => new WebSocket(u),
  ) {}

  private url(): string {
    const proto = location.protocol === 'https:' ? 'wss:' : 'ws:'
    return `${proto}//${location.host}/api/v1/events?token=${encodeURIComponent(this.token)}`
  }
  start(): void {
    this.stopped = false
    this.onState('connecting')
    const ws = this.makeSocket(this.url())
    this.ws = ws
    ws.onopen = () => { this.wait = 1000; this.onState('open') }
    ws.onmessage = (m) => {
      try { this.onEvent(JSON.parse(String(m.data)) as ServerEvent) } catch { /* invalid message: ignored */ }
    }
    ws.onclose = () => {
      this.onState('closed')
      if (this.stopped) return
      this.timer = setTimeout(() => this.start(), this.wait)
      this.wait = Math.min(this.wait * 2, 15000)
    }
    ws.onerror = () => ws.close()
  }
  stop(): void {
    this.stopped = true
    if (this.timer) clearTimeout(this.timer)
    this.ws?.close()
    this.ws = null
  }
}
