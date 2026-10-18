// The projection invariant of docs/api.md ("Invariants"): every result equals the full result projected onto the
// selected fields. The full result of a sample of families is fetched once; every field of the catalogue on its own,
// every preset, `fields: []` and some mixed selections are then fetched and compared with a projection of the full
// result written here from the rules of api.md ("Always included", "Field names", "OpenType entries"). The projection
// is deliberately not ts/project.ts: that code and this reference must not share a mistake.
//
// The sample is the generated test fonts (shared and static, collection, symbol, legacy-named) and three system
// families (a static family, a variable family, a CJK family), so every kind of face is projected.
import assert from 'node:assert/strict'
import { after, before, describe, test } from 'node:test'
import { fileURLToPath } from 'node:url'

import { FIELD_CATALOGUE } from '../dist/field-catalogue.js'
import { getFamilies } from '../dist/index.js'
import { loadNative } from '../dist/native.js'

const fixture = (name) => fileURLToPath(new URL(`./fixtures/${name}`, import.meta.url))
const TEST_FONTS = ['SFITest-Variable.ttf', 'SFITest-Collection.ttc', 'SFITest-Symbol.ttf', 'SFITest-Legacy.ttf'].map(fixture)
const TEST_FAMILIES = ['SFITest Collection', 'SFITest Legacy', 'SFITest Symbol', 'SFITest Variable']
const SYSTEM_FAMILIES = process.platform === 'darwin' ? ['Helvetica Neue', 'Skia', 'PingFang SC'] : []
const FAMILIES = [...TEST_FAMILIES, ...SYSTEM_FAMILIES]
const LANGUAGE_COVERAGE = ['zh-CN', 'ja']

// ---- the rules of docs/api.md, as data ------------------------------------------------------------------------

const FAMILY_FIELDS = ['familyNames', 'supportedLanguages', 'variable', 'enabled']
/** "Always included": the identity of a face, whenever a `faces.*` field is selected. */
const IDENTITY = ['file', 'coordinates', 'simulations']
/** "OpenType entries": each entry and the tags of the tables it is made of (`errors` is limited to these). */
const OPENTYPE_TABLES = {
  tables: [],
  names: ['name'],
  head: ['head'],
  hhea: ['hhea'],
  vhea: ['vhea'],
  os2: ['OS/2'],
  post: ['post'],
  maxp: ['maxp'],
  cmap: ['cmap'],
  fvar: ['fvar'],
  meta: ['meta'],
  layout: ['GSUB', 'GPOS']
}
/** "Presets": the contents, each preset containing the one before it. */
const PLATFORM_FIELDS = FIELD_CATALOGUE.map((spec) => spec.name).filter((name) => name.startsWith('faces.platform.'))
const NAMES = ['familyNames']
const STYLES = [
  ...NAMES,
  'supportedLanguages',
  'enabled',
  ...['postscriptName', 'fullName', 'fullNames', 'styleName', 'styleNames', 'familyNames'].map((key) => `faces.${key}`),
  ...['weight', 'width', 'style', 'monospace', 'color', 'symbol'].map((key) => `faces.${key}`),
  ...['enabled', 'simulations', 'supportedLanguages', 'scope', 'file', 'coordinates'].map((key) => `faces.${key}`)
]
const DETAILS = [
  ...STYLES,
  'variable',
  ...['variable', 'format', 'axes', 'namedInstanceIndex'].map((key) => `faces.${key}`),
  ...PLATFORM_FIELDS
]
const FULL = [...DETAILS, ...Object.keys(OPENTYPE_TABLES).map((entry) => `faces.opentype.${entry}`)]
const PRESETS = { names: NAMES, styles: STYLES, details: DETAILS, full: FULL }

const OPENTYPE_PREFIX = 'faces.opentype.'
const PLATFORM_PREFIX = 'faces.platform.'

/** The fields an options object selects: its preset's fields and its `fields`. */
const selectedFields = ({ fields = [], preset }) => new Set([...(preset ? PRESETS[preset] : []), ...fields])

/**
 * The reference projection: `source` (the full result of the same families) projected onto `selected`.
 * `languageCoverage` is kept when the call asked for it (the entries then come from `source`, which asked the same).
 */
function project(source, selected, { languageCoverage = false } = {}) {
  const names = [...selected]
  const faceKeys = names
    .filter((name) => name.startsWith('faces.') && !name.startsWith(OPENTYPE_PREFIX) && !name.startsWith(PLATFORM_PREFIX))
    .map((name) => name.slice('faces.'.length))
  const entries = names.filter((name) => name.startsWith(OPENTYPE_PREFIX)).map((name) => name.slice(OPENTYPE_PREFIX.length))
  const platformKeys = names.filter((name) => name.startsWith(PLATFORM_PREFIX)).map((name) => name.slice(PLATFORM_PREFIX.length))
  const hasFaceField = names.some((name) => name.startsWith('faces.'))
  const tables = entries.flatMap((entry) => OPENTYPE_TABLES[entry])

  const projectFace = (face) => {
    const result = {}
    for (const key of [...IDENTITY, ...faceKeys]) if (key in face) result[key] = face[key]
    if (entries.length > 0 && face.opentype !== undefined) {
      const opentype = {}
      for (const entry of entries) if (entry in face.opentype) opentype[entry] = face.opentype[entry]
      const errors = (face.opentype.errors ?? []).filter((tag) => tables.includes(tag))
      if (errors.length > 0) opentype.errors = errors
      result.opentype = opentype
    }
    // A platform field no platform defines here is simply absent; `os` is there as soon as any platform field is selected.
    if (names.some((name) => name.startsWith(PLATFORM_PREFIX))) {
      result.platform = { os: face.platform.os }
      for (const key of platformKeys) if (key in face.platform) result.platform[key] = face.platform[key]
    }
    return result
  }

  return source.map((family) => {
    const result = { family: family.family }
    for (const key of FAMILY_FIELDS) if (selected.has(key)) result[key] = family[key]
    if (languageCoverage) result.languageCoverage = family.languageCoverage
    if (hasFaceField) result.faces = family.faces.map(projectFace)
    return result
  })
}

// ---- the selections ---------------------------------------------------------------------------------------------

const SELECTIONS = [
  ...FIELD_CATALOGUE.map((spec) => [`fields: ['${spec.name}']`, { fields: [spec.name] }]),
  ...Object.keys(PRESETS).map((preset) => [`preset: '${preset}'`, { preset }]),
  ['fields: []', { fields: [] }],
  ['fields listed twice and already in the preset', { preset: 'names', fields: ['familyNames', 'faces.weight', 'faces.weight'] }],
  ['preset and fields together', { preset: 'styles', fields: ['faces.opentype.layout', 'faces.platform.shadowed', 'faces.coverage'] }],
  ['derived and base fields together', { fields: ['faces.fullName', 'faces.fullNames', 'faces.styleName', 'familyNames'] }],
  ['family fields without face fields', { fields: ['supportedLanguages', 'variable', 'enabled'] }],
  ['variations', { fields: ['variable', 'faces.axes', 'faces.namedInstanceIndex', 'faces.opentype.fvar'] }],
  ['two opentype entries and platform fields', { fields: ['faces.opentype.tables', 'faces.opentype.names', 'faces.platform.features'] }],
  [
    'fields of other platforms only',
    { fields: ['faces.platform.index', 'faces.platform.sourceName', 'faces.platform.typographicFamilyNames'] }
  ]
]

const usesCoverage = (options) => selectedFields(options).has('faces.coverage')

describe('projection of the full result', () => {
  let full
  let fullWithCoverage

  before(async () => {
    const { registerFont } = loadNative().__testing
    for (const file of TEST_FONTS) registerFont(file)
    ;[full, fullWithCoverage] = await Promise.all([
      getFamilies({ preset: 'full', families: FAMILIES }),
      getFamilies({ preset: 'full', fields: ['faces.coverage'], families: FAMILIES })
    ])
  })
  after(() => {
    const { unregisterFont } = loadNative().__testing
    for (const file of TEST_FONTS) unregisterFont(file)
  })

  test('the sample holds every family it names', () => {
    assert.deepEqual(
      full.map((family) => family.family),
      [...FAMILIES].sort()
    )
    assert.ok(full.every((family) => family.faces.length > 0))
  })

  test('the full result with faces.coverage differs from the full result by coverage alone', () => {
    const withoutCoverage = fullWithCoverage.map((family) => ({
      ...family,
      faces: family.faces.map(({ coverage, ...face }) => {
        assert.ok(Array.isArray(coverage))
        return face
      })
    }))
    assert.deepEqual(withoutCoverage, full)
  })

  test('the preset lists of this test are the ones of the field catalogue', () => {
    const rank = { names: 0, styles: 1, details: 2, full: 3 }
    for (const [preset, fields] of Object.entries(PRESETS)) {
      const catalogued = FIELD_CATALOGUE.filter((spec) => spec.preset !== null && rank[spec.preset] <= rank[preset]).map(
        (spec) => spec.name
      )
      assert.deepEqual([...new Set(fields)].sort(), catalogued.sort(), preset)
    }
  })

  for (const [name, options] of SELECTIONS) {
    test(name, async () => {
      const result = await getFamilies({ ...options, families: FAMILIES })
      const source = usesCoverage(options) ? fullWithCoverage : full
      assert.deepStrictEqual(result, project(source, selectedFields(options)))
    })
  }
})

describe('projection of languageCoverage', () => {
  let full

  before(async () => {
    const { registerFont } = loadNative().__testing
    for (const file of TEST_FONTS) registerFont(file)
    full = await getFamilies({ preset: 'full', families: FAMILIES, languageCoverage: LANGUAGE_COVERAGE })
  })
  after(() => {
    const { unregisterFont } = loadNative().__testing
    for (const file of TEST_FONTS) unregisterFont(file)
  })

  for (const [name, options] of [
    ['fields: []', { fields: [] }],
    ["preset: 'names'", { preset: 'names' }],
    ["fields: ['faces.weight']", { fields: ['faces.weight'] }],
    ["preset: 'details'", { preset: 'details' }]
  ]) {
    test(`${name} with languageCoverage`, async () => {
      const result = await getFamilies({ ...options, families: FAMILIES, languageCoverage: LANGUAGE_COVERAGE })
      assert.deepStrictEqual(result, project(full, selectedFields(options), { languageCoverage: true }))
    })
  }

  test('one entry per requested tag, in request order', async () => {
    const reversed = [...LANGUAGE_COVERAGE].reverse()
    const result = await getFamilies({ fields: [], families: FAMILIES, languageCoverage: reversed })
    assert.deepStrictEqual(
      result,
      full.map((family) => ({ family: family.family, languageCoverage: [...family.languageCoverage].reverse() }))
    )
  })
})

describe('projection on the whole system', () => {
  test('details equals the projection of the whole-system full result', async () => {
    const [full, details] = await Promise.all([getFamilies({ preset: 'full' }), getFamilies({ preset: 'details' })])
    assert.ok(full.length > 0)
    assert.deepStrictEqual(details, project(full, selectedFields({ preset: 'details' })))
  })
})
