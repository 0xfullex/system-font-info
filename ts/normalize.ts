import type { RawAxis, RawFace, RawFamily } from './native'
import type { Field, FontFace, FontFamily, LocalizedString, OpenTypeInfo, VariationAxis } from './types'

// Turns the native result into the public shape: canonical language tags,
// merged duplicates, default strings, family aggregates, sorted families.
// The result holds only the fetched keys. Objects are changed in place, so an object that several faces share (their
// `opentype`, their `axes`) is processed once and stays shared.

const canonicalTags = new Map<string, string>()

/** Intl.getCanonicalLocales for one tag; "" stays "", a tag Intl rejects is kept as written. */
export function canonicalizeTag(tag: string): string {
  if (tag === '') return ''
  let canonical = canonicalTags.get(tag)
  if (canonical === undefined) {
    try {
      canonical = Intl.getCanonicalLocales(tag)[0] ?? tag
    } catch {
      canonical = tag
    }
    canonicalTags.set(tag, canonical)
  }
  return canonical
}

/** Canonicalizes the languages of `strings` and removes exact (language, value) duplicates, keeping the first. In place. */
function tidyLocalized(strings: LocalizedString[]): void {
  const seen = new Set<string>()
  let kept = 0
  for (const entry of strings) {
    entry.language = canonicalizeTag(entry.language)
    const key = `${entry.language}\u0000${entry.value}`
    if (seen.has(key)) continue
    seen.add(key)
    strings[kept++] = entry
  }
  strings.length = kept
}

/** Canonicalizes the tags in `tags` and removes duplicates, keeping the first. In place. */
function tidyTags(tags: string[]): void {
  const seen = new Set<string>()
  let kept = 0
  for (const tag of tags) {
    const canonical = canonicalizeTag(tag)
    if (seen.has(canonical)) continue
    seen.add(canonical)
    tags[kept++] = canonical
  }
  tags.length = kept
}

/** Removes duplicates, keeping the first. In place. */
function dedupeStrings(values: string[]): void {
  const seen = new Set(values)
  if (seen.size === values.length) return
  seen.clear()
  let kept = 0
  for (const value of values) {
    if (seen.has(value)) continue
    seen.add(value)
    values[kept++] = value
  }
  values.length = kept
}

/** en-US, else any en / en-*, else language "", else the first entry, else "". */
export function defaultString(strings: readonly LocalizedString[]): string {
  const exact = strings.find((entry) => entry.language === 'en-US')
  if (exact) return exact.value
  const english = strings.find((entry) => entry.language === 'en' || entry.language.startsWith('en-'))
  if (english) return english.value
  const unspecified = strings.find((entry) => entry.language === '')
  if (unspecified) return unspecified.value
  return strings[0]?.value ?? ''
}

function normalizeOpenType(opentype: Partial<OpenTypeInfo>): void {
  // The raw name table view: languages are canonicalized, records are never merged.
  for (const record of opentype.names ?? []) record.language = canonicalizeTag(record.language)

  for (const axis of opentype.fvar?.axes ?? []) tidyLocalized(axis.names)
  for (const instance of opentype.fvar?.instances ?? []) tidyLocalized(instance.subfamilyNames)

  for (const table of [opentype.layout?.gsub, opentype.layout?.gpos]) {
    for (const names of Object.values(table?.featureNames ?? {})) tidyLocalized(names)
  }

  // Design/supported languages are script-language tags (for example "Latn");
  // those Intl cannot parse stay as written.
  if (opentype.meta?.dlng) tidyTags(opentype.meta.dlng)
  if (opentype.meta?.slng) tidyTags(opentype.meta.slng)
}

function normalizeAxes(axes: RawAxis[]): void {
  for (const axis of axes) {
    tidyLocalized(axis.names)
    ;(axis as VariationAxis).name = defaultString(axis.names)
  }
}

/** A key the field closure promised the native result; its absence is a defect of the native side, so it fails loudly. */
function required<T>(value: T | undefined, what: string): T {
  if (value === undefined) throw new Error(`system-font-info: the native result lacks ${what}`)
  return value
}

function normalizeFace(face: RawFace, derived: ReadonlySet<Field>, done: WeakSet<object>): FontFace {
  if (face.fullNames) tidyLocalized(face.fullNames)
  if (face.styleNames) tidyLocalized(face.styleNames)
  if (face.familyNames) tidyLocalized(face.familyNames)
  if (face.supportedLanguages) tidyTags(face.supportedLanguages)

  if (face.axes && !done.has(face.axes)) {
    done.add(face.axes)
    normalizeAxes(face.axes)
  }
  if (face.opentype && !done.has(face.opentype)) {
    done.add(face.opentype)
    normalizeOpenType(face.opentype)
  }

  const normalized = face as unknown as FontFace
  if (derived.has('faces.fullName')) normalized.fullName = defaultString(required(face.fullNames, 'faces.fullNames'))
  if (derived.has('faces.styleName')) normalized.styleName = defaultString(required(face.styleNames, 'faces.styleNames'))
  return normalized
}

/** Sets the family fields `derived` names from the (already normalized) faces; fields not in `derived` are left as they are. */
function deriveFamilyFields(family: FontFamily, derived: ReadonlySet<Field>): void {
  if (!(['familyNames', 'supportedLanguages', 'variable', 'enabled'] as const).some((field) => derived.has(field))) return
  const faces = required<FontFace[]>(family.faces, 'faces')
  if (derived.has('familyNames')) {
    const familyNames: LocalizedString[] = []
    for (const face of faces) {
      for (const name of required(face.familyNames, 'faces.familyNames')) familyNames.push({ language: name.language, value: name.value })
    }
    tidyLocalized(familyNames)
    family.familyNames = familyNames
  }
  if (derived.has('supportedLanguages')) {
    const supportedLanguages: string[] = []
    for (const face of faces) supportedLanguages.push(...required(face.supportedLanguages, 'faces.supportedLanguages'))
    dedupeStrings(supportedLanguages)
    family.supportedLanguages = supportedLanguages
  }
  if (derived.has('variable')) family.variable = faces.some((face) => required(face.variable, 'faces.variable'))
  if (derived.has('enabled')) family.enabled = faces.some((face) => required(face.enabled, 'faces.enabled'))
}

function normalizeFamily(raw: RawFamily, derived: ReadonlySet<Field>, done: WeakSet<object>): FontFamily {
  for (const face of raw.faces ?? []) normalizeFace(face, derived, done)
  if (raw.familyNames) tidyLocalized(raw.familyNames)
  const family = raw as unknown as FontFamily
  deriveFamilyFields(family, derived)
  return family
}

/** Lowercased, compared by UTF-16 code units; ties broken by the raw string. Locale-independent on purpose. */
function compareFamilies(a: FontFamily, b: FontFamily): number {
  const lowerA = a.family.toLowerCase()
  const lowerB = b.family.toLowerCase()
  if (lowerA !== lowerB) return lowerA < lowerB ? -1 : 1
  if (a.family !== b.family) return a.family < b.family ? -1 : 1
  return 0
}

/**
 * Normalizes a native result in place and sorts the families. Only the keys the native side returned are handled;
 * `derived` names the selected fields this platform computes (`BuiltRequest.derived`), which are computed from the
 * normalized faces. The objects are typed as the complete shapes but hold only the fetched fields until `projectFamilies`.
 */
export function normalizeFamilies(raw: RawFamily[], derived: ReadonlySet<Field> = new Set()): FontFamily[] {
  const done = new WeakSet<object>()
  const families = raw.map((family) => normalizeFamily(family, derived, done))
  return families.sort(compareFamilies)
}
