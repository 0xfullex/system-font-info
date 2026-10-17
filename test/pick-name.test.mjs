import assert from 'node:assert/strict'
import { test } from 'node:test'

import { pickName } from '../dist/pick-name.js'

const names = (...pairs) => pairs.map(([language, value]) => ({ language, value }))

test('an exact tag wins over looser matches in the same locale', () => {
  const list = names(['en', 'plain'], ['en-US', 'american'], ['en-GB', 'british'])
  assert.equal(pickName(list, ['en-GB']), 'british')
  assert.equal(pickName(list, ['en-US']), 'american')
})

test('tags are compared canonically, so case does not matter', () => {
  assert.equal(pickName(names(['zh-CN', 'a']), ['zh-cn']), 'a')
})

test('zh-CN matches zh-Hans-CN and zh-Hans, not zh-TW', () => {
  assert.equal(pickName(names(['zh-TW', 'tw'], ['zh-Hans-CN', 'cn']), ['zh-CN']), 'cn')
  assert.equal(pickName(names(['zh-TW', 'tw'], ['zh-Hans', 'hans']), ['zh-CN']), 'hans')
})

test('zh-TW matches zh-Hant and zh-Hant-TW', () => {
  assert.equal(pickName(names(['zh-Hans', 'hans'], ['zh-Hant', 'hant']), ['zh-TW']), 'hant')
  assert.equal(pickName(names(['zh-CN', 'cn'], ['zh-Hant-TW', 'tw']), ['zh-TW']), 'tw')
})

test('region is ignored at the language-and-script level', () => {
  // zh-Hant-HK and zh-TW differ in region but share language and script.
  assert.equal(pickName(names(['zh-Hans', 'hans'], ['zh-Hant-HK', 'hk']), ['zh-TW']), 'hk')
})

test('falls back to the bare language', () => {
  assert.equal(pickName(names(['de', 'de'], ['fr', 'fr']), ['fr-CA']), 'fr')
  assert.equal(pickName(names(['fr-FR', 'fr']), ['fr-CA']), 'fr')
})

test('en-GB prefers en-GB, then any English, over a different language', () => {
  const list = names(['de', 'de'], ['en-US', 'american'], ['en-GB', 'british'])
  assert.equal(pickName(list, ['en-GB']), 'british')
  assert.equal(pickName(names(['de', 'de'], ['en-US', 'american']), ['en-GB']), 'american')
})

test('an earlier locale beats a later locale with a better match level', () => {
  const list = names(['ja', 'ja'], ['en-US', 'american'])
  // en-GB only matches by language, ja matches exactly, but en-GB is requested first.
  assert.equal(pickName(list, ['en-GB', 'ja']), 'american')
  assert.equal(pickName(list, ['ja', 'en-GB']), 'ja')
})

test('within a match level the first entry in source order wins', () => {
  assert.equal(pickName(names(['en-US', 'first'], ['en-GB', 'second']), ['en-AU']), 'first')
})

test('entries with an empty language never match', () => {
  assert.equal(pickName(names(['', 'none']), ['en']), undefined)
  assert.equal(pickName(names(['', 'none'], ['en', 'english']), ['en']), 'english')
})

test('an empty requested locale matches nothing', () => {
  assert.equal(pickName(names(['', 'none'], ['en', 'english']), ['']), undefined)
})

test('returns undefined when nothing matches', () => {
  assert.equal(pickName(names(['de', 'de']), ['ja', 'ko']), undefined)
  assert.equal(pickName([], ['en']), undefined)
  assert.equal(pickName(names(['en', 'english']), []), undefined)
})

test('tags Intl rejects match only an identical tag', () => {
  assert.equal(pickName(names(['en', 'english'], ['x_bad', 'bad']), ['x_bad']), 'bad')
  assert.equal(pickName(names(['x_bad', 'bad']), ['en']), undefined)
  assert.equal(pickName(names(['en', 'english']), ['x_bad']), undefined)
  // A rejected locale does not stop the search for the next one.
  assert.equal(pickName(names(['en', 'english']), ['x_bad', 'en']), 'english')
})

test('undetermined languages do not match through likely subtags', () => {
  assert.equal(pickName(names(['und', 'unknown']), ['en']), undefined)
  assert.equal(pickName(names(['und-Zsye', 'emoji']), ['und-Zsye']), 'emoji')
})
