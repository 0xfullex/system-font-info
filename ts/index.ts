import { listFamilies } from './list-families'
import type { GetFamiliesOptions, GetFamilyOptions, SelectedFamily } from './types'

export { pickName } from './pick-name'
export type * from './types'

/** Lists the installed font families with the fields `options` selects, sorted by family name. */
export async function getFamilies<const O extends GetFamiliesOptions>(options: O): Promise<Array<SelectedFamily<O>>> {
  return (await listFamilies(options, { getFamily: false })) as unknown as Array<SelectedFamily<O>>
}

/** The family whose name is exactly `family`, with the fields `options` selects; `undefined` when there is none. */
export async function getFamily<const O extends GetFamilyOptions>(family: string, options: O): Promise<SelectedFamily<O> | undefined> {
  if (typeof family !== 'string') throw new TypeError('family must be a string')
  const [found] = await listFamilies(options, { getFamily: true, family })
  return found as unknown as SelectedFamily<O> | undefined
}
