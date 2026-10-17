import type { RawAxis, RawFace, RawFamily } from './native'
import type { FontFace, FontFamily, LocalizedString, OpenTypeInfo, VariationAxis } from './types'

// Turns the native result into the public shape: canonical language tags,
// merged duplicates, default strings, family aggregates, sorted families.
// Objects are changed in place, so an object that several faces share (their
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

function normalizeOpenType(opentype: OpenTypeInfo): void {
  // The raw name table view: languages are canonicalized, records are never merged.
  for (const record of opentype.names) record.language = canonicalizeTag(record.language)

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

function normalizeFace(face: RawFace, done: WeakSet<object>): FontFace {
  tidyLocalized(face.fullNames)
  tidyLocalized(face.styleNames)
  tidyLocalized(face.familyNames)
  tidyTags(face.languages)

  if (face.axes && !done.has(face.axes)) {
    done.add(face.axes)
    normalizeAxes(face.axes)
  }
  if (face.opentype && !done.has(face.opentype)) {
    done.add(face.opentype)
    normalizeOpenType(face.opentype)
  }

  const normalized = face as unknown as FontFace
  normalized.fullName = defaultString(face.fullNames)
  normalized.styleName = defaultString(face.styleNames)
  return normalized
}

function normalizeFamily(raw: RawFamily, done: WeakSet<object>): FontFamily {
  const faces = raw.faces.map((face) => normalizeFace(face, done))

  const familyNames: LocalizedString[] = []
  const languages: string[] = []
  for (const face of faces) {
    for (const name of face.familyNames) familyNames.push({ language: name.language, value: name.value })
    languages.push(...face.languages)
  }
  tidyLocalized(familyNames)
  dedupeStrings(languages)

  return {
    family: raw.family,
    familyNames,
    languages,
    variable: faces.some((face) => face.variable),
    enabled: faces.some((face) => face.enabled),
    faces
  }
}

/** Lowercased, compared by UTF-16 code units; ties broken by the raw string. Locale-independent on purpose. */
function compareFamilies(a: FontFamily, b: FontFamily): number {
  const lowerA = a.family.toLowerCase()
  const lowerB = b.family.toLowerCase()
  if (lowerA !== lowerB) return lowerA < lowerB ? -1 : 1
  if (a.family !== b.family) return a.family < b.family ? -1 : 1
  return 0
}

export function normalizeFamilies(raw: RawFamily[]): FontFamily[] {
  const done = new WeakSet<object>()
  const families = raw.map((family) => normalizeFamily(family, done))
  return families.sort(compareFamilies)
}
