// The request closure and the groups it reaches, computed from the field catalogue. Reads dist/, which
// `npm run build:ts` (and `pretest`) builds.
import assert from 'node:assert/strict'
import { describe, test } from 'node:test'

import { FIELD_CATALOGUE } from '../dist/field-catalogue.js'
import { closureOf, groupsOf } from '../dist/field-closure.js'

const IDENTITY = ['faces.file', 'faces.coordinates', 'faces.simulations']
const sorted = (set) => [...set].sort()

describe('closureOf', () => {
  test('selecting nothing fetches nothing', () => {
    for (const platform of ['darwin', 'win32', 'linux']) {
      assert.deepEqual(sorted(closureOf([], platform)), [])
    }
  })

  test('a family field the platform gets from the OS brings no face field (Windows names)', () => {
    assert.deepEqual(sorted(closureOf(['familyNames'], 'win32')), ['familyNames'])
  })

  test('a derived family field is replaced by the face fields it is computed from, without identity (names)', () => {
    for (const platform of ['darwin', 'linux']) {
      assert.deepEqual(sorted(closureOf(['familyNames'], platform)), ['faces.familyNames'])
    }
  })

  test('selecting a face field adds the face identity', () => {
    for (const platform of ['darwin', 'win32', 'linux']) {
      assert.deepEqual(sorted(closureOf(['faces.weight'], platform)), ['faces.weight', ...IDENTITY].sort())
    }
  })

  test('opentype and platform entries select a face level too', () => {
    assert.deepEqual(sorted(closureOf(['faces.opentype.os2'], 'darwin')), ['faces.opentype.os2', ...IDENTITY].sort())
    assert.deepEqual(sorted(closureOf(['faces.platform.weight'], 'linux')), ['faces.platform.weight', ...IDENTITY].sort())
  })

  test('a derived face field is computed, not fetched, but still selects a face level', () => {
    assert.deepEqual(sorted(closureOf(['faces.fullName'], 'linux')), ['faces.fullNames', ...IDENTITY].sort())
  })

  test('a face field borrowed to derive a family field does not bring identity, one selected does', () => {
    assert.deepEqual(sorted(closureOf(['variable'], 'darwin')), ['faces.variable'])
    assert.deepEqual(sorted(closureOf(['variable', 'faces.weight'], 'darwin')), ['faces.variable', 'faces.weight', ...IDENTITY].sort())
  })

  test('a field selected twice, or both derived and selected, is in the closure once', () => {
    assert.deepEqual(sorted(closureOf(['familyNames', 'faces.familyNames'], 'darwin')), ['faces.familyNames', ...IDENTITY].sort())
  })

  test('the closure holds no field the platform derives', () => {
    for (const platform of ['darwin', 'win32', 'linux']) {
      const everyField = FIELD_CATALOGUE.map((spec) => spec.name)
      for (const field of closureOf(everyField, platform)) {
        const source = FIELD_CATALOGUE.find((spec) => spec.name === field).sources[platform]
        assert.ok(source === null || typeof source === 'string' || !('derivedFrom' in source), `${platform}: ${field}`)
      }
    }
  })
})

describe('groupsOf', () => {
  test('the family list is always reached', () => {
    assert.deepEqual(groupsOf([], 'darwin'), { always: ['A'], conditional: [] })
  })

  test('names costs A on Windows and Linux, A and C on macOS', () => {
    const groups = (platform) => groupsOf(closureOf(['familyNames'], platform), platform)
    assert.deepEqual(groups('win32'), { always: ['A'], conditional: [] })
    assert.deepEqual(groups('linux'), { always: ['A'], conditional: [] })
    assert.deepEqual(groups('darwin'), { always: ['A', 'C'], conditional: [] })
  })

  test('a group that is conditional for one field and unconditional for another is unconditional', () => {
    // Linux faces.format is B with C for cff2; faces.axes is C.
    assert.deepEqual(groupsOf(['faces.format'], 'linux'), { always: ['A', 'B'], conditional: [{ group: 'C', for: '`cff2`' }] })
    assert.deepEqual(groupsOf(['faces.format', 'faces.axes'], 'linux'), { always: ['A', 'B', 'C'], conditional: [] })
  })

  test('fields the platform does not provide cost nothing', () => {
    assert.deepEqual(groupsOf(['faces.platform.features'], 'linux'), { always: ['A'], conditional: [] })
  })
})
