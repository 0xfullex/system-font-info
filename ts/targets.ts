import type { Platform } from './field-catalogue'

/** The C library of a Linux process. Only glibc has prebuilt binaries. */
export type Libc = 'glibc' | 'musl'

/** One platform, architecture and libc combination that ships a prebuilt addon. */
export interface Target {
  /** The directory under `prebuilds/` that holds the addon: `<platform>-<arch>`, with `-<libc>` appended on Linux. */
  id: string
  platform: Platform
  arch: 'x64' | 'arm64'
  /** Present on Linux only. */
  libc?: 'glibc'
  /** The GitHub-hosted runner that builds this target natively (no cross builds) and runs its tests. */
  runner: string
  /** Whether the test suite runs on this target's runner; otherwise the addon is only built and loaded. */
  tests: boolean
}

/**
 * The one list of targets. The loader (`native.ts`), `scripts/copy-prebuild.mjs` and the CI matrix
 * (`scripts/targets.mjs --matrix`) all read it; the literal `require` of every target in `native.ts` is the only
 * other copy, and `test/native.test.mjs` asserts the two agree.
 */
export const TARGETS = [
  { id: 'darwin-arm64', platform: 'darwin', arch: 'arm64', runner: 'macos-15', tests: true },
  { id: 'darwin-x64', platform: 'darwin', arch: 'x64', runner: 'macos-15-intel', tests: true },
  { id: 'win32-x64', platform: 'win32', arch: 'x64', runner: 'windows-2025', tests: true },
  { id: 'win32-arm64', platform: 'win32', arch: 'arm64', runner: 'windows-11-arm', tests: true },
  { id: 'linux-x64-glibc', platform: 'linux', arch: 'x64', libc: 'glibc', runner: 'ubuntu-24.04', tests: true },
  { id: 'linux-arm64-glibc', platform: 'linux', arch: 'arm64', libc: 'glibc', runner: 'ubuntu-24.04-arm', tests: true }
] as const satisfies readonly Target[]

type KnownTarget = (typeof TARGETS)[number]
export type TargetId = KnownTarget['id']

/** The target of a platform, architecture and (on Linux) libc, or undefined when none ships a prebuilt addon. */
export function findTarget(platform: string, arch: string, libc?: Libc): KnownTarget | undefined {
  return TARGETS.find((target) => target.platform === platform && target.arch === arch && (target as Target).libc === libc)
}

/** The name of a combination, which is a target's `id` when a target exists for it (`linux-x64-musl` has none). */
export function targetName(platform: string, arch: string, libc?: Libc): string {
  return libc ? `${platform}-${arch}-${libc}` : `${platform}-${arch}`
}

let detectedLibc: Libc | undefined

/**
 * The libc of this Linux process, detected once: Node's diagnostic report names the glibc version it runs on, and
 * has no such entry on musl. Meaningless on other platforms.
 */
export function detectLibc(): Libc {
  if (detectedLibc) return detectedLibc
  // `excludeNetwork` (Node 22+) keeps the report from enumerating network interfaces, which is slow and unneeded.
  const report = process.report as unknown as { excludeNetwork: boolean; getReport(): object }
  const previous = report.excludeNetwork
  report.excludeNetwork = true
  try {
    const { header } = report.getReport() as { header: { glibcVersionRuntime?: string } }
    detectedLibc = header.glibcVersionRuntime ? 'glibc' : 'musl'
  } finally {
    report.excludeNetwork = previous
  }
  return detectedLibc
}
