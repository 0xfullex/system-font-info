// The macOS backend against the fonts of the machine and the generated test
// fonts registered for this process. Skipped elsewhere.
import assert from 'node:assert/strict'
import { after, before, describe, test } from 'node:test'
import { fileURLToPath } from 'node:url'

import { getFonts, pickName } from '../dist/index.js'
import { loadNative } from '../dist/native.js'

const skip = process.platform !== 'darwin' && 'macOS only'
const fixture = (name) => fileURLToPath(new URL(`./fixtures/${name}`, import.meta.url))

describe('system fonts', { skip }, () => {
  let families
  before(async () => {
    families = await getFonts()
  })
  const family = (name) => families.find((entry) => entry.family === name)

  test('Helvetica Neue has at least four faces', () => {
    assert.ok(family('Helvetica Neue').faces.length >= 4)
  })

  test('picks the Simplified Chinese name of PingFang SC', () => {
    assert.equal(pickName(family('PingFang SC').familyNames, ['zh-Hans']), '苹方-简')
  })

  test('script faces without a slanted design are normal, italic faces are italic', () => {
    const style = (familyName, postscriptName) => family(familyName).faces.find((face) => face.postscriptName === postscriptName).style
    assert.equal(style('Zapfino', 'Zapfino'), 'normal')
    assert.equal(style('Apple Chancery', 'Apple-Chancery'), 'normal')
    assert.equal(style('Helvetica Neue', 'HelveticaNeue-Italic'), 'italic')
    assert.equal(style('Helvetica Neue', 'HelveticaNeue'), 'normal')
    // CoreText's slant trait is not what decides: it stays raw in platform.slant.
    assert.notEqual(family('Zapfino').faces[0].platform.slant, 0)
    assert.ok(families.every((entry) => entry.faces.every((face) => face.style !== 'oblique')))
  })

  test('Avenir lists its typographic family name, not the legacy "Avenir Book" next to it', () => {
    const names = family('Avenir').familyNames
    assert.ok(names.some((name) => name.value === 'Avenir'))
    assert.ok(!names.some((name) => name.value === 'Avenir Book' && name.language.startsWith('en')))
  })

  test('Skia is variable, with coordinates and named instances', () => {
    const { faces } = family('Skia')
    assert.ok(faces.every((face) => face.variable))
    assert.ok(faces.every((face) => ['wght', 'wdth'].every((tag) => typeof face.coordinates[tag] === 'number')))
    const instanced = faces.filter((face) => face.namedInstance !== undefined)
    assert.ok(instanced.length > 0)
    for (const face of instanced) {
      // CoreText reports coordinates cut to four decimals.
      const instance = face.opentype.fvar.instances[face.namedInstance]
      for (const [tag, value] of Object.entries(instance.coordinates)) {
        assert.ok(Math.abs(face.coordinates[tag] - value) < 5e-4, `${face.postscriptName} ${tag}`)
      }
    }
  })
})

describe('registered test fonts', { skip }, () => {
  const { registerFont, unregisterFont } = loadNative().__testing
  const files = ['SFITest-Variable.ttf', 'SFITest-Collection.ttc', 'SFITest-Symbol.ttf', 'SFITest-Legacy.ttf'].map(fixture)
  const registered = []
  const testFamilies = async () => (await getFonts()).filter((entry) => entry.family.startsWith('SFITest'))

  before(() => {
    for (const file of files) {
      registerFont(file)
      registered.push(file)
    }
  })
  after(() => {
    for (const file of registered.splice(0)) unregisterFont(file)
  })

  test('appear with process scope, localized names, collection indices and the symbol flag', async () => {
    const found = Object.fromEntries((await testFamilies()).map((entry) => [entry.family, entry]))
    assert.deepEqual(Object.keys(found).sort(), ['SFITest Collection', 'SFITest Legacy', 'SFITest Symbol', 'SFITest Variable'])
    const faces = Object.values(found).flatMap((entry) => entry.faces)
    assert.ok(faces.length > 0)
    assert.ok(faces.every((face) => face.scope === 'process'))

    const names = found['SFITest Variable'].familyNames
    assert.equal(pickName(names, ['zh-CN']), 'SFITest 变量')
    assert.equal(pickName(names, ['ja-JP']), 'SFITest バリアブル')
    assert.equal(pickName(names, ['ru-RU']), 'SFITest Переменный')
    assert.equal(pickName(names, ['ar-SA']), 'SFITest متغير')

    const byName = Object.fromEntries(found['SFITest Collection'].faces.map((face) => [face.postscriptName, face]))
    assert.equal(byName['SFITestCollection-Regular'].file.index, 0)
    assert.equal(byName['SFITestCollection-Bold'].file.index, 1)
    assert.equal(byName['SFITestCollection-Regular'].file.path, fixture('SFITest-Collection.ttc'))

    assert.deepEqual(
      found['SFITest Symbol'].faces.map((face) => face.symbol),
      [true]
    )
    assert.ok(found['SFITest Variable'].faces.every((face) => !face.symbol))
  })

  test('the named instances of the variable font share one font resource', async () => {
    const { faces } = (await testFamilies()).find((entry) => entry.family === 'SFITest Variable')
    assert.deepEqual(faces.map((face) => face.postscriptName).sort(), ['SFITestVariable-Bold', 'SFITestVariable-Regular', 'SFITestVariable-Thin'])
    assert.ok(faces.every((face) => face.opentype === faces[0].opentype && face.axes === faces[0].axes))
    assert.deepEqual(
      faces.map((face) => face.coordinates.wght).sort((a, b) => a - b),
      [100, 400, 700]
    )
  })

  test('add legacy names only for languages the typographic names lack', async () => {
    const { faces } = (await testFamilies()).find((entry) => entry.family === 'SFITest Legacy')
    assert.equal(faces.length, 1)
    const [face] = faces
    // Windows has ID 16 and 17 in en-US; the Macintosh "en" ID 1 and 2 ("SFITest Legacy Book", "Regular")
    // are the same language and stay out. Turkish and Icelandic are not covered, so their legacy names join.
    assert.deepEqual(face.familyNames, [
      { language: 'en-US', value: 'SFITest Legacy' },
      { language: 'is', value: 'Letur Þþ Ðð' },
      { language: 'tr', value: 'Yazı Tipi Ğğ Şş İ' }
    ])
    assert.deepEqual(face.styleNames, [
      { language: 'en-US', value: 'Book' },
      { language: 'is', value: 'Venjulegt' },
      { language: 'tr', value: 'Düz' }
    ])
  })

  test('keep the legacy names of a language the typographic names lack, Chinese included', async () => {
    const variable = (await testFamilies()).find((entry) => entry.family === 'SFITest Variable')
    const languages = variable.familyNames.map((name) => name.language)
    // ID 16 exists on both platforms in English; every other language comes from ID 1.
    for (const language of ['zh-CN', 'zh-Hans', 'ja-JP', 'ru-RU', 'ar-SA', 'gsw']) assert.ok(languages.includes(language), language)
    assert.deepEqual(variable.familyNames.filter((name) => name.language.startsWith('en')).map((name) => name.language), ['en', 'en-US'])
  })

  test('compose the full names of a named instance from its family and subfamily names, per language', async () => {
    const { faces } = (await testFamilies()).find((entry) => entry.family === 'SFITest Variable')
    const face = (postscriptName) => faces.find((candidate) => candidate.postscriptName === postscriptName)
    const pairs = (strings) => strings.map((entry) => [entry.language, entry.value])

    // Thin and Regular have English subfamily names on both platforms.
    assert.deepEqual(pairs(face('SFITestVariable-Thin').styleNames), [['en', 'Thin'], ['en-US', 'Thin']])
    assert.deepEqual(pairs(face('SFITestVariable-Thin').fullNames), [
      ['en', 'SFITest Variable Thin'],
      ['en-US', 'SFITest Variable Thin']
    ])
    // "Regular" is not dropped from a name composed of ID 16 and the subfamily name.
    assert.deepEqual(pairs(face('SFITestVariable-Regular').fullNames), [
      ['en', 'SFITest Variable Regular'],
      ['en-US', 'SFITest Variable Regular']
    ])
    // Bold also has Chinese: Macintosh zh-Hans and Windows zh-CN, each paired with the family name of its own script.
    assert.deepEqual(pairs(face('SFITestVariable-Bold').styleNames), [['en', 'Bold'], ['zh-Hans', '粗体'], ['en-US', 'Bold'], ['zh-CN', '粗体']])
    assert.deepEqual(pairs(face('SFITestVariable-Bold').fullNames), [
      ['en', 'SFITest Variable Bold'],
      ['zh-Hans', 'SFITest 变量 粗体'],
      ['en-US', 'SFITest Variable Bold'],
      ['zh-CN', 'SFITest 变量 粗体']
    ])
    // The languages that have a family name but no subfamily name (ja-JP, ru-RU, ...) get no full name.
    assert.ok(faces.every((candidate) => candidate.fullNames.every((entry) => ['en', 'en-US', 'zh-Hans', 'zh-CN'].includes(entry.language))))
    // CoreText's own composed string is kept for reference.
    assert.equal(typeof face('SFITestVariable-Bold').platform.displayName, 'string')
  })

  test('are gone after they are unregistered', async () => {
    for (const file of registered.splice(0)) unregisterFont(file)
    assert.deepEqual(await testFamilies(), [])
  })
})

describe('getFonts() freshness', { skip }, () => {
  const { registerFont, unregisterFont } = loadNative().__testing
  const file = fixture('SFITest-Symbol.ttf')
  const hasSymbolFamily = async () => (await getFonts()).some((entry) => entry.family === 'SFITest Symbol')

  test('each call reflects the fonts registered at that moment', async () => {
    assert.equal(await hasSymbolFamily(), false)
    registerFont(file)
    try {
      assert.equal(await hasSymbolFamily(), true)
    } finally {
      unregisterFont(file)
    }
    assert.equal(await hasSymbolFamily(), false)
  })
})
