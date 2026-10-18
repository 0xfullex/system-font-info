import { specOf } from './field-closure'
import type { Field, FontFamily } from './types'

// Removes what a call fetched only to compute something else. The closure holds the selected fields, the face
// identity and the inputs of derived fields; the caller gets the selected fields and the identity.

const withoutKeys = (target: object, keys: readonly string[]) => {
  for (const key of keys) delete (target as Record<string, unknown>)[key]
}

/**
 * Projects normalized families onto the selected fields, in place: removes the borrowed fields (`closure` minus
 * `selected`, except the identity, which every face keeps) and `faces` when no face-level field is selected.
 *
 * Only family and face objects change. A face's `opentype` and `axes` are shared between faces and only ever hold
 * selected entries (nothing is derived from them), so they are never touched.
 */
export function projectFamilies(families: FontFamily[], selected: ReadonlySet<Field>, closure: ReadonlySet<Field>): void {
  const borrowedFamilyKeys: string[] = []
  const borrowedFaceKeys: string[] = []
  for (const field of closure) {
    const spec = specOf(field)
    if (selected.has(field) || spec.identity) continue
    if (spec.level === 'family') borrowedFamilyKeys.push(field)
    else if (spec.level === 'face') borrowedFaceKeys.push(field.slice('faces.'.length))
    else throw new Error(`${field} was fetched but is not selected, and an ${spec.level} entry cannot be removed from a shared object`)
  }

  const hasFaceField = [...selected].some((field) => specOf(field).level !== 'family')
  for (const family of families) {
    withoutKeys(family, borrowedFamilyKeys)
    if (!hasFaceField) delete (family as Partial<FontFamily>).faces
    else for (const face of family.faces) withoutKeys(face, borrowedFaceKeys)
  }
}
