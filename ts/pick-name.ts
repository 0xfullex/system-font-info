import type { LocalizedString } from './types'

// What is known about one language tag, computed once per tag.
interface TagInfo {
  canonical: string
  /** Language subtag as written (zh for zh-Hant). */
  language: string
  /** Likely-subtags expansion (zh-CN -> zh-Hans-CN); undefined for "und", which would expand to a language that was never named. */
  maximized?: { tag: string; language: string; script: string | undefined }
}

const tagInfos = new Map<string, TagInfo | null>()

function languageOf(tag: string): string {
  return tag.split('-')[0] ?? tag
}

/** null when Intl rejects the tag. */
function tagInfo(tag: string): TagInfo | null {
  let info = tagInfos.get(tag)
  if (info === undefined) {
    try {
      const locale = new Intl.Locale(tag)
      // The language is read off the tag: Intl.Locale#language is undefined for "und" in V8.
      const canonical = locale.toString()
      info = { canonical, language: languageOf(canonical) }
      if (info.language !== 'und') {
        const maximized = locale.maximize()
        info.maximized = { tag: maximized.toString(), language: languageOf(maximized.toString()), script: maximized.script }
      }
    } catch {
      info = null
    }
    tagInfos.set(tag, info)
  }
  return info
}

type Matcher = (requested: TagInfo, candidate: TagInfo) => boolean

// From the exact tag down to the bare language.
const matchLevels: Matcher[] = [
  (requested, candidate) => requested.canonical === candidate.canonical,
  (requested, candidate) =>
    requested.maximized !== undefined && requested.maximized.tag === candidate.maximized?.tag,
  (requested, candidate) =>
    requested.maximized !== undefined &&
    requested.maximized.language === candidate.maximized?.language &&
    requested.maximized.script === candidate.maximized?.script,
  (requested, candidate) => requested.language !== 'und' && requested.language === candidate.language
]

/**
 * The value of the entry in `names` that best fits `locales`, or undefined when none fits.
 *
 * Locales are tried in order, and a locale is tried at every match level before the next
 * locale is considered: (1) the same canonical tag, (2) the same tag after adding likely
 * subtags (zh-CN matches zh-Hans-CN), (3) the same language and script, (4) the same
 * language. Within a level the first matching entry wins. Entries with language "" never
 * match; a tag Intl rejects only matches an identical tag.
 */
export function pickName(names: readonly LocalizedString[], locales: readonly string[]): string | undefined {
  const candidates = names.filter((name) => name.language !== '')
  for (const locale of locales) {
    if (locale === '') continue
    const requested = tagInfo(locale)
    if (!requested) {
      const identical = candidates.find((name) => name.language === locale)
      if (identical) return identical.value
      continue
    }
    for (const matches of matchLevels) {
      for (const name of candidates) {
        const candidate = tagInfo(name.language)
        if (candidate ? matches(requested, candidate) : false) return name.value
      }
    }
  }
  return undefined
}
