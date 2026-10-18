// The pipeline behind getFamilies and getFamily, against a fake binding that records the request it receives.
import assert from 'node:assert/strict'
import { test } from 'node:test'

import { listFamilies } from '../dist/list-families.js'

const names = (...pairs) => pairs.map(([language, value]) => ({ language, value }))

function fakeBinding(rawFamilies, orthographies = { 'zh-tw': 13063, ja: 2314, sh: 100 }) {
  const requests = []
  return {
    requests,
    orthographies,
    getFamilies: async (request) => {
      requests.push(request)
      return structuredClone(rawFamilies)
    }
  }
}

const run = (binding, options, mode = { getFamily: false }, platform = 'darwin') =>
  listFamilies(options, mode, { loadBinding: () => binding, platform })

const raw = [
  {
    family: 'b',
    faces: [{ postscriptName: 'B', familyNames: names(['en', 'b']), file: { path: '/b' }, coordinates: undefined, simulations: {} }]
  },
  { family: 'A', faces: [{ postscriptName: 'A', familyNames: names(['en', 'A']), file: { path: '/a' }, simulations: {} }] }
]

test('the native request carries the closure, the families and the orthographies', async () => {
  const binding = fakeBinding([])
  await run(binding, { fields: ['familyNames'], families: ['A', 'B'], languageCoverage: ['zh_TW', 'ja', 'zh-Hant'] })
  assert.deepEqual(binding.requests, [{ fields: ['faces.familyNames'], families: ['A', 'B'], orthographies: ['zh-tw', 'ja'] }])
  await run(binding, { fields: [] })
  assert.deepEqual(binding.requests[1], { fields: [], families: undefined, orthographies: [] })
})

test('families come back sorted, projected and derived', async () => {
  const result = await run(fakeBinding(raw), { fields: ['familyNames'] })
  assert.deepEqual(result, [
    { family: 'A', familyNames: names(['en', 'A']) },
    { family: 'b', familyNames: names(['en', 'b']) }
  ])
})

test('selected face fields keep their faces and the identity', async () => {
  // The fake returns what the closure of this selection names: the field and the identity.
  const faces = [{ postscriptName: 'A', file: { path: '/a' }, simulations: {} }]
  const [first] = await run(fakeBinding([{ family: 'A', faces }]), { fields: ['faces.postscriptName'] })
  assert.deepEqual(first, { family: 'A', faces })
})

test('languageCoverage entries are assembled in request order, with required from the binding', async () => {
  const withCounts = raw.map((family, index) => ({ ...family, languageCoverage: [10 * (index + 1), 7 * (index + 1)] }))
  const result = await run(fakeBinding(withCounts), { fields: [], languageCoverage: ['ja', 'zh_TW', 'JA'] })
  // Sorted: A (second raw family, counts 20 and 14) before b (counts 10 and 7).
  assert.deepEqual(result, [
    {
      family: 'A',
      languageCoverage: [
        { language: 'ja', orthography: 'ja', required: 2314, covered: 20 },
        { language: 'zh-TW', orthography: 'zh-tw', required: 13063, covered: 14 },
        { language: 'ja', orthography: 'ja', required: 2314, covered: 20 }
      ]
    },
    {
      family: 'b',
      languageCoverage: [
        { language: 'ja', orthography: 'ja', required: 2314, covered: 10 },
        { language: 'zh-TW', orthography: 'zh-tw', required: 13063, covered: 7 },
        { language: 'ja', orthography: 'ja', required: 2314, covered: 10 }
      ]
    }
  ])
})

test('an empty languageCoverage gives every family an empty list', async () => {
  const result = await run(fakeBinding(raw), { fields: [], languageCoverage: [] })
  assert.deepEqual(result, [
    { family: 'A', languageCoverage: [] },
    { family: 'b', languageCoverage: [] }
  ])
})

test('an unresolvable languageCoverage tag rejects with a RangeError after the addon was loaded', async () => {
  const binding = fakeBinding(raw)
  const loads = []
  const loadBinding = () => {
    loads.push(binding)
    return binding
  }
  await assert.rejects(listFamilies({ fields: [], languageCoverage: ['xx'] }, { getFamily: false }, { loadBinding }), {
    name: 'RangeError',
    message: /"xx"/
  })
  assert.equal(loads.length, 1)
  assert.equal(binding.requests.length, 0)
})

test('getFamily asks for the one family and resolves to it, or to undefined', async () => {
  const binding = fakeBinding([raw[1]])
  const [found] = await run(binding, { fields: ['faces.postscriptName'] }, { getFamily: true, family: 'A' })
  assert.equal(found.family, 'A')
  assert.deepEqual(binding.requests[0].families, ['A'])
  const none = await run(fakeBinding([]), { fields: [] }, { getFamily: true, family: 'zzz' })
  assert.deepEqual(none, [])
})

test('Windows: familyNames comes from the addon, and the faces are not requested for it', async () => {
  const binding = fakeBinding([{ family: 'F', familyNames: names(['en-us', 'F']) }])
  const result = await run(binding, { fields: ['familyNames'] }, { getFamily: false }, 'win32')
  assert.deepEqual(binding.requests[0].fields, ['familyNames'])
  assert.deepEqual(result, [{ family: 'F', familyNames: names(['en-US', 'F']) }])
})
