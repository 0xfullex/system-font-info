import type { Platform } from './field-catalogue'
import { coverageEntries, resolveTags } from './language-coverage'
import { loadNative, type NativeBinding } from './native'
import { normalizeFamilies } from './normalize'
import { projectFamilies } from './project'
import { buildRequest } from './request'
import type { FontFamily } from './types'

/** What the call is for: all families by the options, or the one family getFamily was given. */
export type Mode = { getFamily: false } | { getFamily: true; family: string }

/** Where the call gets its binding and platform; tests replace them. */
export interface Dependencies {
  loadBinding?: () => NativeBinding
  platform?: Platform
}

/**
 * The pipeline behind getFamilies and getFamily: validate the options, load the addon, resolve the `languageCoverage`
 * tags (which needs the addon's orthography names), ask the addon for the closure of the selected fields, normalize,
 * assemble `languageCoverage`, project onto the selected fields. The families come back sorted.
 */
export async function listFamilies(options: unknown, mode: Mode, dependencies: Dependencies = {}): Promise<FontFamily[]> {
  const request = buildRequest(options, { getFamily: mode.getFamily, platform: dependencies.platform })
  const binding = (dependencies.loadBinding ?? loadNative)()
  const coverage = request.languageCoverage && resolveTags(request.languageCoverage, new Set(Object.keys(binding.orthographies)))
  const families = mode.getFamily ? [mode.family] : request.families

  const raw = await binding.getFamilies({
    fields: [...request.closure],
    families: families && [...families],
    orthographies: coverage?.orthographies ?? []
  })
  const result = normalizeFamilies(raw, request.derived)

  if (coverage) {
    for (const family of result) {
      // The native side returns the counts in `languageCoverage`; they are replaced by the entries.
      const counts = (family.languageCoverage as unknown as number[] | undefined) ?? []
      family.languageCoverage = coverageEntries(coverage.tags, coverage.orthographies, counts, binding.orthographies)
    }
  }
  projectFamilies(result, request.selected, request.closure)
  return result
}
