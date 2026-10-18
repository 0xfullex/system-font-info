// The orthography table built into the addon: fontconfig's fc-lang, one entry per .orth file.
// Loads the addon binary directly, so it does not depend on the TypeScript build.
import assert from 'node:assert/strict'
import { createRequire } from 'node:module'
import { describe, test } from 'node:test'

const { orthographies } = createRequire(import.meta.url)('../build/Release/system-font-info.node')

describe('binding.orthographies', () => {
  test('has one entry per fontconfig orthography file', () => {
    assert.equal(Object.keys(orthographies).length, 284)
  })

  test('counts the code points of the orthographies the contract names', () => {
    assert.equal(orthographies['zh-cn'], 6765)
    assert.equal(orthographies['zh-tw'], 13063)
    assert.equal(orthographies.ja, 2314)
  })

  test('names are lower-case words joined by "-"', () => {
    for (const name of Object.keys(orthographies)) {
      assert.match(name, /^[a-z0-9]+(-[a-z0-9]+)*$/, name)
    }
  })

  test('every count is a positive integer', () => {
    for (const [name, count] of Object.entries(orthographies)) {
      assert.ok(Number.isInteger(count) && count > 0, `${name}: ${count}`)
    }
  })
})
