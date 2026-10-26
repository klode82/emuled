import { beforeEach, describe, expect, it, vi } from 'vitest'
import { createPinia, setActivePinia } from 'pinia'
import { currentTemplateId, DEFAULT_TEMPLATE, TEMPLATE_KEY, useTemplates } from '@/stores/templates'

describe('template d\'interfaccia', () => {
  beforeEach(() => { setActivePinia(createPinia()); localStorage.clear() })

  it('riconosce il template dall\'indirizzo /t/<id>/', () => {
    expect(currentTemplateId('/t/dashboard/')).toBe('dashboard')
    expect(currentTemplateId('/t/mio-2/assets/app.js')).toBe('mio-2')
    expect(currentTemplateId('/')).toBeNull()
    expect(currentTemplateId('/t/')).toBeNull()
    expect(currentTemplateId('/t/A/')).toBeNull()        // maiuscole non valide
    expect(currentTemplateId('/api/t/x/')).toBeNull()
  })
  it('la scelta si ricorda nel browser e porta alla pagina del template, mantenendo la schermata', () => {
    const go = vi.fn(); history.replaceState(null, '', '/t/console/#/prefs')
    useTemplates().choose('dashboard', go)
    expect(localStorage.getItem(TEMPLATE_KEY)).toBe('dashboard')
    expect(go).toHaveBeenCalledWith('/t/dashboard/#/prefs')
  })
  it('scegliere il predefinito vale "nessuna scelta": la chiave sparisce', () => {
    localStorage.setItem(TEMPLATE_KEY, 'studio'); const go = vi.fn()
    useTemplates().choose(DEFAULT_TEMPLATE, go)
    expect(localStorage.getItem(TEMPLATE_KEY)).toBeNull()
    expect(go).toHaveBeenCalledWith(expect.stringMatching(/^\/t\/console\//))
  })
})
