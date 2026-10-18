// projectFamilies: what is removed after a call fetched more than it was asked for. The closures come from
// buildRequest, the same way a call gets them.
import assert from 'node:assert/strict'
import { test } from 'node:test'

import { projectFamilies } from '../dist/project.js'
import { buildRequest } from '../dist/request.js'

const names = (...pairs) => pairs.map(([language, value]) => ({ language, value }))

/** A family the way the native side returns it for the closure: every face-level key of the closure, nothing else. */
function fetched(options, platform = 'darwin') {
  const request = buildRequest(options, { getFamily: false, platform })
  const face = {}
  for (const field of request.closure) {
    if (field.startsWith('faces.')) face[field.slice('faces.'.length)] = `<${field}>`
  }
  return { request, family: { family: 'F', faces: [face, { ...face }] } }
}

const project = ({ request, family }) => {
  projectFamilies([family], request.selected, request.closure)
  return family
}

test('a field borrowed for a derived field is removed', () => {
  const family = project(fetched({ fields: ['faces.fullName', 'faces.weight'] }))
  assert.deepEqual(Object.keys(family.faces[0]).sort(), ['coordinates', 'file', 'simulations', 'weight'])
})

test('the identity stays when a face field is selected, selected or not', () => {
  const family = project(fetched({ fields: ['faces.weight'] }))
  for (const face of family.faces) assert.deepEqual(Object.keys(face).sort(), ['coordinates', 'file', 'simulations', 'weight'])
})

test('a selected field stays even when another field also borrows it', () => {
  const family = project(fetched({ fields: ['faces.fullName', 'faces.fullNames'] }))
  assert.deepEqual(Object.keys(family.faces[0]).sort(), ['coordinates', 'file', 'fullNames', 'simulations'])
})

test('without a face-level field, faces is removed and no identity is involved', () => {
  const { request, family } = fetched({ fields: ['variable', 'enabled'] })
  assert.deepEqual([...request.closure].sort(), ['faces.enabled', 'faces.variable'])
  family.variable = true
  family.enabled = true
  assert.deepEqual(project({ request, family }), { family: 'F', variable: true, enabled: true })
})

test('fields: [] leaves only family', () => {
  assert.deepEqual(project(fetched({ fields: [] })), { family: 'F' })
})

test('family fields that are selected stay; borrowed family fields would go', () => {
  const { request, family } = fetched({ preset: 'names' })
  family.familyNames = names(['en', 'F'])
  assert.deepEqual(project({ request, family }), { family: 'F', familyNames: names(['en', 'F']) })
})

test('a derived-everywhere family field next to face fields keeps faces', () => {
  const { request, family } = fetched({ fields: ['supportedLanguages', 'faces.style'] })
  family.supportedLanguages = ['en']
  const projected = project({ request, family })
  assert.deepEqual(projected.supportedLanguages, ['en'])
  // faces.supportedLanguages was fetched for the family field only.
  assert.deepEqual(Object.keys(projected.faces[0]).sort(), ['coordinates', 'file', 'simulations', 'style'])
})

test('shared opentype and axes objects are not touched', () => {
  const request = buildRequest({ fields: ['faces.fullName', 'faces.axes', 'faces.opentype.os2'] }, { getFamily: false, platform: 'darwin' })
  const axes = [{ tag: 'wght', names: [], name: '' }]
  const opentype = { tables: ['OS/2'], os2: {} }
  const snapshot = structuredClone({ axes, opentype })
  const faces = [0, 1].map(() => ({ fullNames: [], axes, opentype, file: { path: 'x' }, coordinates: {}, simulations: {} }))
  const family = { family: 'F', faces }
  projectFamilies([family], request.selected, request.closure)
  assert.equal(family.faces[0].axes, axes)
  assert.equal(family.faces[1].axes, axes)
  assert.equal(family.faces[0].opentype, opentype)
  assert.equal(family.faces[1].opentype, opentype)
  assert.deepEqual({ axes, opentype }, snapshot)
  assert.equal('fullNames' in family.faces[0], false)
})

test('a fetched opentype or platform entry that was not selected cannot be removed and fails loudly', () => {
  const closure = new Set(['faces.opentype.os2'])
  assert.throws(() => projectFamilies([{ family: 'F', faces: [] }], new Set(), closure), Error)
})

test('platform keys of a selected platform field are untouched', () => {
  const request = buildRequest({ fields: ['faces.platform.weight'] }, { getFamily: false, platform: 'darwin' })
  const platform = { os: 'darwin', weight: 0.4 }
  const family = { family: 'F', faces: [{ file: { path: 'x' }, coordinates: {}, simulations: {}, platform }] }
  projectFamilies([family], request.selected, request.closure)
  assert.equal(family.faces[0].platform, platform)
})
