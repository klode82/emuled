import { ApiError } from '@/lib/api'
import { errText } from '@/stores/toasts'
import { setLang } from '@/i18n'

describe('errText', () => {
  it('con un codice conosciuto traduce nella lingua scelta', () => {
    setLang('it'); expect(errText(new ApiError(404, 'download non trovato', 'download_not_found'))).toBe('Download non trovato')
    setLang('en'); expect(errText(new ApiError(404, 'download non trovato', 'download_not_found'))).toBe('Download not found')
    setLang('it')
  })
  it('senza traduzione per quel codice (o senza codice) resta il testo del demone', () => {
    expect(errText(new ApiError(422, 'la cartella X non esiste', 'bad_directories'))).toBe('la cartella X non esiste')
    expect(errText(new ApiError(500, 'boh'))).toBe('boh')
    expect(errText('testo')).toBe('testo')
  })
})
