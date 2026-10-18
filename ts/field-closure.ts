// What a selection of fields makes a platform fetch, computed from the field catalogue. It is the one home of the
// request closure rules: the JS layer builds the native request from `closureOf`, and `scripts/gen-fields.mjs` uses
// the same functions to generate the "groups each preset reaches" table of docs/platforms.md.

import { FIELD_CATALOGUE, type FieldSpec, type Group, type Platform } from './field-catalogue'
import type { Field } from './types'

const SPECS: readonly FieldSpec[] = FIELD_CATALOGUE
const SPEC_BY_NAME: ReadonlyMap<Field, FieldSpec> = new Map(SPECS.map((spec) => [spec.name, spec]))

const specOf = (field: Field): FieldSpec => {
  const spec = SPEC_BY_NAME.get(field)
  if (spec === undefined) throw new Error(`${field} is not in the field catalogue`)
  return spec
}

/** The fields `platform` computes from other fields, and which. `undefined` when the platform fetches `field` itself. */
const derivationOf = (field: Field, platform: Platform): readonly Field[] | undefined => {
  const source = specOf(field).sources[platform]
  return typeof source === 'object' && source !== null && 'derivedFrom' in source ? source.derivedFrom : undefined
}

/**
 * The fields a call has to fetch on `platform` for the selected ones:
 *
 * - the selected fields;
 * - the identity fields (`faces.file`, `faces.coordinates`, `faces.simulations`) when the caller selected a face-level
 *   field (`faces.*`, opentype and platform entries included). A face field fetched only to compute a family field is
 *   not selected, so it brings no identity;
 * - the inputs of every field the platform derives, and theirs in turn;
 *
 * without the fields the platform derives: they are computed here, never fetched.
 */
export function closureOf(selected: Iterable<Field>, platform: Platform): Set<Field> {
  const wanted = new Set(selected)
  const closure = new Set(wanted)
  if ([...wanted].some((field) => specOf(field).level !== 'family')) {
    for (const spec of SPECS) if (spec.identity) closure.add(spec.name)
  }
  for (const field of closure) {
    // A set visits what is added to it while it is iterated, which makes this the transitive closure.
    for (const input of derivationOf(field, platform) ?? []) closure.add(input)
  }
  for (const field of closure) if (derivationOf(field, platform) !== undefined) closure.delete(field)
  return closure
}

/** A group a field needs only in some cases, and the cases. */
export interface ConditionalGroup {
  readonly group: Group
  readonly for: string
}

export interface GroupsReached {
  /** The groups some field always needs; includes A, since every call lists the families. */
  readonly always: readonly Group[]
  /** The groups some field needs only in some cases, without those already in `always`. */
  readonly conditional: readonly ConditionalGroup[]
}

/** The groups of OS work that fetching `closure` (from `closureOf`) costs on `platform`. */
export function groupsOf(closure: Iterable<Field>, platform: Platform): GroupsReached {
  const always = new Set<Group>(['A'])
  const conditional: ConditionalGroup[] = []
  for (const field of closure) {
    const source = specOf(field).sources[platform]
    if (typeof source !== 'object' || source === null || 'derivedFrom' in source) continue
    for (const group of source.groups) always.add(group)
    for (const { group, for: condition } of source.when ?? []) {
      if (!conditional.some((entry) => entry.group === group && entry.for === condition)) conditional.push({ group, for: condition })
    }
  }
  return {
    always: [...always].sort(),
    conditional: conditional.filter(({ group }) => !always.has(group))
  }
}
