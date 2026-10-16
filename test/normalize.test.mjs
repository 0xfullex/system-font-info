import assert from 'node:assert/strict'
import { test } from 'node:test'

import { normalizeFamilies } from '../lib/normalize.js'

const names = (...pairs) => pairs.map(([language, value]) => ({ language, value }))

function rawFace(overrides = {}) {
  return {
    postscriptName: 'Face-Regular',
    fullNames: [],
    styleNames: [],
    familyNames: [],
    weight: 400,
    width: 100,
    style: 'normal',
    monospace: false,
    color: false,
    symbol: false,
    enabled: true,
    hidden: false,
    simulations: { bold: false, oblique: false },
    languages: [],
    scope: 'system',
    format: 'truetype',
    variable: false,
    platform: { os: 'darwin' },
    ...overrides
  }
}

const rawFamily = (family, ...faces) => ({ family, faces })

test('language tags are canonicalized in every place that carries one', () => {
  const axes = [{ tag: 'wght', names: names(['zh-cn', '粗细']), min: 1, max: 9, default: 4, hidden: false }]
  const opentype = {
    tables: ['name'],
    names: [{ platformId: 3, encodingId: 1, languageId: 0x804, nameId: 1, language: 'zh-cn', value: 'x' }],
    fvar: {
      axes: [{ tag: 'wght', min: 1, default: 4, max: 9, flags: 0, nameId: 256, names: names(['zh-cn', 'a']) }],
      instances: [
        { subfamilyNameId: 257, subfamilyNames: names(['zh-tw', 'b']), flags: 0, coordinates: { wght: 4 } }
      ]
    },
    layout: {
      gsub: { scripts: {}, features: [], featureNames: { ss01: names(['en-us', 'c']) } },
      gpos: { scripts: {}, features: [], featureNames: { cv01: names(['ja-jp', 'd']) } }
    },
    meta: { dlng: ['Latn', 'zh-cn'], slng: ['und-zsye'] }
  }
  const [family] = normalizeFamilies([
    rawFamily(
      'F',
      rawFace({
        fullNames: names(['zh-cn', 'a']),
        styleNames: names(['en-us', 'b']),
        familyNames: names(['en-gb', 'c']),
        languages: ['zh-cn', 'und-zsye'],
        variable: true,
        axes,
        opentype
      })
    )
  ])
  const face = family.faces[0]
  assert.equal(face.fullNames[0].language, 'zh-CN')
  assert.equal(face.styleNames[0].language, 'en-US')
  assert.equal(face.familyNames[0].language, 'en-GB')
  assert.deepEqual(face.languages, ['zh-CN', 'und-Zsye'])
  assert.equal(face.axes[0].names[0].language, 'zh-CN')
  assert.equal(opentype.names[0].language, 'zh-CN')
  assert.equal(opentype.fvar.axes[0].names[0].language, 'zh-CN')
  assert.equal(opentype.fvar.instances[0].subfamilyNames[0].language, 'zh-TW')
  assert.equal(opentype.layout.gsub.featureNames.ss01[0].language, 'en-US')
  assert.equal(opentype.layout.gpos.featureNames.cv01[0].language, 'ja-JP')
  // "Latn" is a script tag, not a locale: Intl rejects it and it is kept as written.
  assert.deepEqual(opentype.meta.dlng, ['Latn', 'zh-CN'])
  assert.deepEqual(opentype.meta.slng, ['und-Zsye'])
})

test('an empty tag stays empty and an unparseable tag is kept as written', () => {
  const [family] = normalizeFamilies([
    rawFamily('F', rawFace({ fullNames: names(['', 'a'], ['not_a_tag', 'b']) }))
  ])
  assert.deepEqual(family.faces[0].fullNames, names(['', 'a'], ['not_a_tag', 'b']))
})

test('exact duplicates are merged after canonicalization, first occurrence kept', () => {
  const [family] = normalizeFamilies([
    rawFamily(
      'F',
      rawFace({
        fullNames: names(['en-us', 'A'], ['de', 'B'], ['en-US', 'A'], ['en-US', 'C']),
        languages: ['en', 'de', 'en-us', 'en-US', 'de']
      })
    )
  ])
  assert.deepEqual(family.faces[0].fullNames, names(['en-US', 'A'], ['de', 'B'], ['en-US', 'C']))
  assert.deepEqual(family.faces[0].languages, ['en', 'de', 'en-US'])
})

test('opentype.names is a raw view and is never merged', () => {
  const record = (value) => ({ platformId: 3, encodingId: 1, languageId: 0x409, nameId: 1, language: 'en-US', value })
  const opentype = { tables: ['name'], names: [record('A'), record('A'), record('A')] }
  normalizeFamilies([rawFamily('F', rawFace({ opentype }))])
  assert.equal(opentype.names.length, 3)
})

test('default string: en-US, else en / en-*, else "", else the first, else ""', () => {
  const fullName = (list) =>
    normalizeFamilies([rawFamily('F', rawFace({ fullNames: list }))])[0].faces[0].fullName
  assert.equal(fullName(names(['de', 'd'], ['en-GB', 'gb'], ['en-US', 'us'])), 'us')
  assert.equal(fullName(names(['de', 'd'], ['en-AU', 'au'], ['en', 'plain'])), 'au')
  assert.equal(fullName(names(['de', 'd'], ['en', 'plain'])), 'plain')
  assert.equal(fullName(names(['de', 'd'], ['', 'none'], ['fr', 'f'])), 'none')
  assert.equal(fullName(names(['de', 'd'], ['fr', 'f'])), 'd')
  assert.equal(fullName([]), '')
  // "end" is not English: the match is on the language subtag, not a string prefix.
  assert.equal(fullName(names(['end', 'x'], ['de', 'd'])), 'x')
})

test('default strings are set for styleName and for each axis name', () => {
  const axes = [
    { tag: 'wght', names: names(['de', 'Gewicht'], ['en-us', 'Weight']), min: 1, max: 9, default: 4, hidden: false },
    { tag: 'XXXX', names: [], min: 0, max: 1, default: 0, hidden: true }
  ]
  const [family] = normalizeFamilies([
    rawFamily('F', rawFace({ styleNames: names(['en', 'Bold']), variable: true, axes }))
  ])
  assert.equal(family.faces[0].styleName, 'Bold')
  assert.equal(family.faces[0].axes[0].name, 'Weight')
  assert.equal(family.faces[0].axes[1].name, '')
})

test('family aggregates', () => {
  const [family] = normalizeFamilies([
    rawFamily(
      'F',
      rawFace({
        familyNames: names(['en-us', 'F'], ['de', 'Fd']),
        languages: ['en', 'de'],
        enabled: false,
        hidden: true
      }),
      rawFace({
        familyNames: names(['en-US', 'F'], ['fr', 'Ff']),
        languages: ['fr', 'en'],
        enabled: true,
        hidden: true,
        variable: true
      })
    )
  ])
  assert.deepEqual(family.familyNames, names(['en-US', 'F'], ['de', 'Fd'], ['fr', 'Ff']))
  assert.deepEqual(family.languages, ['en', 'de', 'fr'])
  assert.equal(family.variable, true)
  assert.equal(family.enabled, true)
  assert.equal(family.hidden, true)
})

test('aggregates: not variable, none enabled, not every face hidden', () => {
  const [family] = normalizeFamilies([
    rawFamily('F', rawFace({ enabled: false, hidden: true }), rawFace({ enabled: false, hidden: false }))
  ])
  assert.equal(family.variable, false)
  assert.equal(family.enabled, false)
  assert.equal(family.hidden, false)
})

test('the family aggregates do not alias the faces\' arrays', () => {
  const [family] = normalizeFamilies([rawFamily('F', rawFace({ familyNames: names(['en', 'F']) }))])
  assert.notEqual(family.familyNames, family.faces[0].familyNames)
})

test('families are sorted by lowercase code units, ties by the raw string', () => {
  const sorted = normalizeFamilies(
    ['b', 'a', 'Z', 'B', 'ä', 'A', 'z', 'Éa', 'ea'].map((name) => rawFamily(name, rawFace()))
  ).map((family) => family.family)
  // Case is folded first, so "ea" sorts before "z"; "ä" and "Éa" come after ASCII by code unit; the raw string breaks ties.
  assert.deepEqual(sorted, ['A', 'a', 'B', 'b', 'ea', 'Z', 'z', 'ä', 'Éa'])
})

test('faces keep their order', () => {
  const [family] = normalizeFamilies([
    rawFamily('F', rawFace({ postscriptName: 'z' }), rawFace({ postscriptName: 'a' }))
  ])
  assert.deepEqual(
    family.faces.map((face) => face.postscriptName),
    ['z', 'a']
  )
})

test('objects shared between faces stay one object', () => {
  const axes = [{ tag: 'wght', names: names(['en-us', 'Weight'], ['en-US', 'Weight']), min: 1, max: 9, default: 4, hidden: false }]
  const opentype = {
    tables: ['fvar'],
    names: [],
    fvar: { axes: [{ tag: 'wght', min: 1, default: 4, max: 9, flags: 0, nameId: 1, names: names(['en-us', 'W']) }], instances: [] }
  }
  const faces = [0, 1].map(() => rawFace({ variable: true, axes, opentype }))
  const [family] = normalizeFamilies([rawFamily('F', ...faces)])
  assert.equal(family.faces[0].opentype, family.faces[1].opentype)
  assert.equal(family.faces[0].axes, family.faces[1].axes)
  assert.equal(family.faces[0].axes[0].names.length, 1)
  assert.equal(family.faces[0].axes[0].name, 'Weight')
  assert.equal(opentype.fvar.axes[0].names[0].language, 'en-US')
})
