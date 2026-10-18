// buildRequest and the option validation of getFamilies / getFamily. Validation needs no native addon: the errors
// are checked on buildRequest and, as rejections, on listFamilies with a binding that must never be reached.
import assert from 'node:assert/strict'
import { test } from 'node:test'

import { FIELD_CATALOGUE } from '../dist/field-catalogue.js'
import { listFamilies } from '../dist/list-families.js'
import { buildRequest } from '../dist/request.js'

const build = (options, platform = 'darwin', getFamily = false) => buildRequest(options, { getFamily, platform })
const names = (set) => [...set].sort()
const presetNames = (...presets) => FIELD_CATALOGUE.filter((spec) => presets.includes(spec.preset)).map((spec) => spec.name)

test('options that is not an object is a TypeError', () => {
  for (const options of [undefined, null, 'full', 7, true]) assert.throws(() => build(options), TypeError)
})

test('a field that is not a field name is a TypeError naming it', () => {
  assert.throws(() => build({ fields: ['faces.weight', 'faces.nope'] }), { name: 'TypeError', message: /faces\.nope/ })
  // `family` and `faces` are not field names; neither is a path deeper than the catalogue's levels.
  for (const field of ['family', 'faces', 'faces.axes.min', 'languageCoverage', 'faces.platform']) {
    assert.throws(() => build({ fields: [field] }), { name: 'TypeError', message: new RegExp(field.replace('.', '\\.')) })
  }
})

test('a field of another platform is a field name, not an error', () => {
  assert.doesNotThrow(() => build({ fields: ['faces.platform.typographicFamilyNames', 'faces.platform.foundry'] }, 'darwin'))
})

test('a preset that is not one of the four names is a TypeError', () => {
  for (const preset of ['everything', 'FULL', 3, null]) assert.throws(() => build({ preset }), TypeError)
})

test('fields that is not an array of strings is a TypeError', () => {
  for (const fields of ['faces.weight', null, {}, [1], ['familyNames', undefined]]) assert.throws(() => build({ fields }), TypeError)
})

test('families and languageCoverage that are not arrays of strings are a TypeError', () => {
  for (const value of ['Arial', null, {}, [1], [undefined]]) {
    assert.throws(() => build({ fields: [], families: value }), TypeError)
    assert.throws(() => build({ fields: [], languageCoverage: value }), TypeError)
  }
})

test('getFamily given a families option is a TypeError, even an empty or undefined-looking one', () => {
  assert.throws(() => build({ fields: [], families: ['A'] }, 'darwin', true), TypeError)
  assert.throws(() => build({ fields: [], families: [] }, 'darwin', true), TypeError)
  assert.doesNotThrow(() => build({ fields: [], families: undefined }, 'darwin', true))
})

test('neither fields nor preset is a TypeError; either alone, or fields: [], is fine', () => {
  assert.throws(() => build({}), TypeError)
  assert.throws(() => build({ families: ['A'], languageCoverage: ['en'] }), TypeError)
  assert.doesNotThrow(() => build({ fields: [] }))
  assert.doesNotThrow(() => build({ preset: 'names' }))
})

test('fields: [] selects nothing and fetches nothing', () => {
  const request = build({ fields: [] })
  assert.deepEqual(names(request.selected), [])
  assert.deepEqual(names(request.closure), [])
  assert.deepEqual(names(request.derived), [])
})

test('the selection is the fields joined with the preset, and duplicates are legal', () => {
  const request = build({ preset: 'names', fields: ['faces.weight', 'faces.weight', 'familyNames', 'faces.opentype.os2'] })
  assert.deepEqual(names(request.selected), names(['familyNames', 'faces.weight', 'faces.opentype.os2']))
})

test('each preset expands to the catalogue fields of that preset and the ones before it', () => {
  const nameSet = (preset) => names(build({ preset }).selected)
  assert.deepEqual(nameSet('names'), names(presetNames('names')))
  assert.deepEqual(nameSet('styles'), names(presetNames('names', 'styles')))
  assert.deepEqual(nameSet('details'), names(presetNames('names', 'styles', 'details')))
  assert.deepEqual(nameSet('full'), names(presetNames('names', 'styles', 'details', 'full')))
  assert.ok(!nameSet('full').includes('faces.coverage'))
})

test('the closure adds the identity for a face-level selection and the inputs of derived fields', () => {
  const closure = (options, platform) => names(build(options, platform).closure)
  assert.deepEqual(closure({ fields: ['faces.weight'] }), names(['faces.weight', 'faces.file', 'faces.coordinates', 'faces.simulations']))
  // A family field derived from face fields borrows them without bringing the identity.
  assert.deepEqual(closure({ fields: ['variable'] }), ['faces.variable'])
  // fullName is computed from fullNames, which is fetched instead.
  assert.deepEqual(
    closure({ fields: ['faces.fullName'] }),
    names(['faces.fullNames', 'faces.file', 'faces.coordinates', 'faces.simulations'])
  )
})

test('derivation is per platform: Windows gets familyNames from the OS, the others derive it', () => {
  const darwin = build({ fields: ['familyNames'] }, 'darwin')
  assert.deepEqual(names(darwin.closure), ['faces.familyNames'])
  assert.deepEqual(names(darwin.derived), ['familyNames'])
  const win32 = build({ fields: ['familyNames'] }, 'win32')
  assert.deepEqual(names(win32.closure), ['familyNames'])
  assert.deepEqual(names(win32.derived), [])
})

test('derived is the selected fields the platform computes', () => {
  const request = build({ fields: ['enabled', 'faces.styleName', 'faces.weight'] })
  assert.deepEqual(names(request.derived), ['enabled', 'faces.styleName'])
})

test('families and languageCoverage are passed through', () => {
  const request = build({ fields: [], families: ['A', 'B'], languageCoverage: ['en', 'ja'] })
  assert.deepEqual(request.families, ['A', 'B'])
  assert.deepEqual(request.languageCoverage, ['en', 'ja'])
  const none = build({ fields: [] })
  assert.equal(none.families, undefined)
  assert.equal(none.languageCoverage, undefined)
})

test('errors reach the caller as rejections, before the addon is loaded', async () => {
  const loadBinding = () => assert.fail('the addon must not be loaded for an invalid call')
  await assert.rejects(listFamilies(undefined, { getFamily: false }, { loadBinding }), TypeError)
  await assert.rejects(listFamilies({ fields: ['nope'] }, { getFamily: false }, { loadBinding }), TypeError)
  await assert.rejects(listFamilies({ fields: [], families: ['A'] }, { getFamily: true, family: 'A' }, { loadBinding }), TypeError)
})
