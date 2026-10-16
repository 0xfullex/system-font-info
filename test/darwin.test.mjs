// The macOS backend against the fonts of the machine and the generated test
// fonts registered for this process. Skipped elsewhere.
import assert from 'node:assert/strict'
import { execFile } from 'node:child_process'
import { after, before, describe, test } from 'node:test'
import { fileURLToPath } from 'node:url'
import { promisify } from 'node:util'

import { getFonts, pickName, watch } from '../lib/index.js'
import { loadNative } from '../lib/native.js'

const skip = process.platform !== 'darwin' && 'macOS only'
const fixture = (name) => fileURLToPath(new URL(`./fixtures/${name}`, import.meta.url))
const libraryEntry = fileURLToPath(new URL('../lib/index.js', import.meta.url))

describe('system fonts', { skip }, () => {
  let families
  before(async () => {
    families = await getFonts()
  })
  const family = (name) => families.find((entry) => entry.family === name)

  test('Helvetica Neue has at least four faces', () => {
    assert.ok(family('Helvetica Neue').faces.length >= 4)
  })

  test('picks the Simplified Chinese name of PingFang SC', () => {
    assert.equal(pickName(family('PingFang SC').familyNames, ['zh-Hans']), '苹方-简')
  })

  test('Skia is variable, with coordinates and named instances', () => {
    const { faces } = family('Skia')
    assert.ok(faces.every((face) => face.variable))
    assert.ok(faces.every((face) => ['wght', 'wdth'].every((tag) => typeof face.coordinates[tag] === 'number')))
    const instanced = faces.filter((face) => face.namedInstance !== undefined)
    assert.ok(instanced.length > 0)
    for (const face of instanced) {
      // CoreText reports coordinates cut to four decimals.
      const instance = face.opentype.fvar.instances[face.namedInstance]
      for (const [tag, value] of Object.entries(instance.coordinates)) {
        assert.ok(Math.abs(face.coordinates[tag] - value) < 5e-4, `${face.postscriptName} ${tag}`)
      }
    }
  })
})

describe('registered test fonts', { skip }, () => {
  const { registerFont, unregisterFont } = loadNative().__testing
  const files = ['SFITest-Variable.ttf', 'SFITest-Collection.ttc', 'SFITest-Symbol.ttf'].map(fixture)
  const registered = []
  const testFamilies = async () => (await getFonts()).filter((entry) => entry.family.startsWith('SFITest'))

  before(() => {
    for (const file of files) {
      registerFont(file)
      registered.push(file)
    }
  })
  after(() => {
    for (const file of registered.splice(0)) unregisterFont(file)
  })

  test('appear with process scope, localized names, collection indices and the symbol flag', async () => {
    const found = Object.fromEntries((await testFamilies()).map((entry) => [entry.family, entry]))
    assert.deepEqual(Object.keys(found).sort(), ['SFITest Collection', 'SFITest Symbol', 'SFITest Variable'])
    const faces = Object.values(found).flatMap((entry) => entry.faces)
    assert.ok(faces.length > 0)
    assert.ok(faces.every((face) => face.scope === 'process'))

    const names = found['SFITest Variable'].familyNames
    assert.equal(pickName(names, ['zh-CN']), 'SFITest 变量')
    assert.equal(pickName(names, ['ja-JP']), 'SFITest バリアブル')
    assert.equal(pickName(names, ['ru-RU']), 'SFITest Переменный')
    assert.equal(pickName(names, ['ar-SA']), 'SFITest متغير')

    const byName = Object.fromEntries(found['SFITest Collection'].faces.map((face) => [face.postscriptName, face]))
    assert.equal(byName['SFITestCollection-Regular'].file.index, 0)
    assert.equal(byName['SFITestCollection-Bold'].file.index, 1)
    assert.equal(byName['SFITestCollection-Regular'].file.path, fixture('SFITest-Collection.ttc'))

    assert.deepEqual(
      found['SFITest Symbol'].faces.map((face) => face.symbol),
      [true]
    )
    assert.ok(found['SFITest Variable'].faces.every((face) => !face.symbol))
  })

  test('the named instances of the variable font share one font resource', async () => {
    const { faces } = (await testFamilies()).find((entry) => entry.family === 'SFITest Variable')
    assert.deepEqual(faces.map((face) => face.postscriptName).sort(), ['SFITestVariable-Bold', 'SFITestVariable-Regular', 'SFITestVariable-Thin'])
    assert.ok(faces.every((face) => face.opentype === faces[0].opentype && face.axes === faces[0].axes))
    assert.deepEqual(
      faces.map((face) => face.coordinates.wght).sort((a, b) => a - b),
      [100, 400, 700]
    )
  })

  test('are gone after they are unregistered', async () => {
    for (const file of registered.splice(0)) unregisterFont(file)
    assert.deepEqual(await testFamilies(), [])
  })
})

describe('watch()', { skip }, () => {
  test('returns an unsubscribe function that can be called twice', () => {
    const stop = watch(() => {})
    assert.equal(typeof stop, 'function')
    stop()
    stop()
  })

  test('can be started again after the last subscriber stopped', () => {
    watch(() => {})()
    const first = watch(() => {})
    const second = watch(() => {})
    first()
    second()
  })

  test('does not keep the process alive, subscribed or not', async () => {
    const run = promisify(execFile)
    for (const script of [`require(${JSON.stringify(libraryEntry)}).watch(() => {})`, `require(${JSON.stringify(libraryEntry)}).watch(() => {})()`]) {
      const { stderr } = await run(process.execPath, ['-e', script], { timeout: 20000 })
      assert.equal(stderr, '')
    }
  })
})
