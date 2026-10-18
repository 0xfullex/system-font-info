// Tag resolution of the languageCoverage option (docs/api.md, "Matching a tag to an orthography"), with an injected
// list of orthography names.
import assert from 'node:assert/strict'
import { test } from 'node:test'

import { coverageEntries, resolveOrthography, resolveTags } from '../dist/language-coverage.js'

// The names of the table in api.md plus the fc-lang names that Intl would rewrite into other tags.
const NAMES = new Set(['zh-cn', 'zh-tw', 'zh-hk', 'pa', 'pa-pk', 'en', 'ja', 'sr', 'sh', 'ro', 'mo', 'fil', 'tl', 'pap-an', 'und-zmth'])
const resolve = (tag) => resolveOrthography(tag, NAMES).orthography

test('the worked table of api.md', () => {
  assert.equal(resolve('sh'), 'sh')
  assert.equal(resolve('zh_TW'), 'zh-tw')
  assert.equal(resolve('zh-HK'), 'zh-hk')
  assert.equal(resolve('zh-Hans'), 'zh-cn')
  assert.equal(resolve('zh-Hant'), 'zh-tw')
  assert.equal(resolve('zh'), 'zh-cn')
  assert.equal(resolve('pa-Arab'), 'pa-pk')
  assert.equal(resolve('en-US'), 'en')
  assert.equal(resolve('sr-Latn'), 'sr')
})

test('step (0): an fc-lang name is used as written, lowercased, with _ read as -', () => {
  // Intl rewrites these: sh -> sr-Latn, mo -> ro, tl -> fil, pap-AN -> pap-CW. Step (0) keeps the name asked for.
  assert.equal(resolve('sh'), 'sh')
  assert.equal(resolve('mo'), 'mo')
  assert.equal(resolve('tl'), 'tl')
  assert.equal(resolve('pap-AN'), 'pap-an')
  assert.equal(resolve('PAP_an'), 'pap-an')
  assert.equal(resolve('und-Zmth'), 'und-zmth')
  assert.equal(resolve('ZH_cn'), 'zh-cn')
})

test('the language is the requested tag, canonicalized', () => {
  assert.deepEqual(resolveOrthography('zh_tw', NAMES), { language: 'zh-TW', orthography: 'zh-tw' })
  assert.deepEqual(resolveOrthography('sh', NAMES), { language: 'sr-Latn', orthography: 'sh' })
  assert.deepEqual(resolveOrthography('en-us', NAMES), { language: 'en-US', orthography: 'en' })
})

test('(a) uses the tag own language and region, ignoring the script', () => {
  assert.equal(resolve('zh-Hant-HK'), 'zh-hk')
  assert.equal(resolve('zh-Hans-TW'), 'zh-tw')
})

test('(c) falls back to the language alone', () => {
  assert.equal(resolve('ja-JP'), 'ja')
  assert.equal(resolve('en-GB'), 'en')
})

test('a tag that resolves to no orthography is a RangeError naming it', () => {
  assert.throws(() => resolve('xx'), { name: 'RangeError', message: /"xx"/ })
  assert.throws(() => resolve('de-DE'), { name: 'RangeError', message: /"de-DE"/ })
  assert.throws(() => resolveOrthography('en', new Set()), RangeError)
})

test('a tag that is not a well-formed language tag is a RangeError naming it', () => {
  for (const tag of ['', 'not a tag', 'e', '123', 'en--US', 'zh_']) {
    assert.throws(() => resolve(tag), { name: 'RangeError', message: new RegExp(`"${tag}"`) }, tag)
  }
})

test('resolveTags keeps the request order and lists each orthography once, first-seen first', () => {
  const { tags, orthographies } = resolveTags(['ja', 'zh-Hant', 'zh_TW', 'en-US', 'ja-JP'], NAMES)
  assert.deepEqual(
    tags.map((tag) => tag.orthography),
    ['ja', 'zh-tw', 'zh-tw', 'en', 'ja']
  )
  assert.deepEqual(orthographies, ['ja', 'zh-tw', 'en'])
  assert.deepEqual(resolveTags([], NAMES), { tags: [], orthographies: [] })
  assert.throws(() => resolveTags(['ja', 'xx'], NAMES), RangeError)
})

test('coverageEntries gives one entry per requested tag, in request order, repeated tags included', () => {
  const { tags, orthographies } = resolveTags(['zh-TW', 'ja', 'zh_TW'], NAMES)
  const entries = coverageEntries(tags, orthographies, [100, 20], { 'zh-tw': 13063, ja: 2314 })
  assert.deepEqual(entries, [
    { language: 'zh-TW', orthography: 'zh-tw', required: 13063, covered: 100 },
    { language: 'ja', orthography: 'ja', required: 2314, covered: 20 },
    { language: 'zh-TW', orthography: 'zh-tw', required: 13063, covered: 100 }
  ])
  assert.deepEqual(coverageEntries([], [], [], {}), [])
})

test('coverageEntries fails loudly when the native side lacks a count', () => {
  const { tags, orthographies } = resolveTags(['ja'], NAMES)
  assert.throws(() => coverageEntries(tags, orthographies, [], { ja: 2314 }), Error)
  assert.throws(() => coverageEntries(tags, orthographies, [1], {}), Error)
})
