import assert from 'node:assert/strict'
import { test } from 'node:test'

import { normalizeFamilies } from '../dist/normalize.js'

const locations = [
  ...['typographicFamilyNames', 'typographicStyleNames', 'designLanguages', 'supportedLanguages'].map((key) => ({
    name: key,
    wrap: (strings) => ({ [key]: strings }),
    read: (platform) => platform[key]
  })),
  {
    name: 'resourceAxes[].names',
    wrap: (strings) => ({ resourceAxes: [{ tag: 'wght', min: 100, max: 900, default: 400, attributes: 0, names: strings }] }),
    read: (platform) => platform.resourceAxes[0].names
  },
  {
    name: 'additionalInformationalStrings[key]',
    wrap: (strings) => ({ additionalInformationalStrings: { 19: strings } }),
    read: (platform) => platform.additionalInformationalStrings[19]
  }
]

function normalizePlatform(platform) {
  const face = { platform }
  const family = { family: 'F', faces: [face] }
  const [result] = normalizeFamilies([family])
  assert.equal(result, family)
  assert.equal(result.faces[0], face)
  assert.equal(face.platform, platform)
  return platform
}

for (const { name, wrap, read } of locations) {
  test(`Windows ${name} canonicalizes before merging and preserves raw values and references`, () => {
    const value = ' Latn; cyrl, en-us  zh-cn\n'
    const strings = [
      { language: 'en-us', value },
      { language: 'de-de', value: 'Other' },
      { language: 'en-US', value },
      { language: 'iw-il', value: 'Alias' },
      { language: 'he-IL', value: 'Alias' },
      { language: 'en-US', value: `${value}different` },
      { language: '', value: 'Unspecified' },
      { language: '', value: 'Unspecified' },
      { language: 'not_a_tag', value: 'Invalid' },
      { language: 'not_a_tag', value: 'Invalid' }
    ]
    const kept = [strings[0], strings[1], strings[3], strings[5], strings[6], strings[8]]
    const platform = { os: 'win32', ...wrap(strings) }
    const keys = Object.keys(platform)

    normalizePlatform(platform)

    assert.deepEqual(Object.keys(platform), keys)
    assert.equal(read(platform), strings)
    assert.deepEqual(strings, [
      { language: 'en-US', value },
      { language: 'de-DE', value: 'Other' },
      { language: 'he-IL', value: 'Alias' },
      { language: 'en-US', value: `${value}different` },
      { language: '', value: 'Unspecified' },
      { language: 'not_a_tag', value: 'Invalid' }
    ])
    for (const [index, entry] of strings.entries()) assert.equal(entry, kept[index])
  })
}

test('Windows normalization handles every resource axis and informational-string key in place', () => {
  const names = () => [{ language: 'en-us', value: 'Raw' }]
  const resourceAxes = ['wght', 'wdth'].map((tag) => ({ tag, min: 0, max: 1, default: 0, attributes: 0, names: names() }))
  const additionalInformationalStrings = { 19: names(), 20: names() }
  const platform = { os: 'win32', resourceAxes, additionalInformationalStrings }
  const axes = [...resourceAxes]
  const arrays = [...resourceAxes.map((axis) => axis.names), ...Object.values(additionalInformationalStrings)]
  const faces = [{ platform }, { platform }]

  const [family] = normalizeFamilies([{ family: 'F', faces }])

  assert.equal(family.faces, faces)
  for (const face of faces) assert.equal(face.platform, platform)
  assert.equal(platform.resourceAxes, resourceAxes)
  assert.equal(platform.additionalInformationalStrings, additionalInformationalStrings)
  for (const [index, axis] of resourceAxes.entries()) assert.equal(axis, axes[index])
  const normalizedArrays = [...resourceAxes.map((axis) => axis.names), ...Object.values(additionalInformationalStrings)]
  for (const [index, strings] of normalizedArrays.entries()) assert.equal(strings, arrays[index])
  for (const strings of arrays) assert.deepEqual(strings, [{ language: 'en-US', value: 'Raw' }])
  assert.equal('name' in resourceAxes[0], false)
})

test('sparse Windows platform fields stay sparse, including empty arrays and records', () => {
  for (const fields of [{}, { typographicFamilyNames: [] }, { resourceAxes: [] }, { additionalInformationalStrings: {} }]) {
    const platform = { os: 'win32', ...fields }
    const before = structuredClone(platform)
    normalizePlatform(platform)
    assert.deepEqual(platform, before)
  }
  const raw = [{ family: 'F' }, { family: 'G', faces: [{}] }]
  assert.deepEqual(normalizeFamilies(raw), raw)
})

test('Darwin and Linux platform values are unchanged', () => {
  for (const os of ['darwin', 'linux']) {
    const platform = { os, supportedLanguages: ['en-us', 'en-US'] }
    const before = structuredClone(platform)
    normalizePlatform(platform)
    assert.deepEqual(platform, before)
  }
})

test('a bare four-letter script tag becomes und-<Script> on every language field; opentype.meta is untouched; qualified and language tags are unchanged', () => {
  const tags = () => ['Latn', 'hans', 'zh-cn', 'Latn', 'en', 'und-Cyrl', 'Grek1']
  const expected = ['und-Latn', 'und-Hans', 'zh-CN', 'en', 'und-Cyrl', 'Grek1']

  const [family] = normalizeFamilies([{ family: 'F', faces: [{ supportedLanguages: tags() }] }])
  assert.deepEqual(family.faces[0].supportedLanguages, expected)

  const [derived] = normalizeFamilies(
    [{ family: 'F', faces: [{ supportedLanguages: tags() }, { supportedLanguages: ['Arab', 'und-Latn'] }] }],
    new Set(['supportedLanguages'])
  )
  assert.deepEqual(derived.supportedLanguages, [...expected, 'und-Arab'])

  const [windows] = normalizeFamilies([
    { family: 'F', faces: [{ platform: { os: 'win32', supportedLanguages: [{ language: 'cyrl', value: 'x' }] } }] }
  ])
  assert.equal(windows.faces[0].platform.supportedLanguages[0].language, 'und-Cyrl')

  const [meta] = normalizeFamilies([
    { family: 'F', faces: [{ opentype: { meta: { dlng: ['Latn', 'zh-cn'], slng: ['Cyrl'], tags: [] } } }] }
  ])
  assert.deepEqual(meta.faces[0].opentype.meta.dlng, ['Latn', 'zh-CN'])
  assert.deepEqual(meta.faces[0].opentype.meta.slng, ['Cyrl'])
})
