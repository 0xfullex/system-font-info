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
const COUNTERS = { C: 'fontResources', D: 'tableEntries', E: 'characterSets' }

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

  const workOf = async (selected, orthographies = []) => {
    const closure = closureOf(selected, platform)
    const { work } = await getFamiliesWithWork({ fields: [...closure], families: FAMILIES, orthographies })
    return { work, closure }
  }

  /** The groups a call may do work in: those the closure's fields need, always or in some cases. E needs an orthography. */
  const allowedGroups = (closure, orthographies) => {
    const { always, conditional } = groupsOf(closure, platform)
    const allowed = new Set([...always, ...conditional.map(({ group }) => group)])
    if (orthographies.length > 0) allowed.add('E')
    return allowed
  }

  for (const [name, selected] of SELECTIONS) {
    test(name, async (t) => {
      const { work, closure } = await workOf(selected)
      const undocumented = [...closure].filter(
        (field) => FIELD_CATALOGUE.find((spec) => spec.name === field)?.sources[platform] === 'undocumented'
      )
      if (undocumented.length > 0) return t.skip(`the source of ${undocumented.join(', ')} is not documented on ${platform}`)
      const allowed = allowedGroups(closure, [])
      for (const [group, counter] of Object.entries(COUNTERS)) {
        if (!allowed.has(group)) assert.equal(work[counter], 0, `${counter} (group ${group}) for ${[...closure].join(', ')}`)
      }
    })
  }

  test('languageCoverage alone does group E work and no C or D work', async () => {
    const { work, closure } = await workOf([], ['zh-cn', 'ja'])
    assert.deepEqual([...closure], [])
    assert.ok(work.characterSets > 0, 'characterSets')
    assert.equal(work.fontResources, 0, 'fontResources')
    assert.equal(work.tableEntries, 0, 'tableEntries')
  })

  test('the counters count: the full result with coverage does work of every counted group', async () => {
    const { work } = await workOf([...presetFields('full'), 'faces.coverage'], ['zh-cn'])
    assert.ok(work.fontResources > 0, 'fontResources')
    assert.ok(work.tableEntries > 0, 'tableEntries')
    assert.ok(work.characterSets > 0, 'characterSets')
  })
})
