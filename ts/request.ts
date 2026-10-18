import { FIELD_CATALOGUE, type Platform } from './field-catalogue'
import { closureOf, PRESETS, presetFields } from './field-closure'
import type { Field, Preset } from './types'

// Validates the options of getFamilies / getFamily and turns them into what a call fetches. It never touches the
// native addon; every error here is a TypeError, and the RangeError for an unresolvable `languageCoverage` tag is
// raised later (language-coverage.ts), because resolving needs the addon's orthography names.

const FIELD_NAMES: ReadonlySet<string> = new Set(FIELD_CATALOGUE.map((spec) => spec.name))

/** What a validated call needs. */
export interface BuiltRequest {
  /** The fields the caller asked for: `fields` plus the preset's fields. */
  selected: ReadonlySet<Field>
  /** The fields to fetch on this platform (`closureOf`): the selection, the face identity, the inputs of derived fields. */
  closure: ReadonlySet<Field>
  /** The selected fields this platform computes itself instead of fetching. */
  derived: ReadonlySet<Field>
  /** From `options.families`; `undefined` when omitted, and for getFamily, whose caller supplies the family. */
  families?: readonly string[]
  /** From `options.languageCoverage`; `undefined` when omitted. */
  languageCoverage?: readonly string[]
}

/** The platform this process runs on, in the catalogue's terms. */
export function currentPlatform(): Platform {
  const platform = process.platform
  if (platform === 'darwin' || platform === 'win32' || platform === 'linux') return platform
  throw new Error(`system-font-info does not support the platform ${platform}`)
}

const isStringArray = (value: unknown): value is readonly string[] =>
  Array.isArray(value) && value.every((item) => typeof item === 'string')

const isPreset = (value: unknown): value is Preset => PRESETS.includes(value as Preset)

/**
 * Validates `options` as docs/api.md's error table lists and computes the selection and its closure.
 * `getFamily` makes `options.families` an error: the family is the call's own argument.
 */
export function buildRequest(
  options: unknown,
  { getFamily, platform = currentPlatform() }: { getFamily: boolean; platform?: Platform }
): BuiltRequest {
  if (typeof options !== 'object' || options === null) throw new TypeError('options must be an object')
  const { fields, preset, families, languageCoverage } = options as Record<string, unknown>

  if (getFamily && families !== undefined) throw new TypeError('getFamily does not take the families option')
  if (fields !== undefined) {
    if (!isStringArray(fields)) throw new TypeError('fields must be an array of strings')
    for (const field of fields) if (!FIELD_NAMES.has(field)) throw new TypeError(`"${field}" is not a field name`)
  }
  if (preset !== undefined && !isPreset(preset))
    throw new TypeError(`preset must be one of ${PRESETS.map((name) => `"${name}"`).join(', ')}`)
  if (fields === undefined && preset === undefined) throw new TypeError('options must give fields, preset or both')
  if (families !== undefined && !isStringArray(families)) throw new TypeError('families must be an array of strings')
  if (languageCoverage !== undefined && !isStringArray(languageCoverage))
    throw new TypeError('languageCoverage must be an array of strings')

  const selected = new Set<Field>([...((fields ?? []) as Field[]), ...(preset === undefined ? [] : presetFields(preset as Preset))])
  const closure = closureOf(selected, platform)
  return {
    selected,
    closure,
    derived: new Set([...selected].filter((field) => !closure.has(field))),
    families: families as readonly string[] | undefined,
    languageCoverage: languageCoverage as readonly string[] | undefined
  }
}
