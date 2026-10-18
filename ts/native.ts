import * as path from 'node:path'

import { detectLibc, findTarget, type Libc, TARGETS, type TargetId, targetName } from './targets'
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

/** What the loader reads from its surroundings. The real one is `realEnvironment`; tests pass their own. */
export interface LoaderEnvironment {
  platform: string
  arch: string
  /** Asked on Linux only. */
  libc: () => Libc
  /** The directory that holds `prebuilds/` and `build/`. */
  packageRoot: string
  /** Requires the prebuilt addon of a target. */
  requirePrebuilt: (id: TargetId) => unknown
  /** Requires the addon file a local build left at `file`. */
  requireFile: (file: string) => unknown
}

/**
 * Requires the prebuilt addon of a target. Each target has its own string-literal `require`, so a bundler can trace
 * every path and include it; a path assembled at run time would be invisible to it. The compiler checks that the cases
 * cover `TargetId` and `test/native.test.mjs` that they are the table's targets, each with its own directory.
 */
function requirePrebuilt(id: TargetId): unknown {
  switch (id) {
    case 'darwin-arm64':
      return require('../prebuilds/darwin-arm64/system-font-info.node')
    case 'darwin-x64':
      return require('../prebuilds/darwin-x64/system-font-info.node')
    case 'win32-x64':
      return require('../prebuilds/win32-x64/system-font-info.node')
    case 'win32-arm64':
      return require('../prebuilds/win32-arm64/system-font-info.node')
    case 'linux-x64-glibc':
      return require('../prebuilds/linux-x64-glibc/system-font-info.node')
    case 'linux-arm64-glibc':
      return require('../prebuilds/linux-arm64-glibc/system-font-info.node')
    default: {
      // Reached only when a target was added to the table without a case above: that fails to compile here.
      const unknownTarget: never = id
      throw new Error(`system-font-info: no prebuilt addon is required for ${unknownTarget}`)
    }
  }
}

function realEnvironment(): LoaderEnvironment {
  return {
    platform: process.platform,
    arch: process.arch,
    libc: detectLibc,
    packageRoot: path.join(__dirname, '..'),
    requirePrebuilt,
    requireFile: (file) => require(file)
  }
}

/** The system libraries the Linux addon links dynamically, with the package that provides each. */
const SYSTEM_LIBRARIES = [
  { soname: 'libfontconfig.so.1', debian: 'libfontconfig1', rpm: 'fontconfig' },
  { soname: 'libfreetype.so.6', debian: 'libfreetype6', rpm: 'freetype' }
]

/** For each system library the dynamic loader reported missing, how to install it; empty when none was. */
function missingLibraryHints(failures: string[]): string[] {
  return SYSTEM_LIBRARIES.filter(({ soname }) =>
    failures.some((failure) => failure.includes(`${soname}: cannot open shared object file`))
  ).map(
    ({ soname, debian, rpm }) =>
      `${soname} is not installed. Install it: apt install ${debian} (Debian, Ubuntu), dnf install ${rpm} (Fedora, RHEL).`
  )
}

/**
 * Loads the addon: the prebuilt one of this platform, architecture and (on Linux) libc, else the one a local build left
 * in `build/Release`. A process that matches no target (musl, or a platform not listed) can only use a local build.
 * Throws an error that lists every path tried with its failure.
 */
export function loadBinding(environment: LoaderEnvironment): NativeBinding {
  const { platform, arch, packageRoot } = environment
  const libc = platform === 'linux' ? environment.libc() : undefined
  const name = targetName(platform, arch, libc)
  const target = findTarget(platform, arch, libc)

  const attempts: { file: string; load: () => unknown }[] = []
  if (target) {
    attempts.push({ file: path.join(packageRoot, 'prebuilds', target.id, ADDON_FILE), load: () => environment.requirePrebuilt(target.id) })
  }
  const localBuild = path.join(packageRoot, 'build', 'Release', ADDON_FILE)
  attempts.push({ file: localBuild, load: () => environment.requireFile(localBuild) })

  const failures: string[] = []
  for (const { file, load } of attempts) {
    try {
      return load() as NativeBinding
    } catch (error) {
      failures.push(`  ${file}: ${error instanceof Error ? error.message : String(error)}`)
    }
  }

  const reason =
    libc === 'musl'
      ? `musl libc is not supported yet, so there is no prebuilt addon for ${name}`
      : target
        ? `cannot load the native addon for ${name}`
        : `no prebuilt addon for ${name}`
  const supported = target ? '' : ` (prebuilt targets: ${TARGETS.map((t) => t.id).join(', ')})`
  const hints = platform === 'linux' ? missingLibraryHints(failures) : []
  throw new Error([`system-font-info: ${reason}${supported}. Tried:`, ...failures, ...hints].join('\n'))
}

let binding: NativeBinding | undefined

/** Loads the addon on first use, see `loadBinding`. */
export function loadNative(): NativeBinding {
  binding ??= loadBinding(realEnvironment())
  return binding
}
