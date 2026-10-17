// Reads the generated fonts in test/fixtures/ through the same table reader the
// platform backends use, and checks the values scripts/make-test-fonts.py puts
// in them. `readFontFile` returns the native view: language tags are as the
// reader mapped them, before the JS layer canonicalizes them.
import assert from 'node:assert/strict'
import { mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join } from 'node:path'
import { after, describe, test } from 'node:test'
import { fileURLToPath } from 'node:url'

import { loadNative } from '../dist/native.js'

const { readFontFile } = loadNative().__testing
const fixture = (name) => fileURLToPath(new URL(`./fixtures/${name}`, import.meta.url))

const variable = readFontFile(fixture('SFITest-Variable.ttf'), 0)
const collection = [0, 1].map((index) => readFontFile(fixture('SFITest-Collection.ttc'), index))
const symbol = readFontFile(fixture('SFITest-Symbol.ttf'), 0)
const legacy = readFontFile(fixture('SFITest-Legacy.ttf'), 0)

const records = (opentype, nameId) => opentype.names.filter((record) => record.nameId === nameId)
const windows = (opentype, nameId, languageId) =>
  opentype.names.find((record) => record.platformId === 3 && record.nameId === nameId && record.languageId === languageId)

describe('SFITest-Variable.ttf', () => {
  test('lists its tables in ascending order', () => {
    assert.deepEqual(variable.tables, [
      'GSUB',
      'OS/2',
      'STAT',
      'cmap',
      'fvar',
      'glyf',
      'gvar',
      'head',
      'hhea',
      'hmtx',
      'loca',
      'maxp',
      'meta',
      'name',
      'post'
    ])
    assert.equal(variable.errors, undefined)
  })

  test('keeps every name record, in table order, unmerged', () => {
    assert.equal(variable.names.length, 48)
    // Mac Roman records precede the Windows ones, as stored.
    assert.equal(variable.names[0].platformId, 1)
    assert.equal(variable.names.at(-1).platformId, 3)
    assert.equal(records(variable, 1).filter((record) => record.value === 'SFITest Variable').length, 2)
  })

  test('decodes the Windows names in zh-CN, ja-JP, ru-RU and ar-SA', () => {
    const expected = [
      [0x0804, 'zh-CN', 'SFITest 变量', 'SFITest 变量 Regular'],
      [0x0411, 'ja-JP', 'SFITest バリアブル', 'SFITest バリアブル Regular'],
      [0x0419, 'ru-RU', 'SFITest Переменный', 'SFITest Переменный Regular'],
      [0x0401, 'ar-SA', 'SFITest متغير', 'SFITest متغير Regular']
    ]
    for (const [languageId, language, family, full] of expected) {
      assert.deepEqual([windows(variable, 1, languageId)?.language, windows(variable, 1, languageId)?.value], [language, family])
      assert.deepEqual([windows(variable, 4, languageId)?.language, windows(variable, 4, languageId)?.value], [language, full])
    }
  })

  test('keeps a supplementary-plane character as one code point', () => {
    const record = records(variable, 10)[0]
    assert.equal(record.platformId, 3)
    assert.equal(record.encodingId, 10)
    assert.equal(record.value, 'SFITest Variable \u{1F600}')
  })

  test('resolves the language of the format 1 language-tag record', () => {
    const record = windows(variable, 1, 0x8000)
    assert.equal(record.language, 'gsw')
    assert.equal(record.value, 'SFITest Alemannisch')
  })

  test('decodes the Mac Chinese Simplified record, or keeps its bytes', {
    skip: process.platform !== 'darwin' && 'only the macOS decoder is certain to know the encoding'
  }, () => {
    const mac = variable.names.filter((record) => record.platformId === 1 && record.encodingId === 25)
    assert.deepEqual(
      mac.map((record) => [record.nameId, record.languageId, record.language, record.value]),
      [
        [1, 33, 'zh-Hans', 'SFITest 变量'],
        [4, 33, 'zh-Hans', 'SFITest 变量 Regular'],
        [256, 33, 'zh-Hans', '字重'],
        [260, 33, 'zh-Hans', '粗体']
      ]
    )
    assert.ok(mac.every((record) => record.bytes === undefined))
  })

  test('reads head', () => {
    assert.deepEqual(variable.head, {
      version: 1,
      fontRevision: 1,
      flags: 3,
      unitsPerEm: 1000,
      created: '2020-01-02T03:04:05Z',
      modified: '2021-06-07T08:09:10Z',
      boundingBox: [50, 0, 700, 700],
      macStyle: 0,
      lowestRecPPEM: 3,
      fontDirectionHint: 2,
      indexToLocFormat: 0,
      glyphDataFormat: 0
    })
  })

  test('reads fvar axes, instances and instance PostScript names', () => {
    const { axes, instances } = variable.fvar
    assert.equal(axes.length, 1)
    assert.deepEqual(
      { ...axes[0], names: undefined },
      { tag: 'wght', min: 100, default: 400, max: 900, flags: 0, nameId: 256, names: undefined }
    )
    assert.deepEqual(
      axes[0].names.map((name) => [name.language, name.value]),
      [
        ['en', 'Weight'],
        ['zh-Hans', '字重'],
        ['en-US', 'Weight'],
        ['zh-CN', '字重']
      ]
    )
    assert.deepEqual(
      instances.map((instance) => [instance.coordinates, instance.subfamilyNameId, instance.postscriptNameId, instance.postscriptName]),
      [
        [{ wght: 100 }, 257, 258, 'SFITestVariable-Thin'],
        [{ wght: 400 }, 2, 259, 'SFITestVariable-Regular'],
        [{ wght: 700 }, 260, 261, 'SFITestVariable-Bold']
      ]
    )
    const bold = instances[2].subfamilyNames
    assert.deepEqual(
      bold.filter((name) => name.language.startsWith('zh')).map((name) => name.value),
      ['粗体', '粗体']
    )
    assert.ok(bold.some((name) => name.language === 'en-US' && name.value === 'Bold'))
  })

  test('reads meta', () => {
    assert.deepEqual(variable.meta, { dlng: ['Latn'], slng: ['Latn', 'Cyrl', 'Arab', 'Hans'], tags: ['dlng', 'slng'] })
  })

  test('reads the GSUB scripts, language systems, features and feature names', () => {
    const { gsub } = variable.layout
    assert.equal(variable.layout.gpos, undefined)
    assert.deepEqual(gsub.scripts, {
      DFLT: { dflt: ['liga', 'ss01'] },
      latn: { dflt: ['liga', 'ss01'], 'TRK ': ['locl', 'liga', 'ss01'] }
    })
    assert.deepEqual(gsub.features, ['liga', 'locl', 'ss01'])
    assert.deepEqual(gsub.featureNames, {
      ss01: [
        { language: 'en-US', value: 'Alternate B' },
        { language: 'zh-CN', value: '备选 B' }
      ]
    })
  })

  test('reads OS/2 version 5 with optical sizes in points', () => {
    const { os2 } = variable
    assert.equal(os2.version, 5)
    assert.equal(os2.weightClass, 400)
    assert.equal(os2.vendorId, 'SFI ')
    assert.deepEqual([os2.xHeight, os2.capHeight], [500, 700])
    assert.deepEqual([os2.lowerOpticalPointSize, os2.upperOpticalPointSize], [8, 72])
    assert.deepEqual(os2.codePageRanges, [1, 0])
  })

  test('lists the cmap subtables', () => {
    assert.deepEqual(variable.cmap.subtables, [
      { platformId: 0, encodingId: 3, format: 4 },
      { platformId: 3, encodingId: 1, format: 4 },
      { platformId: 3, encodingId: 10, format: 12 }
    ])
  })
})

describe('SFITest-Collection.ttc', () => {
  test('has two faces with their own tables', () => {
    assert.ok(collection[0].tables.includes('glyf'))
    assert.ok(!collection[0].tables.includes('CFF '))
    assert.ok(collection[1].tables.includes('CFF '))
    assert.ok(!collection[1].tables.includes('glyf'))
  })

  test('reads each face name table separately', () => {
    const postscriptNames = collection.map((face) => windows(face, 6, 0x0409).value)
    assert.deepEqual(postscriptNames, ['SFITestCollection-Regular', 'SFITestCollection-Bold'])
    const styles = collection.map((face) => windows(face, 2, 0x0409).value)
    assert.deepEqual(styles, ['Regular', 'Bold'])
  })

  test('reads each face OS/2 and maxp', () => {
    assert.deepEqual(
      collection.map((face) => face.os2.weightClass),
      [400, 700]
    )
    assert.deepEqual(
      collection.map((face) => face.maxp.version),
      [1, 0.5]
    )
    assert.ok(collection.every((face) => face.maxp.numGlyphs === 8))
  })

  test('rejects a face index beyond the collection', () => {
    assert.throws(() => readFontFile(fixture('SFITest-Collection.ttc'), 2), RangeError)
  })
})

describe('SFITest-Symbol.ttf', () => {
  test('has only a (3,0) cmap subtable', () => {
    assert.deepEqual(symbol.cmap.subtables, [{ platformId: 3, encodingId: 0, format: 4 }])
  })

  test('declares the symbol character set in OS/2', () => {
    assert.equal(symbol.os2.codePageRanges[0], 2 ** 31)
    assert.deepEqual([symbol.os2.firstCharIndex, symbol.os2.lastCharIndex], [0xf041, 0xf042])
  })
})

describe('SFITest-Legacy.ttf', () => {
  const idsOn = (platformId, nameIds) =>
    legacy.names.filter((record) => record.platformId === platformId && nameIds.includes(record.nameId))

  test('has typographic names on Windows and legacy names on the Macintosh platform only', () => {
    assert.deepEqual(
      idsOn(3, [16, 17]).map((record) => [record.nameId, record.language, record.value]),
      [
        [16, 'en-US', 'SFITest Legacy'],
        [17, 'en-US', 'Book']
      ]
    )
    assert.deepEqual(idsOn(3, [1, 2]), [])
    assert.deepEqual(idsOn(1, [16, 17]), [])
    assert.deepEqual(
      idsOn(1, [1, 2]).map((record) => [record.nameId, record.language]),
      [
        [1, 'en'],
        [2, 'en'],
        [1, 'is'],
        [2, 'is'],
        [1, 'tr'],
        [2, 'tr']
      ]
    )
  })

  // The same bytes mean other characters in the Turkish and Icelandic Mac Roman
  // variants than in plain Mac Roman (0xDA is Ğ in Mac Turkish, the fraction slash in Mac Roman).
  test('decodes the Mac Roman variants of Turkish and Icelandic', {
    skip: process.platform !== 'darwin' && 'only the macOS decoder is certain to know the encodings'
  }, () => {
    const value = (nameId, language) => legacy.names.find((r) => r.platformId === 1 && r.nameId === nameId && r.language === language)
    assert.equal(value(1, 'tr').value, 'Yazı Tipi Ğğ Şş İ')
    assert.equal(value(2, 'tr').value, 'Düz')
    assert.equal(value(1, 'is').value, 'Letur Þþ Ðð')
    assert.equal(value(2, 'is').value, 'Venjulegt')
    assert.ok(legacy.names.every((record) => record.bytes === undefined))
  })
})

describe('opentype.errors', () => {
  const directory = mkdtempSync(join(tmpdir(), 'sfi-corrupt-'))
  after(() => rmSync(directory, { recursive: true, force: true }))

  /** Copies SFITest-Variable.ttf with the directory entry or the data of some tables damaged. */
  function corruptedCopy(damage) {
    const bytes = Buffer.from(readFileSync(fixture('SFITest-Variable.ttf')))
    const tableCount = bytes.readUInt16BE(4)
    for (let i = 0; i < tableCount; i++) {
      const record = 12 + i * 16
      const tag = bytes.toString('latin1', record, record + 4)
      const offset = bytes.readUInt32BE(record + 8)
      if (damage[tag]) damage[tag]({ bytes, record, offset })
    }
    const file = join(directory, `corrupt-${Object.keys(damage).join('-').replaceAll('/', '')}.ttf`)
    writeFileSync(file, bytes)
    return readFontFile(file, 0)
  }

  const damage = {
    // An unknown table format.
    name: ({ bytes, offset }) => bytes.writeUInt16BE(7, offset),
    // A table too short for its fixed fields.
    head: ({ bytes, record }) => bytes.writeUInt32BE(10, record + 12),
    // A table that starts beyond the end of the file and cannot be fetched.
    'OS/2': ({ bytes, record }) => bytes.writeUInt32BE(0x7ffffff0, record + 8)
  }

  test('lists a table that is present but cannot be parsed or fetched, and leaves it out', () => {
    const font = corruptedCopy(damage)
    assert.deepEqual(font.errors, ['OS/2', 'head', 'name'])
    assert.equal(font.head, undefined)
    assert.equal(font.os2, undefined)
    // The other tables are still read, and every table stays in the list.
    assert.ok(font.tables.includes('name') && font.tables.includes('head') && font.tables.includes('OS/2'))
    assert.equal(font.maxp.numGlyphs, 8)
    assert.equal(font.cmap.subtables.length, 3)
  })

  test('a malformed name table gives empty names, also in fvar', () => {
    const font = corruptedCopy({ name: damage.name })
    assert.deepEqual(font.errors, ['name'])
    assert.deepEqual(font.names, [])
    assert.deepEqual(font.fvar.axes[0].names, [])
    assert.deepEqual(
      font.fvar.instances.map((instance) => instance.subfamilyNames),
      [[], [], []]
    )
    assert.equal(font.fvar.instances[0].postscriptName, undefined)
    assert.equal(font.head.unitsPerEm, 1000)
  })
})
