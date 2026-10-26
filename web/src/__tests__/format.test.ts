import { bytes, dateTime, duration, eta, ed2kLink, extractLinks, looksLikeLink, number, percent, speed } from '@/lib/format'
import { setLang } from '@/i18n'

describe('format (italiano)', () => {
  beforeAll(() => setLang('it'))
  it('dimensioni in base 1024 come eMule, con la virgola', () => {
    expect(bytes(0)).toBe('0 B'); expect(bytes(1023)).toBe('1023 B'); expect(bytes(1024)).toBe('1,00 KB'); expect(bytes(300, 2, true)).toBe('0,29 KB'); expect(bytes(0, 2, true)).toBe('0,00 KB'); expect(bytes(5 * 1048576, 2, true)).toBe('5,00 MB'); expect(bytes(3 * 1073741824, 2, true)).toBe('3,00 GB')
    expect(bytes(16222515)).toBe('15,47 MB'); expect(bytes(6342123520)).toBe('5,91 GB'); expect(bytes(-1)).toBe('—')
  })
  it('velocita: trattino se ferma', () => { expect(speed(0)).toBe('—'); expect(speed(2048)).toBe('2,0 KB/s') })
  it('percentuale non arrotonda per eccesso (99,96 non diventa 100)', () => { expect(percent(99.96)).toBe('99,9%'); expect(percent(100)).toBe('100%'); expect(percent(100.4)).toBe('100%'); expect(percent(0)).toBe('0%'); expect(percent(77.3)).toBe('77,3%') })
  it('durate', () => { expect(duration(45)).toBe('45 s'); expect(duration(125)).toBe('2 min'); expect(duration(7500)).toBe('2 h 5 min'); expect(duration(200000)).toBe('2 g 7 h'); expect(duration(NaN)).toBe('—') })
  it('tempo rimasto richiede velocita e qualcosa da scaricare', () => { expect(eta(100, 0, 0)).toBe('—'); expect(eta(100, 100, 10)).toBe('—'); expect(eta(6000, 0, 10)).toBe('10 min') })
  it('numeri con il separatore delle migliaia', () => { expect(number(1234567)).toBe('1.234.567') })
  it('link ed2k: creazione, estrazione, riconoscimento', () => {
    const l = ed2kLink('a b.pdf', 10, 'A'.repeat(32)); expect(l).toBe('ed2k://|file|a%20b.pdf|10|' + 'A'.repeat(32) + '|/')
    expect(extractLinks(`x ${l}\n${l} y`)).toHaveLength(2); expect(extractLinks('ed2k://|file|x|1|zz|/')).toEqual([])
    expect(looksLikeLink('  ed2k://|file|')).toBe(true); expect(looksLikeLink('ubuntu')).toBe(false)
  })
})

describe('format (English)', () => {
  beforeAll(() => setLang('en'))
  afterAll(() => setLang('it'))
  it('point as decimal separator, comma for thousands, translated units of time', () => {
    expect(bytes(16222515)).toBe('15.47 MB'); expect(percent(77.3)).toBe('77.3%'); expect(number(1234567)).toBe('1,234,567')
    expect(duration(200000)).toBe('2 d 7 h'); expect(dateTime(0)).toMatch(/1970/)
  })
})
