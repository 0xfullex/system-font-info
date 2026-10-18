import type { LanguageCoverage } from './types'

// Resolves the tags of the `languageCoverage` option to fc-lang orthographies and assembles the result entries.
// Pure: the orthography names are passed in, so nothing here needs the native addon.

/** A tag the caller asked for, and the orthography it is counted against. */
export interface ResolvedTag {
  /** The requested tag, canonicalized. */
  language: string
  /** The fc-lang name. */
  orthography: string
}

const invalidTag = (tag: string) => new RangeError(`languageCoverage: "${tag}" is not a valid language tag`)

/**
 * The orthography `tag` is counted against; docs/api.md ("Matching a tag to an orthography") defines the rule.
 * Tries, in order: (0) the tag itself, lowercased with `_` read as `-`, when it is exactly an fc-lang name; (a) its
 * own language and region; (b) the language and region after adding likely subtags; (c) the language alone.
 * Throws a `RangeError` naming the tag when it is not a language tag or resolves to no orthography.
 */
export function resolveOrthography(tag: string, names: ReadonlySet<string>): ResolvedTag {
  const hyphenated = tag.replaceAll('_', '-')
  let locale: Intl.Locale
  try {
    locale = new Intl.Locale(Intl.getCanonicalLocales(hyphenated)[0] ?? hyphenated)
  } catch {
    throw invalidTag(tag)
  }
  const language = locale.toString()

  const exact = hyphenated.toLowerCase()
  if (names.has(exact)) return { language, orthography: exact }

  const withRegion = (l: Intl.Locale) => (l.region === undefined ? undefined : `${l.language}-${l.region}`.toLowerCase())
  const maximized = locale.maximize()
  for (const candidate of [withRegion(locale), withRegion(maximized), locale.language]) {
    if (candidate !== undefined && names.has(candidate)) return { language, orthography: candidate }
  }
  throw new RangeError(`languageCoverage: "${tag}" resolves to no orthography`)
}

/** The tags resolved in request order, and the distinct orthographies in first-seen order (what the native side counts). */
export function resolveTags(tags: readonly string[], names: ReadonlySet<string>): { tags: ResolvedTag[]; orthographies: string[] } {
  const resolved = tags.map((tag) => resolveOrthography(tag, names))
  return { tags: resolved, orthographies: [...new Set(resolved.map((entry) => entry.orthography))] }
}

/**
 * One family's `languageCoverage`: an entry per requested tag, in request order. `counts[i]` is the covered count of
 * `orthographies[i]`, `required` the code point count of each orthography.
 */
export function coverageEntries(
  tags: readonly ResolvedTag[],
  orthographies: readonly string[],
  counts: readonly number[],
  required: Readonly<Record<string, number>>
): LanguageCoverage[] {
  return tags.map(({ language, orthography }) => {
    const covered = counts[orthographies.indexOf(orthography)]
    const total = required[orthography]
    if (covered === undefined || total === undefined)
      throw new Error(`system-font-info: the native result has no count for orthography ${orthography}`)
    return { language, orthography, required: total, covered }
  })
}
