// Validates the real getFamilies() result of the machine the tests run on against
// the public types in ts/types.ts, and the invariants docs/api.md documents.
// The validator is written by hand from those types: a schema generated from
// them would share their mistakes.
import assert from 'node:assert/strict'
import { before, describe, test } from 'node:test'

import { getFamilies } from '../dist/index.js'
import { loadNative } from '../dist/native.js'

// ---- a small schema language -------------------------------------------------
// A schema is a function (value, path, errors) that pushes a message for every
// violation. Objects are strict: a key the schema does not name is a violation.

const isObject = (value) => typeof value === 'object' && value !== null && !Array.isArray(value)

const str = (value, path, errors) => {
  if (typeof value !== 'string') errors.push(`${path}: expected a string, got ${describe_(value)}`)
}
const bool = (value, path, errors) => {
  if (typeof value !== 'boolean') errors.push(`${path}: expected a boolean, got ${describe_(value)}`)
}
const num = (value, path, errors) => {
  if (typeof value !== 'number' || !Number.isFinite(value)) errors.push(`${path}: expected a finite number, got ${describe_(value)}`)
}
const int = (value, path, errors) => {
  if (!Number.isInteger(value)) errors.push(`${path}: expected an integer, got ${describe_(value)}`)
}
const uint = (value, path, errors) => {
  if (!Number.isInteger(value) || value < 0) errors.push(`${path}: expected a non-negative integer, got ${describe_(value)}`)
}
const literal =
  (...allowed) =>
  (value, path, errors) => {
    if (!allowed.includes(value)) errors.push(`${path}: expected one of ${allowed.join(' | ')}, got ${describe_(value)}`)
  }
const arrayOf = (item) => (value, path, errors) => {
  if (!Array.isArray(value)) return errors.push(`${path}: expected an array, got ${describe_(value)}`)
  value.forEach((entry, i) => {
    item(entry, `${path}[${i}]`, errors)
  })
}
const tuple = (length, item) => (value, path, errors) => {
  if (!Array.isArray(value) || value.length !== length)
    return errors.push(`${path}: expected an array of ${length}, got ${describe_(value)}`)
  value.forEach((entry, i) => {
    item(entry, `${path}[${i}]`, errors)
  })
}
const recordOf = (item) => (value, path, errors) => {
  if (!isObject(value)) return errors.push(`${path}: expected an object, got ${describe_(value)}`)
  for (const [key, entry] of Object.entries(value)) item(entry, `${path}.${key}`, errors)
}
const shape =
  (required, optional = {}) =>
  (value, path, errors) => {
    if (!isObject(value)) return errors.push(`${path}: expected an object, got ${describe_(value)}`)
    for (const [key, schema] of Object.entries(required)) {
      if (!(key in value)) errors.push(`${path}.${key}: missing`)
      else schema(value[key], `${path}.${key}`, errors)
    }
    for (const [key, schema] of Object.entries(optional)) {
      if (key in value) {
        // An optional field is absent, never present with undefined.
        if (value[key] === undefined) errors.push(`${path}.${key}: present with undefined`)
        else schema(value[key], `${path}.${key}`, errors)
      }
    }
    for (const key of Object.keys(value)) {
      if (!(key in required) && !(key in optional)) errors.push(`${path}.${key}: not in the type`)
    }
  }
const check = (predicate, message) => (value, path, errors) => {
  if (!predicate(value)) errors.push(`${path}: ${message}, got ${describe_(value)}`)
}
const all =
  (...schemas) =>
  (value, path, errors) => {
    for (const schema of schemas) {
      const before = errors.length
      schema(value, path, errors)
      if (errors.length > before) return
    }
  }

function describe_(value) {
  const text = JSON.stringify(value)
  return text === undefined ? String(value) : text.length > 80 ? `${text.slice(0, 80)}...` : text
}

// ---- the types of ts/types.ts ----------------------------------------------------

/** A language tag is canonical, or kept as written when Intl rejects it. */
function languageTag(value, path, errors) {
  if (typeof value !== 'string') return errors.push(`${path}: expected a string, got ${describe_(value)}`)
  if (value === '') return
  let canonical
  try {
    canonical = Intl.getCanonicalLocales(value)[0]
  } catch {
    return
  }
  if (canonical !== value) errors.push(`${path}: ${JSON.stringify(value)} is not canonical (${canonical})`)
}

const localizedString = shape({ language: languageTag, value: str })
const localizedStrings = arrayOf(localizedString)

const tag4 = check((value) => typeof value === 'string' && [...value].length === 4, 'expected a four-character tag')
const hex = check((value) => typeof value === 'string' && /^(?:[0-9a-fA-F]{2})+$/.test(value), 'expected hex of whole bytes')
const ascending = (value, path, errors) => {
  if (Array.isArray(value) && value.some((entry, i) => i > 0 && !(value[i - 1] < entry))) errors.push(`${path}: not strictly ascending`)
}
const isoDate = check((value) => {
  const match = typeof value === 'string' && /^(\d{4})-\d\d-\d\dT\d\d:\d\d:\d\dZ$/.exec(value)
  return match && Number(match[1]) >= 1 && !Number.isNaN(Date.parse(value))
}, 'expected an ISO 8601 UTC date in years 0001-9999')

const nameRecord = all(
  shape({ platformId: uint, encodingId: uint, languageId: uint, nameId: uint, language: languageTag }, { value: str, bytes: hex }),
  check((record) => 'value' in record !== 'bytes' in record, 'expected exactly one of value and bytes')
)

const horizontalHeader = shape({
  version: num,
  ascender: int,
  descender: int,
  lineGap: int,
  advanceMax: uint,
  minLeadingBearing: int,
  minTrailingBearing: int,
  maxExtent: int,
  caretSlopeRise: int,
  caretSlopeRun: int,
  caretOffset: int,
  metricDataFormat: int,
  numberOfMetrics: uint
})

const subscript = shape({ xSize: int, ySize: int, xOffset: int, yOffset: int })
const os2 = all(
  shape(
    {
      version: uint,
      xAvgCharWidth: int,
      weightClass: uint,
      widthClass: uint,
      fsType: uint,
      subscript,
      superscript: subscript,
      strikeout: shape({ size: int, position: int }),
      familyClass: int,
      panose: tuple(10, uint),
      unicodeRanges: tuple(4, uint),
      vendorId: str,
      fsSelection: uint,
      firstCharIndex: uint,
      lastCharIndex: uint,
      typoAscender: int,
      typoDescender: int,
      typoLineGap: int,
      winAscent: uint,
      winDescent: uint
    },
    {
      codePageRanges: tuple(2, uint),
      xHeight: int,
      capHeight: int,
      defaultChar: uint,
      breakChar: uint,
      maxContext: uint,
      lowerOpticalPointSize: num,
      upperOpticalPointSize: num
    }
  ),
  // A field group of a newer table version is present only in a table of that version.
  check((table) => {
    const needs = {
      codePageRanges: 1,
      xHeight: 2,
      capHeight: 2,
      defaultChar: 2,
      breakChar: 2,
      maxContext: 2,
      lowerOpticalPointSize: 5,
      upperOpticalPointSize: 5
    }
    return Object.entries(needs).every(([key, version]) => !(key in table) || table.version >= version)
  }, 'expected fields only of tables at least as new as their group')
)

const layoutTable = shape(
  { scripts: recordOf(recordOf(arrayOf(tag4))), features: arrayOf(tag4) },
  { featureNames: recordOf(localizedStrings) }
)

const openTypeInfo = shape(
  {
    tables: all(arrayOf(tag4), ascending),
    names: arrayOf(nameRecord)
  },
  {
    errors: all(arrayOf(tag4), ascending),
    head: shape(
      {
        version: num,
        fontRevision: num,
        flags: uint,
        unitsPerEm: uint,
        boundingBox: tuple(4, int),
        macStyle: uint,
        lowestRecPPEM: uint,
        fontDirectionHint: int,
        indexToLocFormat: int,
        glyphDataFormat: int
      },
      { created: isoDate, modified: isoDate }
    ),
    hhea: horizontalHeader,
    vhea: horizontalHeader,
    os2,
    post: shape({ version: num, italicAngle: num, underlinePosition: int, underlineThickness: int, isFixedPitch: bool }),
    maxp: shape({ version: num, numGlyphs: uint }),
    cmap: shape({ subtables: arrayOf(shape({ platformId: uint, encodingId: uint, format: uint })) }),
    fvar: shape({
      axes: arrayOf(shape({ tag: tag4, min: num, default: num, max: num, flags: uint, nameId: uint, names: localizedStrings })),
      instances: arrayOf(
        shape(
          { subfamilyNameId: uint, subfamilyNames: localizedStrings, flags: uint, coordinates: recordOf(num) },
          { postscriptNameId: uint, postscriptName: str }
        )
      )
    }),
    meta: shape({ tags: arrayOf(tag4) }, { dlng: arrayOf(str), slng: arrayOf(str) }),
    layout: shape({}, { gsub: layoutTable, gpos: layoutTable })
  }
)

const variationAxis = shape({ tag: tag4, name: str, names: localizedStrings, min: num, max: num, default: num, hidden: bool })

const darwinNative = shape(
  { os: literal('darwin') },
  {
    weight: num,
    width: num,
    slant: num,
    symbolicTraits: uint,
    format: uint,
    priority: uint,
    registrationScope: uint,
    downloadable: bool,
    downloaded: bool,
    styleName: str,
    displayName: str,
    localizedFamilyName: str,
    localizedStyleName: str,
    fixedAdvance: num,
    features: arrayOf(
      shape(
        {
          id: int,
          name: str,
          exclusive: bool,
          selectors: arrayOf(shape({ id: int, name: str, default: bool }, { openTypeTag: str, openTypeValue: int }))
        },
        { openTypeTag: str }
      )
    ),
    metrics: shape({
      ascent: num,
      descent: num,
      leading: num,
      capHeight: num,
      xHeight: num,
      slantAngle: num,
      underlinePosition: num,
      underlineThickness: num,
      unitsPerEm: num,
      glyphCount: num,
      boundingBox: tuple(4, num)
    }),
    shadowed: arrayOf(shape({ path: str, variable: bool }))
  }
)
const win32Native = shape(
  {
    os: literal('win32'),
    weight: num,
    width: num,
    slant: num,
    format: uint,
    typographicFamilyNames: localizedStrings,
    typographicStyleNames: localizedStrings,
    designLanguages: localizedStrings,
    supportedLanguages: localizedStrings,
    inactiveFamilyPreference: bool,
    additionalInformationalStrings: recordOf(localizedStrings)
  },
  {
    sourceType: uint,
    sourceName: str,
    axisValues: arrayOf(shape({ tag: tag4, value: num })),
    resourceAxes: arrayOf(shape({ tag: tag4, min: num, max: num, default: num, attributes: uint, names: localizedStrings }))
  }
)
const linuxNative = shape(
  { os: literal('linux'), weight: num, width: num, slant: num, index: uint },
  {
    spacing: uint,
    format: str,
    fontWrapper: str,
    foundry: str,
    decorative: bool,
    fontHasHint: bool,
    order: uint,
    outline: bool,
    scalable: bool,
    antialias: bool,
    pixelSize: num
  }
)

const fontFace = shape(
  {
    postscriptName: str,
    fullName: str,
    fullNames: localizedStrings,
    styleName: str,
    styleNames: localizedStrings,
    familyNames: localizedStrings,
    weight: num,
    width: num,
    style: literal('normal', 'italic', 'oblique'),
    monospace: bool,
    color: bool,
    symbol: bool,
    enabled: bool,
    simulations: shape({ bold: bool, oblique: bool }),
    supportedLanguages: arrayOf(languageTag),
    scope: literal('system', 'user', 'process', 'unknown'),
    format: literal('truetype', 'cff', 'cff2', 'type1', 'bitmap', 'unknown'),
    variable: bool,
    platform: (value, path, errors) => {
      const schema = { darwin: darwinNative, win32: win32Native, linux: linuxNative }[value?.os]
      if (!schema) errors.push(`${path}.os: expected darwin | win32 | linux, got ${describe_(value?.os)}`)
      else schema(value, path, errors)
    }
  },
  {
    file: shape({ path: str }, { index: uint }),
    axes: arrayOf(variationAxis),
    coordinates: recordOf(num),
    namedInstanceIndex: uint,
    opentype: openTypeInfo,
    coverage: arrayOf(tuple(2, uint))
  }
)

const languageCoverage = shape({ language: languageTag, orthography: str, required: uint, covered: uint })

const fontFamily = shape(
  {
    family: str,
    familyNames: localizedStrings,
    supportedLanguages: arrayOf(languageTag),
    variable: bool,
    enabled: bool,
    faces: arrayOf(fontFace)
  },
  { languageCoverage: arrayOf(languageCoverage) }
)

// ---- invariants the documentation states ----------------------------------------

/** en-US, else any en / en-*, else language "", else the first, else "". */
function defaultString(strings) {
  return (
    (
      strings.find((s) => s.language === 'en-US') ??
      strings.find((s) => s.language === 'en' || s.language.startsWith('en-')) ??
      strings.find((s) => s.language === '') ??
      strings[0]
    )?.value ?? ''
  )
}

function mergeLocalized(strings) {
  const seen = new Set()
  return strings.filter((s) => {
    const key = `${s.language}\u0000${s.value}`
    if (seen.has(key)) return false
    seen.add(key)
    return true
  })
}

/**
 * The identity of a face: (file.path, file.index, coordinates, simulations). PostScript names are not part of it:
 * they repeat on Windows. Absent parts are null, so two different faces never produce the same key.
 */
function faceIdentity(face) {
  const coordinates =
    face.coordinates && Object.fromEntries(Object.entries(face.coordinates).sort(([a], [b]) => (a < b ? -1 : a > b ? 1 : 0)))
  return JSON.stringify([
    face.file?.path ?? null,
    face.file?.index ?? null,
    coordinates ?? null,
    face.simulations.bold,
    face.simulations.oblique
  ])
}

/**
 * Everything that can be said about a result without knowing which fonts the machine has.
 * `coverage`: `faces.coverage` was selected. `languageCoverage`: the tags the option was given, or undefined.
 */
function validateResult(families, { coverage, languageCoverage }) {
  const errors = []
  arrayOf(fontFamily)(families, 'families', errors)
  assert.deepEqual(errors.slice(0, 20), [], `${errors.length} shape violations`)

  const resources = new Map()
  const seenFamilies = new Set()
  let previous
  families.forEach((family, f) => {
    const where = `families[${f}] (${family.family})`
    const problems = []
    const expect = (condition, message) => condition || problems.push(`${where}: ${message}`)

    expect(family.faces.length > 0, 'has no faces')

    // `family` is unique among the families, always the value of one entry of familyNames, and the families are sorted by it.
    expect(!seenFamilies.has(family.family), 'family repeats an earlier family')
    seenFamilies.add(family.family)
    expect(
      family.familyNames.some((name) => name.value === family.family),
      'family is not the value of an entry of familyNames'
    )
    if (previous !== undefined) {
      const lower = [previous.toLowerCase(), family.family.toLowerCase()]
      expect(lower[0] < lower[1] || (lower[0] === lower[1] && previous < family.family), `sorted after ${previous}`)
    }
    previous = family.family

    // Aggregates are derived from the faces.
    const names = mergeLocalized(family.faces.flatMap((face) => face.familyNames))
    expect(JSON.stringify(family.familyNames) === JSON.stringify(names), 'familyNames is not the union of the faces')
    expect(
      JSON.stringify(family.supportedLanguages) === JSON.stringify([...new Set(family.faces.flatMap((face) => face.supportedLanguages))]),
      'supportedLanguages is not the union of the faces'
    )
    expect(family.variable === family.faces.some((face) => face.variable), 'variable is not any-of the faces')
    expect(family.enabled === family.faces.some((face) => face.enabled), 'enabled is not any-of the faces')

    if (languageCoverage === undefined) expect(family.languageCoverage === undefined, 'languageCoverage was not requested')
    else {
      const entries = family.languageCoverage
      expect(entries?.length === languageCoverage.length, 'languageCoverage has not one entry per requested tag')
      languageCoverage.forEach((tag, i) => {
        const entry = entries?.[i]
        expect(entry?.language === Intl.getCanonicalLocales(tag)[0], `languageCoverage[${i}].language is not the canonical ${tag}`)
        expect(
          entry?.required === orthographies[entry?.orthography],
          `languageCoverage[${i}].required is not the size of ${entry?.orthography}`
        )
        expect(entry?.covered <= entry?.required, `languageCoverage[${i}] covers more than it requires`)
      })
    }

    // Face identity is unique within a family.
    const identities = new Map()
    family.faces.forEach((face, i) => {
      const at = `${where} face ${i} (${face.postscriptName})`
      const fail = (message) => problems.push(`${at}: ${message}`)

      if (face.platform.os !== process.platform) fail(`platform.os is ${face.platform.os}`)
      const identity = faceIdentity(face)
      if (identities.has(identity)) fail(`(file.path, file.index, coordinates, simulations) repeats ${identities.get(identity)}`)
      identities.set(identity, at)

      if (!(face.weight >= 1 && face.weight <= 1000)) fail(`weight ${face.weight} is outside 1-1000`)
      if (!(face.width >= 50 && face.width <= 200)) fail(`width ${face.width} is outside 50-200`)
      if (face.fullName !== defaultString(face.fullNames)) fail('fullName is not the default of fullNames')
      if (face.styleName !== defaultString(face.styleNames)) fail('styleName is not the default of styleNames')

      // Variations: axes and coordinates exist together with `variable`; namedInstanceIndex points into opentype.fvar.instances.
      if (face.variable !== (face.axes !== undefined)) fail('axes is present iff variable')
      if (face.variable !== (face.coordinates !== undefined)) fail('coordinates is present iff variable')
      if (face.variable) {
        if (face.axes.length === 0) fail('variable with no axes')
        if (JSON.stringify(Object.keys(face.coordinates)) !== JSON.stringify(face.axes.map((axis) => axis.tag))) {
          fail('coordinates do not cover the axes in order')
        }
        for (const axis of face.axes) {
          const value = face.coordinates[axis.tag]
          if (!(axis.min <= value && value <= axis.max)) fail(`${axis.tag} coordinate ${value} is outside ${axis.min}-${axis.max}`)
          if (!(axis.min <= axis.default && axis.default <= axis.max)) fail(`${axis.tag} default is outside its range`)
          if (axis.name !== defaultString(axis.names)) fail(`${axis.tag} name is not the default of names`)
        }
      }
      if (face.namedInstanceIndex !== undefined) {
        const instances = face.opentype?.fvar?.instances
        if (!face.variable) fail('namedInstanceIndex on a face that is not variable')
        else if (instances && !(face.namedInstanceIndex < instances.length)) fail('namedInstanceIndex is beyond opentype.fvar.instances')
      }

      // Faces of one font resource (one face of one file) share their objects.
      if (face.file?.index !== undefined) {
        const key = `${face.file.path}\u0000${face.file.index}`
        const first = resources.get(key)
        if (!first) resources.set(key, { face, at })
        else {
          if (first.face.opentype !== face.opentype) fail(`opentype is not the same object as ${first.at}`)
          if (first.face.axes !== face.axes) fail(`axes is not the same object as ${first.at}`)
        }
      }

      if (coverage) {
        if (!face.coverage) fail('no coverage')
        else {
          face.coverage.forEach(([start, end], r) => {
            if (!(start <= end && end <= 0x10ffff)) fail(`coverage range ${start}-${end} is invalid`)
            if (r > 0 && !(face.coverage[r - 1][1] + 1 < start))
              fail(`coverage range ${start}-${end} overlaps or touches ${face.coverage[r - 1]}`)
          })
        }
      } else if (face.coverage !== undefined) fail('coverage was not selected')
    })
    assert.deepEqual(problems.slice(0, 10), [], `${problems.length} invariant violations`)
  })
}

// ---- tests -----------------------------------------------------------------------

const { orthographies } = loadNative()

describe("getFamilies({ preset: 'full' })", () => {
  let families
  before(async () => {
    families = await getFamilies({ preset: 'full' })
  })

  test('lists fonts', () => {
    assert.ok(families.length > 0)
  })

  test('every family and face has the documented shape and invariants', () => {
    validateResult(families, { coverage: false })
  })
})

test("getFamilies({ preset: 'full', fields: ['faces.coverage'] }) adds ascending, non-overlapping ranges", async () => {
  validateResult(await getFamilies({ preset: 'full', fields: ['faces.coverage'] }), { coverage: true })
})

test("getFamilies({ preset: 'full', languageCoverage }) adds one entry per tag, in request order", async () => {
  const languageCoverage = ['zh-CN', 'ja', 'en-US']
  validateResult(await getFamilies({ preset: 'full', languageCoverage }), { coverage: false, languageCoverage })
})
