// A call does only the OS work its fields need (docs/platforms.md, "Source groups"). The native test hook
// `getFamiliesWithWork` returns the work of a call counted per group: C (font resources opened), D (OpenType entries
// built) and E (character sets walked). For every preset and every field on its own, the counter of each group that the
// catalogue does not give the selection's closure on this platform must be zero. A group that is only conditional
// (`C for named instances`) counts as possibly needed.
//
// Platform independent: the groups come from the catalogue for `process.platform`. A, the family list, and B, the
// per-face attributes, are not counted: A is the minimum of every call and B is cheap and not separable on every platform.
import assert from 'node:assert/strict'
import { after, before, describe, test } from 'node:test'
import { fileURLToPath } from 'node:url'

import { FIELD_CATALOGUE } from '../dist/field-catalogue.js'
import { closureOf, groupsOf, PRESETS, presetFields } from '../dist/field-closure.js'
import { loadNative } from '../dist/native.js'

const platform = process.platform
const fixture = (name) => fileURLToPath(new URL(`./fixtures/${name}`, import.meta.url))
const TEST_FONTS = ['SFITest-Variable.ttf', 'SFITest-Collection.ttc', 'SFITest-Symbol.ttf', 'SFITest-Legacy.ttf'].map(fixture)
const FAMILIES = [
  'SFITest Collection',
  'SFITest Legacy',
  'SFITest Symbol',
  'SFITest Variable',
  ...(platform === 'darwin' ? ['Helvetica Neue', 'Skia', 'PingFang SC'] : [])
]

/** The counter of each group that is counted. */
const COUNTERS = { C: 'fontsOpened', D: 'tableEntries', E: 'characterSets' }

const SELECTIONS = [
  ...PRESETS.map((preset) => [`preset '${preset}'`, presetFields(preset)]),
  ['fields: []', []],
  ...FIELD_CATALOGUE.map((spec) => [`field '${spec.name}'`, [spec.name]])
]

describe('work per group', () => {
  let getFamiliesWithWork

  before(() => {
    const { registerFont, getFamiliesWithWork: hook } = loadNative().__testing
    for (const file of TEST_FONTS) registerFont(file)
    getFamiliesWithWork = hook
  })
  after(() => {
    const { unregisterFont } = loadNative().__testing
    for (const file of TEST_FONTS) unregisterFont(file)
  })

  /** `families` is null for the whole collection (the option is then omitted): a different path in the backend from a list of families. */
  const workOf = async (selected, orthographies = [], families = FAMILIES) => {
    const closure = closureOf(selected, platform)
    const { work } = await getFamiliesWithWork({ fields: [...closure], ...(families && { families }), orthographies })
    return { work, closure }
  }

  /** The groups a call may do work in: those the closure's fields need, always or in some cases. E needs an orthography. */
  const allowedGroups = (closure, orthographies) => {
    const { always, conditional } = groupsOf(closure, platform)
    const allowed = new Set([...always, ...conditional.map(({ group }) => group)])
    if (orthographies.length > 0) allowed.add('E')
    return allowed
  }

  /** Asserts that every counted group the catalogue does not give the closure did no work. Returns the groups that may. */
  const assertNoWorkOutsideCatalogue = (work, closure, orthographies = []) => {
    const allowed = allowedGroups(closure, orthographies)
    for (const [group, counter] of Object.entries(COUNTERS)) {
      if (!allowed.has(group)) assert.equal(work[counter], 0, `${counter} (group ${group}) for ${[...closure].join(', ')}`)
    }
    return allowed
  }

  for (const [name, selected] of SELECTIONS) {
    test(name, async (t) => {
      const { work, closure } = await workOf(selected)
      const undocumented = [...closure].filter(
        (field) => FIELD_CATALOGUE.find((spec) => spec.name === field)?.sources[platform] === 'undocumented'
      )
      if (undocumented.length > 0) return t.skip(`the source of ${undocumented.join(', ')} is not documented on ${platform}`)
      assertNoWorkOutsideCatalogue(work, closure)
    })
  }

  // Without `families` the backend lists the whole collection, a path of its own: the catalogue holds it to the same work.
  describe('the whole collection (no families)', () => {
    /** A field whose closure the catalogue gives groups A and B only on this platform: a family-level field needs no face identity. */
    const onlyAB = FIELD_CATALOGUE.find((spec) => {
      const { always, conditional } = groupsOf(closureOf([spec.name], platform), platform)
      return spec.level === 'family' && conditional.length === 0 && always.every((group) => group === 'A' || group === 'B')
    })

    test('fields: [] does no C, D or E work', async () => {
      const { work, closure } = await workOf([], [], null)
      assert.deepEqual([...closure], [])
      assert.deepEqual(work, { fontsOpened: 0, tableEntries: 0, characterSets: 0 })
    })

    test('a field the catalogue gives groups A and B only does no C, D or E work', async () => {
      assert.ok(onlyAB, 'the catalogue has a family-level field of groups A and B on this platform')
      const { work, closure } = await workOf([onlyAB.name], [], null)
      const allowed = assertNoWorkOutsideCatalogue(work, closure)
      assert.ok(!['C', 'D', 'E'].some((group) => allowed.has(group)), `${onlyAB.name} reaches groups ${[...allowed]}`)
    })

    test('languageCoverage alone does E work only', async () => {
      const { work } = await workOf([], ['zh-cn', 'ja'], null)
      assert.ok(work.characterSets > 0, 'characterSets')
      assert.equal(work.fontsOpened, 0, 'fontsOpened')
      assert.equal(work.tableEntries, 0, 'tableEntries')
    })
  })

  test('languageCoverage alone does group E work and no C or D work', async () => {
    const { work, closure } = await workOf([], ['zh-cn', 'ja'])
    assert.deepEqual([...closure], [])
    assert.ok(work.characterSets > 0, 'characterSets')
    assert.equal(work.fontsOpened, 0, 'fontsOpened')
    assert.equal(work.tableEntries, 0, 'tableEntries')
  })

  test('the counters count: the full result with coverage does work of every counted group', async () => {
    const { work } = await workOf([...presetFields('full'), 'faces.coverage'], ['zh-cn'])
    assert.ok(work.fontsOpened > 0, 'fontsOpened')
    assert.ok(work.tableEntries > 0, 'tableEntries')
    assert.ok(work.characterSets > 0, 'characterSets')
  })
})
