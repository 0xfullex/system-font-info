// Fontconfig's real configuration, application fonts and match results cannot be
// observed by the pure normalization tests. This suite exercises that OS seam.
import assert from 'node:assert/strict'
import { spawnSync } from 'node:child_process'
import { readFileSync, realpathSync } from 'node:fs'
import { after, before, describe, test } from 'node:test'
import { fileURLToPath } from 'node:url'

import { getFamilies, getFamily, pickName } from '../dist/index.js'
import { loadNative } from '../dist/native.js'

const skip = process.platform !== 'linux' && 'Linux only'
const fixture = (name) => fileURLToPath(new URL(`./fixtures/${name}`, import.meta.url))
const files = ['SFITest-Variable.ttf', 'SFITest-Collection.ttc', 'SFITest-Symbol.ttf', 'SFITest-Legacy.ttf'].map(fixture)
const names = ['SFITest Collection', 'SFITest Legacy', 'SFITest Symbol', 'SFITest Variable']

function readOrthography(name) {
  const directory = new URL('../third_party/fontconfig/fc-lang/', import.meta.url)
  const result = new Set()
  const read = (file) => {
    for (const raw of readFileSync(new URL(file, directory), 'utf8').split('\n')) {
      const line = raw.replace(/#.*/, '').trim()
      if (!line) continue
      const include = /^include\s+(\S+)$/.exec(line)
      if (include) {
        read(include[1])
        continue
      }
      const remove = line.startsWith('- ')
      const [first, last = first] = (remove ? line.slice(2) : line).split('-').map((hex) => Number.parseInt(hex, 16))
      for (let point = first; point <= last; point++) {
        if (remove) result.delete(point)
        else result.add(point)
      }
    }
  }
  read(`${name.replaceAll('-', '_')}.orth`)
  return result
}

describe('Linux fontconfig backend', { skip }, () => {
  let full
  const registered = []
  const hooks = () => loadNative().__testing
  before(async () => {
    for (const file of files) {
      hooks().registerFont(file)
      registered.push(file)
    }
    full = await getFamilies({ preset: 'full' })
  })
  after(() => {
    for (const file of registered.splice(0)) hooks().unregisterFont(file)
  })
  const family = (name) => full.find((entry) => entry.family === name)

  test('lists application fonts with aliases, collection indices and symbol flags', () => {
    assert.deepEqual(
      full.filter((entry) => entry.family.startsWith('SFITest')).map((entry) => entry.family),
      names
    )
    const variable = family('SFITest Variable')
    assert.equal(pickName(variable.familyNames, ['en']), variable.family)
    assert.equal(pickName(variable.familyNames, ['zh-CN']), 'SFITest 变量')
    assert.ok(family('SFITest Legacy').familyNames.some((name) => name.value === 'SFITest Legacy Book'))
    for (const face of family('SFITest Collection').faces) {
      assert.equal(face.file.index, face.postscriptName.endsWith('-Bold') ? 1 : 0)
      assert.equal(realpathSync(face.file.path), fixture('SFITest-Collection.ttc'))
    }
    assert.ok(family('SFITest Symbol').faces.every((face) => face.symbol))
    assert.ok(variable.faces.every((face) => !face.symbol))
    assert.ok(names.flatMap((name) => family(name).faces).every((face) => face.scope === 'unknown'))
  })

  test('lists fixed variable instances once, without a range face, sharing resource values', () => {
    const { faces } = family('SFITest Variable')
    assert.deepEqual(
      faces.map((face) => face.coordinates.wght).sort((a, b) => a - b),
      [100, 400, 700]
    )
    assert.deepEqual(faces.map((face) => face.postscriptName).sort(), [
      'SFITestVariable-Bold',
      'SFITestVariable-Regular',
      'SFITestVariable-Thin'
    ])
    for (const face of faces) {
      assert.equal(face.variable, true)
      assert.equal(face.file.index, 0)
      assert.equal(face.axes, faces[0].axes)
      assert.equal(face.opentype, faces[0].opentype)
      const first = face.opentype.fvar.instances.findIndex((instance) =>
        Object.entries(face.coordinates).every(([tag, value]) => instance.coordinates[tag] === value)
      )
      assert.equal(face.namedInstanceIndex, first)
      if (face.platform.index >> 16) assert.equal(face.namedInstanceIndex, (face.platform.index >> 16) - 1)
    }
    // The default pattern has packed instance zero, yet its coordinates name an fvar instance.
    const regular = faces.find((face) => face.coordinates.wght === 400)
    assert.equal(regular.platform.index >> 16, 0)
    assert.equal(typeof regular.namedInstanceIndex, 'number')
    assert.equal(regular.fullName, 'SFITest Variable Regular')
    const bold = faces.find((face) => face.coordinates.wght === 700)
    assert.ok(bold.styleNames.some((name) => name.language === 'zh-CN' && name.value === '粗体'))
    assert.ok(bold.fullNames.some((name) => name.language === 'zh-CN' && name.value === 'SFITest 变量 粗体'))
  })

  test('maps classification and raw values using fontconfig conventions', () => {
    const faces = full.flatMap((entry) => entry.faces)
    assert.ok(faces.length > 0)
    for (const face of faces) {
      const raw = face.platform
      assert.equal(face.width, raw.width)
      assert.equal(face.file.index, raw.index & 0xffff)
      assert.equal(face.monospace, [90, 100, 110].includes(raw.spacing))
      assert.equal(face.style, raw.slant === 100 ? 'italic' : raw.slant === 110 ? 'oblique' : 'normal')
      assert.equal(face.enabled, true)
      assert.deepEqual(face.simulations, { bold: false, oblique: false })
      if (face.format === 'type1' || face.format === 'bitmap') assert.equal(face.opentype, undefined)
      if (face.file.path.startsWith('/usr/share/fonts/') || face.file.path.startsWith('/usr/local/share/fonts/')) {
        assert.equal(face.scope, 'system')
      }
    }
    const regular = family('SFITest Collection').faces.find((face) => face.postscriptName.endsWith('-Regular'))
    const bold = family('SFITest Collection').faces.find((face) => face.postscriptName.endsWith('-Bold'))
    assert.equal(regular.weight, 400)
    assert.equal(bold.weight, 700)
  })

  test('asking for every family merges separate fontconfig queries into the full listing', async () => {
    assert.deepEqual(await getFamilies({ preset: 'full', families: full.map((entry) => entry.family) }), full)
    for (const name of names) assert.deepEqual(await getFamily(name, { preset: 'full' }), family(name))
    assert.deepEqual(await getFamilies({ fields: [], families: [] }), [])
    assert.deepEqual(await getFamilies({ fields: [], families: ['SFITest Variable', 'SFITest Variable', 'No Such Family 12345'] }), [
      { family: 'SFITest Variable' }
    ])
    assert.deepEqual(await getFamilies({ fields: [], families: ['sfitest variable', 'SFITest Legacy Book', 'SFITest 变量'] }), [])
  })

  test('LANG and CHARSET object selection preserves member and localized-name order', async () => {
    const fields = ['faces.postscriptName', 'faces.fullNames', 'faces.styleNames', 'faces.familyNames']
    const base = await getFamilies({ fields })
    for (const additional of [['supportedLanguages'], ['faces.coverage'], ['supportedLanguages', 'faces.coverage']]) {
      const current = await getFamilies({ fields: [...fields, ...additional] })
      assert.deepEqual(
        current.map((entry) => ({
          family: entry.family,
          faces: entry.faces.map((face) =>
            Object.fromEntries(Object.entries(face).filter(([key]) => !['coverage', 'supportedLanguages'].includes(key)))
          )
        })),
        base
      )
    }
    assert.deepEqual(
      await getFamilies({ preset: 'names' }),
      full.map(({ family, familyNames }) => ({ family, familyNames }))
    )
  })

  test('matches a default face inside each family and counts the built-in orthographies', async () => {
    const orthographies = ['zh-cn', 'ja', 'en']
    const required = Object.fromEntries(orthographies.map((name) => [name, readOrthography(name)]))
    assert.deepEqual([required['zh-cn'].size, required.ja.size], [6765, 2314])
    const result = await getFamilies({ fields: [], families: names, languageCoverage: orthographies })
    const covered = await getFamilies({ fields: ['faces.coverage'], families: names })
    for (const entry of result) {
      // All faces of each fixture have the same coverage, making the expected set independent of the matcher.
      const ranges = covered.find((other) => other.family === entry.family).faces[0].coverage
      for (const coverage of entry.languageCoverage) {
        const points = [...required[coverage.orthography]]
        assert.equal(coverage.required, points.length)
        assert.equal(coverage.covered, points.filter((point) => ranges.some(([first, last]) => first <= point && point <= last)).length)
      }
    }
    assert.ok(result.find((entry) => entry.family === 'SFITest Variable').languageCoverage[2].covered > 0)
    // Fontconfig aliases the symbol cmap's U+F041 / U+F042 mappings to Latin A / B too.
    assert.deepEqual(
      result.find((entry) => entry.family === 'SFITest Symbol').languageCoverage.map((entry) => entry.covered),
      [0, 0, 2]
    )
    const options = { fields: [], languageCoverage: ['en'] }
    const plain = await getFamilies(options)
    const withLanguages = await getFamilies({ ...options, fields: ['supportedLanguages'] })
    assert.deepEqual(
      withLanguages.map(({ family, languageCoverage }) => ({ family, languageCoverage })),
      plain
    )
    assert.deepEqual(await getFamilies({ ...options, families: plain.map((entry) => entry.family) }), plain)
  })

  test('test registration is isolated from children even when their environment is inherited', () => {
    const script = `import { getFamilies } from ${JSON.stringify(new URL('../dist/index.js', import.meta.url).href)};
      const result = await getFamilies({ fields: [], families: ['SFITest Variable'] });
      process.stdout.write(JSON.stringify(result));`
    const child = spawnSync(process.execPath, ['--input-type=module', '-e', script], { encoding: 'utf8' })
    assert.equal(child.status, 0, child.stderr)
    assert.deepEqual(JSON.parse(child.stdout), [])
  })

  test('rejects an unreadable or invalid test font through fontconfig', () => {
    assert.throws(() => hooks().registerFont(fixture('not-a-font.ttf')), /FcConfigAppFontAddFile/)
  })

  test('setFontEnabled is not supported: fontconfig has no disabled state', () => {
    assert.throws(() => hooks().setFontEnabled(files[0], false), /setFontEnabled is not supported/)
  })

  test('the next fresh configuration sees unregister and register', async () => {
    const file = files[2]
    hooks().unregisterFont(file)
    registered.splice(registered.indexOf(file), 1)
    assert.equal(await getFamily('SFITest Symbol', { fields: [] }), undefined)
    hooks().registerFont(file)
    registered.push(file)
    assert.deepEqual(await getFamily('SFITest Symbol', { fields: [] }), { family: 'SFITest Symbol' })
  })
})
