// Type-level tests for ts/field-catalogue.ts. Nothing here runs: `npm run typecheck` compiles it, and a
// catalogue that disagrees with the public types is a compile error.

import type { FIELD_CATALOGUE } from '../../ts/field-catalogue.ts'
import type { Field, PresetFields } from '../../ts/types.ts'

type Equal<A, B> = (<T>() => T extends A ? 1 : 2) extends <T>() => T extends B ? 1 : 2 ? true : false
type Expect<T extends true> = T

type Entry = (typeof FIELD_CATALOGUE)[number]

/** The names of the entries whose smallest preset is one of `P`. */
type NamesIn<P extends string> = Extract<Entry, { preset: P }>['name']

// The catalogue names every field, and only fields.
export type CatalogueIsEveryField = Expect<Equal<Entry['name'], Field>>

// Each preset contains the fields whose smallest preset is it or an earlier one.
export type NamesPreset = Expect<Equal<NamesIn<'names'>, PresetFields['names']>>
export type StylesPreset = Expect<Equal<NamesIn<'names' | 'styles'>, PresetFields['styles']>>
export type DetailsPreset = Expect<Equal<NamesIn<'names' | 'styles' | 'details'>, PresetFields['details']>>
export type FullPreset = Expect<Equal<NamesIn<'names' | 'styles' | 'details' | 'full'>, PresetFields['full']>>

// No field is in a preset only by accident: what is in no preset is what the docs say is.
export type InNoPreset = Expect<Equal<Extract<Entry, { preset: null }>['name'], 'faces.coverage'>>
