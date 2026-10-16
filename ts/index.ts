import { loadNative } from './native'
import { normalizeFamilies } from './normalize'
import type { FontFamily, GetFontsOptions } from './types'

export { pickName } from './pick-name'
export { watch } from './watch'
export type * from './types'

/** Lists the installed fonts, grouped by family and sorted by family name. */
export async function getFonts(options: GetFontsOptions = {}): Promise<FontFamily[]> {
  const raw = await loadNative().getFonts({
    tables: options.tables ?? true,
    coverage: options.coverage ?? false
  })
  return normalizeFamilies(raw)
}
