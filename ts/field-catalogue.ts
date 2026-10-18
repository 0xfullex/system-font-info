// The field catalogue: every field name a caller can select, with what the planner and the documentation
// need to know about it. It is the one source for
//   - the C++ planner (src/core/field_catalogue.h, generated),
//   - the Presets table of docs/api.md and the "group of every field" matrix of docs/platforms.md (generated),
//   - the request closure (field-closure.ts) and the work-per-group tests.
// `scripts/gen-fields.mjs` writes those from this file; test/types/catalogue.test.ts asserts at compile time that
// the names are exactly `Field` and that the preset memberships are exactly `PresetFields`.
//
// A TypeScript module rather than JSON so that `as const` keeps every name and preset a literal type, which
// those assertions need. Types are not repeated here: the interfaces of types.ts define them.

import type { Field, Preset } from './types'

/** The kinds of OS work a field can need; docs/platforms.md ("Source groups") defines them. */
export type Group = 'A' | 'B' | 'C' | 'D' | 'E'

export type Platform = 'darwin' | 'win32' | 'linux'

/** Where a field comes from on one platform. A platform that does not provide the field has `null` instead. */
export type Source =
  | {
      /** The groups the field always needs. */
      readonly groups: readonly Group[]
      /** Groups the field needs only in some cases; `for` says which, as documentation text. */
      readonly when?: readonly { readonly group: Group; readonly for: string }[]
    }
  /** Computed by the package from the fields named in `derivedFrom`; the native side is never asked for it. */
  | { readonly derivedFrom: readonly Field[] }
  /** No OS work: the platform has no such notion and the value is fixed. */
  | 'constant'
  /** The platform provides the field but its source has not been established. */
  | 'undocumented'

export interface FieldSpec {
  readonly name: Field
  readonly level: 'family' | 'face' | 'opentype' | 'platform'
  /** The smallest preset that contains the field; `null` when no preset does. */
  readonly preset: Preset | null
  /** Part of every face's identity (api.md, "Always included"). */
  readonly identity?: true
  /** The `opentype` entry's tables (empty for `tables`, which is the table directory itself). */
  readonly tables?: readonly string[]
  readonly sources: { readonly darwin: Source | null; readonly win32: Source | null; readonly linux: Source | null }
}

const A = { groups: ['A'] } as const
const B = { groups: ['B'] } as const
const C = { groups: ['C'] } as const
const D = { groups: ['D'] } as const
const E = { groups: ['E'] } as const
const A_AND_B = { groups: ['A', 'B'] } as const
const B_AND_C = { groups: ['B', 'C'] } as const
const B_C_FOR_CFF2 = { groups: ['B'], when: [{ group: 'C', for: '`cff2`' }] } as const
const B_C_FOR_NAMED_INSTANCES = { groups: ['B'], when: [{ group: 'C', for: 'named instances' }] } as const
const C_FOR_VARIABLE_FACES = { groups: [], when: [{ group: 'C', for: 'variable faces only' }] } as const

/** The package computes the field from `input` on every platform. */
const derivedEverywhere = <const F extends Field>(input: F) => {
  const source = { derivedFrom: [input] } as const
  return { darwin: source, win32: source, linux: source } as const
}
const everywhere = <const S extends Source>(source: S) => ({ darwin: source, win32: source, linux: source }) as const

/** An OpenType entry: group D on every platform. */
const opentype = <const N extends Field>(name: N, tables: readonly string[]) =>
  ({ name, level: 'opentype', preset: 'full', tables, sources: everywhere(D) }) as const

/** A platform field; every one is in `details`. */
const platformField = <const N extends Field, const S extends FieldSpec['sources']>(name: N, sources: S) =>
  ({ name, level: 'platform', preset: 'details', sources }) as const

const onlyDarwin = <const S extends Source>(source: S) => ({ darwin: source, win32: null, linux: null }) as const
const onlyWin32 = <const S extends Source>(source: S) => ({ darwin: null, win32: source, linux: null }) as const
const onlyLinux = <const S extends Source>(source: S) => ({ darwin: null, win32: null, linux: source }) as const

export const FIELD_CATALOGUE = [
  // ---- family level
  // DirectWrite gives a family's names directly; the other platforms have only the faces' names to collect.
  {
    name: 'familyNames',
    level: 'family',
    preset: 'names',
    sources: { darwin: { derivedFrom: ['faces.familyNames'] }, win32: A, linux: { derivedFrom: ['faces.familyNames'] } }
  },
  { name: 'supportedLanguages', level: 'family', preset: 'styles', sources: derivedEverywhere('faces.supportedLanguages') },
  { name: 'variable', level: 'family', preset: 'details', sources: derivedEverywhere('faces.variable') },
  { name: 'enabled', level: 'family', preset: 'styles', sources: derivedEverywhere('faces.enabled') },

  // ---- face level
  { name: 'faces.postscriptName', level: 'face', preset: 'styles', sources: everywhere(B) },
  { name: 'faces.fullName', level: 'face', preset: 'styles', sources: derivedEverywhere('faces.fullNames') },
  { name: 'faces.fullNames', level: 'face', preset: 'styles', sources: { darwin: C, win32: B, linux: B_C_FOR_NAMED_INSTANCES } },
  { name: 'faces.styleName', level: 'face', preset: 'styles', sources: derivedEverywhere('faces.styleNames') },
  { name: 'faces.styleNames', level: 'face', preset: 'styles', sources: { darwin: C, win32: B, linux: B_C_FOR_NAMED_INSTANCES } },
  { name: 'faces.familyNames', level: 'face', preset: 'styles', sources: { darwin: C, win32: A, linux: A } },
  { name: 'faces.weight', level: 'face', preset: 'styles', sources: everywhere(B) },
  { name: 'faces.width', level: 'face', preset: 'styles', sources: everywhere(B) },
  { name: 'faces.style', level: 'face', preset: 'styles', sources: everywhere(B) },
  { name: 'faces.monospace', level: 'face', preset: 'styles', sources: everywhere(B) },
  { name: 'faces.color', level: 'face', preset: 'styles', sources: everywhere(B) },
  { name: 'faces.symbol', level: 'face', preset: 'styles', sources: { darwin: C, win32: B, linux: B } },
  // On macOS a disabled font reports itself as enabled; only a second listing of the families without the disabled fonts
  // tells them apart, so `enabled` (and `scope`, which depends on it) costs a family list (A) on top of the attributes.
  { name: 'faces.enabled', level: 'face', preset: 'styles', sources: { darwin: A_AND_B, win32: 'constant', linux: 'constant' } },
  {
    name: 'faces.simulations',
    level: 'face',
    preset: 'styles',
    identity: true,
    sources: { darwin: 'constant', win32: B, linux: 'constant' }
  },
  { name: 'faces.supportedLanguages', level: 'face', preset: 'styles', sources: everywhere(B) },
  { name: 'faces.scope', level: 'face', preset: 'styles', sources: { darwin: A_AND_B, win32: B, linux: B } },
  // `file` is `file.path` and `file.index`; on macOS the index is C.
  { name: 'faces.file', level: 'face', preset: 'styles', identity: true, sources: { darwin: B_AND_C, win32: B, linux: B } },
  { name: 'faces.variable', level: 'face', preset: 'details', sources: { darwin: B, win32: C, linux: B } },
  { name: 'faces.format', level: 'face', preset: 'details', sources: { darwin: B_C_FOR_CFF2, win32: C, linux: B_C_FOR_CFF2 } },
  { name: 'faces.axes', level: 'face', preset: 'details', sources: everywhere(C) },
  {
    name: 'faces.coordinates',
    level: 'face',
    preset: 'styles',
    identity: true,
    sources: { darwin: C, win32: C, linux: C_FOR_VARIABLE_FACES }
  },
  { name: 'faces.namedInstanceIndex', level: 'face', preset: 'details', sources: { darwin: C, win32: C, linux: C_FOR_VARIABLE_FACES } },
  { name: 'faces.coverage', level: 'face', preset: null, sources: everywhere(E) },

  // ---- OpenType entries
  opentype('faces.opentype.tables', []),
  opentype('faces.opentype.names', ['name']),
  opentype('faces.opentype.head', ['head']),
  opentype('faces.opentype.hhea', ['hhea']),
  opentype('faces.opentype.vhea', ['vhea']),
  opentype('faces.opentype.os2', ['OS/2']),
  opentype('faces.opentype.post', ['post']),
  opentype('faces.opentype.maxp', ['maxp']),
  opentype('faces.opentype.cmap', ['cmap']),
  opentype('faces.opentype.fvar', ['fvar']),
  opentype('faces.opentype.meta', ['meta']),
  opentype('faces.opentype.layout', ['GSUB', 'GPOS']),

  // ---- platform fields (a platform that does not define a field has null; the fields are those of DarwinNative, Win32Native and LinuxNative)
  platformField('faces.platform.weight', everywhere(B)),
  platformField('faces.platform.width', everywhere(B)),
  platformField('faces.platform.slant', everywhere(B)),
  platformField('faces.platform.symbolicTraits', onlyDarwin(B)),
  platformField('faces.platform.format', { darwin: B, win32: 'undocumented', linux: B }),
  platformField('faces.platform.priority', onlyDarwin(B)),
  platformField('faces.platform.registrationScope', onlyDarwin(B)),
  platformField('faces.platform.downloadable', onlyDarwin(B)),
  platformField('faces.platform.downloaded', onlyDarwin(B)),
  platformField('faces.platform.styleName', onlyDarwin(B)),
  platformField('faces.platform.displayName', onlyDarwin(B)),
  platformField('faces.platform.localizedFamilyName', onlyDarwin(B)),
  platformField('faces.platform.localizedStyleName', onlyDarwin(B)),
  platformField('faces.platform.fixedAdvance', onlyDarwin(B)),
  platformField('faces.platform.features', onlyDarwin(C)),
  platformField('faces.platform.metrics', onlyDarwin(C)),
  platformField('faces.platform.shadowed', onlyDarwin(B)),
  platformField('faces.platform.typographicFamilyNames', onlyWin32(B)),
  platformField('faces.platform.typographicStyleNames', onlyWin32(B)),
  platformField('faces.platform.designLanguages', onlyWin32(B)),
  platformField('faces.platform.supportedLanguages', onlyWin32(B)),
  platformField('faces.platform.inactiveFamilyPreference', onlyWin32('undocumented')),
  platformField('faces.platform.sourceType', onlyWin32(B)),
  platformField('faces.platform.sourceName', onlyWin32(B)),
  platformField('faces.platform.axisValues', onlyWin32(B)),
  platformField('faces.platform.resourceAxes', onlyWin32(C)),
  platformField('faces.platform.additionalInformationalStrings', onlyWin32(C)),
  platformField('faces.platform.index', onlyLinux(B)),
  platformField('faces.platform.spacing', onlyLinux(B)),
  platformField('faces.platform.fontWrapper', onlyLinux(B)),
  platformField('faces.platform.foundry', onlyLinux(B)),
  platformField('faces.platform.decorative', onlyLinux(B)),
  platformField('faces.platform.fontHasHint', onlyLinux(B)),
  platformField('faces.platform.order', onlyLinux(B)),
  platformField('faces.platform.outline', onlyLinux(B)),
  platformField('faces.platform.scalable', onlyLinux(B)),
  platformField('faces.platform.antialias', onlyLinux(B)),
  platformField('faces.platform.pixelSize', onlyLinux(B))
] as const satisfies readonly FieldSpec[]
