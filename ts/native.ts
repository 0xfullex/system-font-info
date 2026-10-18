import * as path from 'node:path'

import type { FontFace, LocalizedString, VariationAxis } from './types'

// What the native addon is asked for and what it returns, before the JS layer
// normalizes it. The request names the fields to produce, so a result holds
// only those keys: language tags are not canonicalized, localized arrays may
// hold exact duplicates (the native side lists every name record), and the
// default strings (fullName, styleName, axis name) and the family aggregates
// are not derived yet.

/** The request the JS layer sends; docs/platforms.md and src/core/README.md describe how the addon plans its work from it. */
export interface NativeRequest {
  /** Catalogue field names to produce: the closure of the selection on this platform. Fields the platform derives are not in it. */
  fields: string[]
  /** Only the families whose `family` equals one of these; omitted: every family; `[]`: none. */
  families?: string[]
  /** Resolved, distinct fc-lang names. The order is the order of every family's `languageCoverage` counts. */
  orthographies: string[]
}

export type RawAxis = Omit<VariationAxis, 'name'>
export type RawFace = Partial<Omit<FontFace, 'fullName' | 'styleName' | 'axes'>> & { axes?: RawAxis[] }
export interface RawFamily {
  family: string
  /** Only where the platform provides family names itself (Windows); elsewhere the JS layer collects them from the faces. */
  familyNames?: LocalizedString[]
  faces?: RawFace[]
  /** The covered code point count of each requested orthography, in the order of `NativeRequest.orthographies`. */
  languageCoverage?: number[]
}

/** The groups of work a call did; only for tests. */
export interface WorkLog {
  fontsOpened: number
  tableEntries: number
  characterSets: number
}

export interface NativeBinding {
  getFamilies(request: NativeRequest): Promise<RawFamily[]>
  /** Every available orthography: its fc-lang name and its code point count. */
  orthographies: Record<string, number>
  __testing: {
    registerFont(path: string): void
    unregisterFont(path: string): void
    /** Turns the fonts of a file registered for this process off or on, for this process only. Throws where the platform has no disabled state. */
    setFontEnabled(path: string, enabled: boolean): void
    /** The `opentype` object of one face of a font file, with the named entries only (every entry when omitted). */
    readFontFile(path: string, index: number, entries?: string[]): NonNullable<FontFace['opentype']>
    getFamiliesWithWork(request: NativeRequest): Promise<{ families: RawFamily[]; work: WorkLog }>
  }
}

const ADDON_FILE = 'system-font-info.node'

let binding: NativeBinding | undefined

/** Loads the addon on first use: a prebuilt binary for this platform and architecture, else a local build. */
export function loadNative(): NativeBinding {
  if (binding) return binding
  const packageRoot = path.join(__dirname, '..')
  const candidates = [
    path.join(packageRoot, 'prebuilds', `${process.platform}-${process.arch}`, ADDON_FILE),
    path.join(packageRoot, 'build', 'Release', ADDON_FILE)
  ]
  const failures: string[] = []
  for (const candidate of candidates) {
    try {
      binding = require(candidate) as NativeBinding
      return binding
    } catch (error) {
      failures.push(`  ${candidate}: ${error instanceof Error ? error.message : String(error)}`)
    }
  }
  throw new Error(`system-font-info: cannot load the native addon for ${process.platform}-${process.arch}. Tried:\n${failures.join('\n')}`)
}
