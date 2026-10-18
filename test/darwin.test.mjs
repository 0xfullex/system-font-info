// The macOS backend against the fonts of the machine and the generated test
// fonts registered for this process. Skipped elsewhere.
import assert from 'node:assert/strict'
import { readFileSync } from 'node:fs'
import { after, before, describe, test } from 'node:test'
import { fileURLToPath } from 'node:url'

import { getFamilies, getFamily, pickName } from '../dist/index.js'
import { loadNative } from '../dist/native.js'

const skip = process.platform !== 'darwin' && 'macOS only'
const fixture = (name) => fileURLToPath(new URL(`./fixtures/${name}`, import.meta.url))

describe('system fonts', { skip }, () => {
  let families
  before(async () => {
    families = await getFamilies({ preset: 'full' })
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
    const instanced = faces.filter((face) => face.namedInstanceIndex !== undefined)
    assert.ok(instanced.length > 0)
    for (const face of instanced) {
      // CoreText reports coordinates cut to four decimals.
      const instance = face.opentype.fvar.instances[face.namedInstanceIndex]
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
  const testFamilies = async () => (await getFamilies({ preset: 'full' })).filter((entry) => entry.family.startsWith('SFITest'))

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
    assert.deepEqual(faces.map((face) => face.postscriptName).sort(), [
      'SFITestVariable-Bold',
      'SFITestVariable-Regular',
      'SFITestVariable-Thin'
    ])
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
    assert.deepEqual(
      variable.familyNames.filter((name) => name.language.startsWith('en')).map((name) => name.language),
      ['en', 'en-US']
    )
  })

  test('compose the full names of a named instance from its family and subfamily names, per language', async () => {
    const { faces } = (await testFamilies()).find((entry) => entry.family === 'SFITest Variable')
    const face = (postscriptName) => faces.find((candidate) => candidate.postscriptName === postscriptName)
    const pairs = (strings) => strings.map((entry) => [entry.language, entry.value])

    // Thin and Regular have English subfamily names on both platforms.
    assert.deepEqual(pairs(face('SFITestVariable-Thin').styleNames), [
      ['en', 'Thin'],
      ['en-US', 'Thin']
    ])
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
    assert.deepEqual(pairs(face('SFITestVariable-Bold').styleNames), [
      ['en', 'Bold'],
      ['zh-Hans', '粗体'],
      ['en-US', 'Bold'],
      ['zh-CN', '粗体']
    ])
    assert.deepEqual(pairs(face('SFITestVariable-Bold').fullNames), [
      ['en', 'SFITest Variable Bold'],
      ['zh-Hans', 'SFITest 变量 粗体'],
      ['en-US', 'SFITest Variable Bold'],
      ['zh-CN', 'SFITest 变量 粗体']
    ])
    // The languages that have a family name but no subfamily name (ja-JP, ru-RU, ...) get no full name.
    assert.ok(
      faces.every((candidate) => candidate.fullNames.every((entry) => ['en', 'en-US', 'zh-Hans', 'zh-CN'].includes(entry.language)))
    )
    // CoreText's own composed string is kept for reference.
    assert.equal(typeof face('SFITestVariable-Bold').platform.displayName, 'string')
  })

  test('are gone after they are unregistered', async () => {
    for (const file of registered.splice(0)) unregisterFont(file)
    assert.deepEqual(await testFamilies(), [])
  })
})

describe('getFamilies() freshness', { skip }, () => {
  const { registerFont, unregisterFont } = loadNative().__testing
  const file = fixture('SFITest-Symbol.ttf')
  const hasSymbolFamily = async () => (await getFamilies({ fields: [] })).some((entry) => entry.family === 'SFITest Symbol')

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

describe('the families option', { skip }, () => {
  const { registerFont, unregisterFont } = loadNative().__testing
  const testFonts = ['SFITest-Variable.ttf', 'SFITest-Collection.ttc', 'SFITest-Symbol.ttf', 'SFITest-Legacy.ttf'].map(fixture)
  let full

  before(async () => {
    for (const file of testFonts) registerFont(file)
    full = await getFamilies({ preset: 'full' })
  })
  after(() => {
    for (const file of testFonts) unregisterFont(file)
  })

  test('asking for every family name of a full run gives the full run, faces in the same order', async () => {
    const names = full.map((entry) => entry.family)
    assert.ok(names.includes('SFITest Variable'))
    assert.deepStrictEqual(await getFamilies({ preset: 'full', families: names }), full)
  })

  test('getFamily gives the matching entry of the full run', async () => {
    for (const name of ['Helvetica Neue', 'Skia', 'PingFang SC', 'SFITest Collection', 'SFITest Variable']) {
      assert.deepStrictEqual(
        await getFamily(name, { preset: 'full' }),
        full.find((entry) => entry.family === name),
        name
      )
    }
  })

  test('an empty list returns no families', async () => {
    assert.deepEqual(await getFamilies({ fields: [], families: [] }), [])
  })

  test('names that match nothing are ignored', async () => {
    assert.deepEqual(await getFamilies({ fields: [], families: ['No Such Family 12345', 'Skia'] }), [{ family: 'Skia' }])
    assert.equal(await getFamily('No Such Family 12345', { fields: [] }), undefined)
  })

  test('the match is case-sensitive', async () => {
    assert.deepEqual(await getFamilies({ fields: [], families: ['skia', 'SKIA', 'helvetica neue'] }), [])
    assert.equal(await getFamily('skia', { fields: [] }), undefined)
  })

  test('a localized or legacy name does not match', async () => {
    const names = new Set(full.map((entry) => entry.family))
    for (const name of ['苹方-简', 'Avenir Book']) {
      assert.ok(!names.has(name), `${name} is a family name on this machine`)
      assert.deepEqual(await getFamilies({ fields: [], families: [name] }), [], name)
    }
  })

  test('a family whose name is the start of another family name returns itself alone', async () => {
    // CoreText's own lookup is fuzzy: it can answer with another family for a name (Kefa gives Kefa III).
    const names = full.map((entry) => entry.family)
    const prefixes = names.filter((name) => names.some((other) => other.startsWith(`${name} `)))
    assert.ok(prefixes.length > 0)
    for (const name of prefixes.slice(0, 5)) {
      assert.deepEqual(await getFamilies({ fields: [], families: [name] }), [{ family: name }], name)
    }
  })

  test('a family the lookup of another requested name also yields comes once, with its own member order', async () => {
    const names = full.map((entry) => entry.family)
    const pairs = names.flatMap((name) => names.filter((other) => other.startsWith(`${name} `)).map((other) => [name, other]))
    assert.ok(pairs.length > 0)
    for (const [name, other] of pairs.slice(0, 5)) {
      const options = { fields: ['faces.postscriptName'] }
      const alone = [...(await getFamilies({ ...options, families: [name] })), ...(await getFamilies({ ...options, families: [other] }))]
      assert.deepEqual(
        await getFamilies({ ...options, families: [other, name] }),
        alone.sort((a, b) => (a.family < b.family ? -1 : 1)),
        name
      )
    }
  })

  test('the result is sorted by family whatever the order of the list, and a repeated name gives one family', async () => {
    const result = await getFamilies({ fields: [], families: ['Skia', 'Helvetica Neue', 'Skia', 'PingFang SC'] })
    assert.deepEqual(result, [{ family: 'Helvetica Neue' }, { family: 'PingFang SC' }, { family: 'Skia' }])
  })
})

/** The code points of an fc-lang orthography, read from the vendored `.orth` file and the files it includes. */
function readOrthography(name) {
  const directory = new URL('../third_party/fontconfig/fc-lang/', import.meta.url)
  const codePoints = new Set()
  const read = (file) => {
    for (const rawLine of readFileSync(new URL(file, directory), 'utf8').split('\n')) {
      const line = rawLine.replace(/#.*/, '').trim()
      if (line === '') continue
      const include = /^include\s+(\S+)$/.exec(line)
      if (include) {
        read(include[1])
        continue
      }
      const [first, last = first] = line.split('-').map((hex) => Number.parseInt(hex, 16))
      for (let codePoint = first; codePoint <= last; codePoint++) codePoints.add(codePoint)
    }
  }
  read(`${name.replaceAll('-', '_')}.orth`)
  return codePoints
}

describe('languageCoverage', { skip }, () => {
  const { registerFont, unregisterFont } = loadNative().__testing
  const testFonts = ['SFITest-Variable.ttf', 'SFITest-Collection.ttc', 'SFITest-Symbol.ttf', 'SFITest-Legacy.ttf'].map(fixture)
  const families = ['SFITest Collection', 'SFITest Legacy', 'SFITest Symbol', 'SFITest Variable']
  const orthographies = ['zh-cn', 'ja', 'en']

  before(() => {
    for (const file of testFonts) registerFont(file)
  })
  after(() => {
    for (const file of testFonts) unregisterFont(file)
  })

  test('counts the code points of the default face against zh-cn, ja and en', async () => {
    const [result, faces] = await Promise.all([
      getFamilies({ fields: [], families, languageCoverage: ['zh-cn', 'ja', 'en'] }),
      getFamilies({ fields: ['faces.weight', 'faces.style', 'faces.coverage'], families })
    ])
    assert.deepEqual(
      result.map((entry) => entry.family),
      families
    )
    const required = Object.fromEntries(orthographies.map((name) => [name, readOrthography(name)]))
    assert.deepEqual(
      [required['zh-cn'].size, required.ja.size],
      [6765, 2314],
      'the orthography files read by this test are the ones of the contract'
    )

    for (const entry of result) {
      // Every face of a test family maps the same characters; the default face is the one of weight 400 and normal style.
      const { faces: members } = faces.find((candidate) => candidate.family === entry.family)
      const face = members.find((member) => member.weight === 400 && member.style === 'normal') ?? members[0]
      const mapped = (codePoint) => face.coverage.some(([first, last]) => first <= codePoint && codePoint <= last)

      assert.deepEqual(
        entry.languageCoverage.map(({ language, orthography }) => [language, orthography]),
        [
          ['zh-CN', 'zh-cn'],
          ['ja', 'ja'],
          ['en', 'en']
        ],
        entry.family
      )
      for (const { orthography, required: count, covered } of entry.languageCoverage) {
        assert.equal(count, required[orthography].size, `${entry.family} ${orthography} required`)
        assert.equal(covered, [...required[orthography]].filter(mapped).length, `${entry.family} ${orthography} covered`)
      }
    }

    const coveredBy = (family) =>
      Object.fromEntries(result.find((entry) => entry.family === family).languageCoverage.map((c) => [c.orthography, c.covered]))
    // The text fonts map a few Latin letters and no Han or kana; the symbol font maps private-use characters only.
    assert.ok(coveredBy('SFITest Variable').en > 0)
    assert.equal(coveredBy('SFITest Variable')['zh-cn'], 0)
    assert.deepEqual(coveredBy('SFITest Symbol'), { 'zh-cn': 0, ja: 0, en: 0 })
  })

  test('is counted on a system family selected by the families option', async () => {
    const [skia] = await getFamilies({ fields: [], families: ['Skia'], languageCoverage: ['en'] })
    const [entry] = skia.languageCoverage
    assert.deepEqual([entry.language, entry.orthography, entry.required], ['en', 'en', readOrthography('en').size])
    // A Latin text font maps most of the English letters, and never more than the set holds.
    assert.ok(entry.covered > 0 && entry.covered <= entry.required, `covered ${entry.covered}`)
  })
})

describe('scope of registered fonts', { skip }, () => {
  // CTFontManagerRegisterFontsForURL with kCTFontManagerScopeSession lists as `user` (docs/platforms.md, `scope`), but
  // the only registration hook of the addon registers for the process, and a test hook for session scope is native code
  // this change does not add. The rule is covered by code review until such a hook exists.
  test('a session-registered font is user scope', { skip: 'no test hook registers a font with session scope' }, () => {})
})
