// DirectWrite's WSS members, resource/source joins, and process-local test collection. Skipped on other platforms.
import assert from 'node:assert/strict'
import { readFileSync } from 'node:fs'
import { after, before, describe, test } from 'node:test'
import { fileURLToPath } from 'node:url'

import { closureOf, presetFields } from '../dist/field-closure.js'
import { getFamilies, getFamily } from '../dist/index.js'
import { loadNative } from '../dist/native.js'

const skip = process.platform !== 'win32' && 'Windows only'
const fixture = (name) => fileURLToPath(new URL(`./fixtures/${name}`, import.meta.url))
const files = ['SFITest-Variable.ttf', 'SFITest-Collection.ttc', 'SFITest-Symbol.ttf', 'SFITest-Legacy.ttf'].map(fixture)
const fixtureFamilies = ['SFITest Collection', 'SFITest Legacy', 'SFITest Symbol', 'SFITest Variable']
const resourceKey = (face) => JSON.stringify([face.file?.path, face.file?.index])
const identity = (face) => JSON.stringify([face.file?.path, face.file?.index, face.coordinates, face.simulations])

describe('DirectWrite system families', { skip }, () => {
  let full
  before(async () => {
    full = await getFamilies({ preset: 'full' })
    assert.ok(full.length > 0)
  })

  test('preserves WSS classification, simulations, enabled state, and outline formats', () => {
    const widths = [undefined, 50, 62.5, 75, 87.5, 100, 112.5, 125, 150, 200]
    const styles = ['normal', 'oblique', 'italic']
    let simulated = 0
    for (const family of full) {
      for (const face of family.faces) {
        assert.equal(face.weight, face.platform.weight)
        assert.equal(face.width, widths[face.platform.width])
        assert.equal(face.style, styles[face.platform.slant])
        assert.equal(face.enabled, true)
        if (face.simulations.bold || face.simulations.oblique) simulated++
        const tags = face.opentype?.tables ?? []
        if (tags.includes('CFF2')) assert.equal(face.format, 'cff2')
        else if (tags.includes('CFF ')) assert.equal(face.format, 'cff')
        else if (tags.includes('glyf')) assert.equal(face.format, 'truetype')
      }
    }
    assert.ok(simulated > 0, 'the WSS collection exposes synthesized members')
  })

  test('simulated members use the same source as their physical resource', () => {
    const resources = new Map()
    let knownSources = 0
    for (const face of full.flatMap((family) => family.faces)) {
      if (!face.file) continue
      const source = [face.platform.sourceType, face.platform.sourceName, face.scope]
      const key = resourceKey(face)
      if (resources.has(key)) assert.deepEqual(source, resources.get(key), key)
      else resources.set(key, source)
      if (face.platform.sourceType === 1) {
        assert.equal(face.scope, 'system')
        knownSources++
      } else if (face.platform.sourceType === 2) {
        assert.equal(face.scope, 'user')
        knownSources++
      } else assert.equal(face.scope, 'unknown')
    }
    assert.ok(knownSources > 0, 'the whole-system source join finds local registrations')
  })

  test('identical repeated members are merged, with distinct simulations and coordinates retained', () => {
    for (const family of full) {
      const identities = family.faces.map(identity)
      assert.equal(new Set(identities).size, identities.length, family.family)
    }
    // These families contained repeated OS members on the captured host; keep checking them when installed.
    for (const name of ['Gen', 'LXGW WenKai GB']) {
      const family = full.find((entry) => entry.family === name)
      if (!family) continue
      const simulations = new Map()
      for (const face of family.faces) {
        const key = resourceKey(face)
        const values = simulations.get(key) ?? new Set()
        values.add(JSON.stringify(face.simulations))
        simulations.set(key, values)
      }
      assert.ok(
        [...simulations.values()].some((values) => values.size > 1),
        name
      )
    }
  })

  test('asking for every canonical family name gives the full run and the same member order', async () => {
    assert.deepEqual(await getFamilies({ preset: 'full', families: full.map((entry) => entry.family) }), full)
  })

  test('family matching rejects case and localized aliases while preserving exact candidates', async () => {
    const canonical = new Set(full.map((entry) => entry.family))
    const candidate = full.find((entry) => entry.family.toLowerCase() !== entry.family && !canonical.has(entry.family.toLowerCase()))
    assert.ok(candidate)
    assert.equal(await getFamily(candidate.family.toLowerCase(), { fields: [] }), undefined)
    assert.deepEqual(await getFamily(candidate.family, { fields: [] }), { family: candidate.family })
    const aliases = [...new Set(full.flatMap((entry) => entry.familyNames.map((name) => name.value)))].filter(
      (name) => !canonical.has(name)
    )
    assert.deepEqual(await getFamilies({ fields: [], families: aliases }), [])
    assert.deepEqual(await getFamilies({ fields: [], families: [] }), [])
    assert.deepEqual(await getFamilies({ fields: [], families: ['No Such Windows Family 12345'] }), [])
  })
})

describe('DirectWrite process-local test fonts', { skip }, () => {
  const registered = []
  let full
  const readFixtures = (options) => getFamilies({ ...options, families: fixtureFamilies })

  before(async () => {
    const { registerFont } = loadNative().__testing
    for (const path of files) {
      registerFont(path)
      registered.push(path)
    }
    full = await readFixtures({ preset: 'full' })
  })
  after(() => {
    for (const path of registered.splice(0)) loadNative().__testing.unregisterFont(path)
  })

  test('are visible with process scope, matcher family names, collection indices, and the symbol flag', () => {
    assert.deepEqual(
      full.map((entry) => entry.family),
      fixtureFamilies
    )
    const byFamily = Object.fromEntries(full.map((entry) => [entry.family, entry]))
    for (const family of full) {
      assert.ok(family.familyNames.some((name) => name.value === family.family))
      assert.ok(family.faces.length > 0)
      assert.ok(
        family.faces.every((face) => face.scope === 'process'),
        family.family
      )
      assert.ok(family.faces.every((face) => face.familyNames.some((name) => name.value === family.family)))
    }
    for (const face of byFamily['SFITest Collection'].faces) {
      assert.equal(face.file.path.toLowerCase(), fixture('SFITest-Collection.ttc').toLowerCase())
      const boldResource = face.postscriptName === 'SFITestCollection-Bold'
      assert.equal(face.file.index, boldResource ? 1 : 0)
    }
    assert.ok(byFamily['SFITest Symbol'].faces.every((face) => face.symbol))
    assert.ok(byFamily['SFITest Variable'].faces.every((face) => !face.symbol))
  })

  test('retains variable instances, fvar axes, injected raw axes, and shared resource objects', () => {
    const { faces } = full.find((entry) => entry.family === 'SFITest Variable')
    const originals = faces.filter((face) => !face.simulations.bold && !face.simulations.oblique)
    assert.deepEqual(
      originals.map((face) => face.coordinates.wght),
      [100, 400, 700]
    )
    for (const face of faces) {
      assert.equal(face.variable, true)
      assert.deepEqual(
        face.axes.map((axis) => axis.tag),
        ['wght']
      )
      assert.deepEqual(Object.keys(face.coordinates), ['wght'])
      assert.equal(face.opentype, faces[0].opentype)
      assert.equal(face.axes, faces[0].axes)
      assert.ok(face.platform.axisValues.some((axis) => axis.tag === 'wdth'))
      assert.ok(face.platform.resourceAxes.some((axis) => axis.tag === 'wdth'))
      const instance = face.opentype.fvar.instances[face.namedInstanceIndex]
      assert.ok(instance)
      assert.equal(face.coordinates.wght, instance.coordinates.wght)
    }
  })

  test('preserves multilingual font-table records independently of WSS matcher names', () => {
    const { faces } = full.find((entry) => entry.family === 'SFITest Variable')
    const names = faces[0].opentype.names
    assert.ok(names.some((name) => name.language === 'zh-CN' && name.value === 'SFITest 变量'))
    assert.ok(names.some((name) => name.language === 'ar-SA' && name.value === 'SFITest متغير'))
    const legacy = full.find((entry) => entry.family === 'SFITest Legacy').faces[0].opentype.names
    assert.ok(legacy.some((name) => name.value === 'Letur Þþ Ðð'))
    assert.ok(legacy.some((name) => name.value === 'Yazı Tipi Ğğ Şş İ'))
  })

  test('static styles and family variable classification open no resources', async () => {
    const { getFamiliesWithWork } = loadNative().__testing
    const styles = await getFamiliesWithWork({
      fields: [...closureOf(presetFields('styles'), 'win32')],
      families: ['SFITest Collection', 'SFITest Symbol'],
      orthographies: []
    })
    assert.deepEqual(styles.work, { fontsOpened: 0, tableEntries: 0, characterSets: 0 })
    const variable = await getFamiliesWithWork({ fields: ['faces.variable'], families: fixtureFamilies, orthographies: [] })
    assert.deepEqual(variable.work, { fontsOpened: 0, tableEntries: 0, characterSets: 0 })
    assert.ok(variable.families.find((entry) => entry.family === 'SFITest Variable').faces.every((face) => face.variable))
  })

  test('languageCoverage counts the default face with E work only', async () => {
    const tags = ['en', 'zh-cn', 'ja']
    const [counts, ranges] = await Promise.all([
      readFixtures({ fields: [], languageCoverage: tags }),
      readFixtures({ fields: ['faces.weight', 'faces.style', 'faces.coverage'] })
    ])
    const required = Object.fromEntries(tags.map((tag) => [tag, readOrthography(tag)]))
    assert.deepEqual([required['zh-cn'].size, required.ja.size], [6765, 2314])
    for (const family of counts) {
      const { faces } = ranges.find((entry) => entry.family === family.family)
      const face = faces.find((entry) => entry.weight === 400 && entry.style === 'normal')
      assert.ok(face)
      for (const coverage of family.languageCoverage) {
        const mapped = (point) => face.coverage.some(([first, last]) => first <= point && point <= last)
        assert.equal(coverage.required, required[coverage.orthography].size)
        assert.equal(coverage.covered, [...required[coverage.orthography]].filter(mapped).length)
      }
    }
    const { work } = await loadNative().__testing.getFamiliesWithWork({ fields: [], families: fixtureFamilies, orthographies: tags })
    assert.deepEqual(work, { fontsOpened: 0, tableEntries: 0, characterSets: fixtureFamilies.length })
  })
})

test('process font registration is visible on the next call and unregistration removes it', { skip }, async () => {
  const { registerFont, unregisterFont } = loadNative().__testing
  const path = fixture('SFITest-Symbol.ttf')
  assert.equal(await getFamily('SFITest Symbol', { fields: [] }), undefined)
  registerFont(path)
  try {
    const family = await getFamily('SFITest Symbol', { fields: ['faces.scope', 'faces.file'] })
    assert.ok(family.faces.every((face) => face.scope === 'process'))
    assert.ok(family.faces.every((face) => face.file.path.toLowerCase() === path.toLowerCase()))
  } finally {
    unregisterFont(path)
  }
  assert.equal(await getFamily('SFITest Symbol', { fields: [] }), undefined)
})

/** Read the vendored orthography independently of the native counter. */
function readOrthography(name) {
  const directory = new URL('../third_party/fontconfig/fc-lang/', import.meta.url)
  const points = new Set()
  const read = (file) => {
    for (const raw of readFileSync(new URL(file, directory), 'utf8').split('\n')) {
      const line = raw.replace(/#.*/, '').trim()
      if (!line) continue
      const include = /^include\s+(\S+)$/.exec(line)
      if (include) {
        read(include[1])
        continue
      }
      const [first, last = first] = line.split('-').map((value) => Number.parseInt(value, 16))
      for (let point = first; point <= last; point++) points.add(point)
    }
  }
  read(`${name.replaceAll('-', '_')}.orth`)
  return points
}
