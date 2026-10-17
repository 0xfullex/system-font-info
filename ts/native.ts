import * as path from 'node:path'

import type { FontFace, GetFontsOptions, VariationAxis } from './types'

// What the native addon returns, before the JS layer normalizes it: language
// tags are not canonicalized, localized arrays may hold exact duplicates (the
// native side lists every name record), and the default strings (fullName,
// styleName, axis name) are not derived yet.

export type RawAxis = Omit<VariationAxis, 'name'>
export type RawFace = Omit<FontFace, 'fullName' | 'styleName' | 'axes'> & { axes?: RawAxis[] }
export interface RawFamily {
  family: string
  faces: RawFace[]
}

export interface NativeBinding {
  getFonts(options: Required<GetFontsOptions>): Promise<RawFamily[]>
  __testing: {
    registerFont(path: string): void
    unregisterFont(path: string): void
    /** The `opentype` object of one face of a font file. */
    readFontFile(path: string, index: number): NonNullable<FontFace['opentype']>
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
