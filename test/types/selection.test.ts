// Type-level tests for the selection types in ts/types.ts. Nothing here runs: `npm run typecheck` compiles it,
// and a wrong inference is a compile error. `// @ts-expect-error` lines assert that a call is rejected.

import type {
  DarwinNative,
  Field,
  FontFace,
  FontFamily,
  GetFamiliesOptions,
  GetFamilyOptions,
  LanguageCoverage,
  LinuxNative,
  LocalizedString,
  PresetFields,
  SelectedFamily,
  Win32Native
} from '../../ts/types.ts'

type Equal<A, B> = (<T>() => T extends A ? 1 : 2) extends <T>() => T extends B ? 1 : 2 ? true : false
type Expect<T extends true> = T

/** Flattens intersections so that `Equal` compares shapes rather than how they were assembled. */
type Flat<T> = T extends readonly unknown[] ? { [K in keyof T]: Flat<T[K]> } : T extends object ? { [K in keyof T]: Flat<T[K]> } : T

// The signatures of docs/api.md "Functions", declared here because the test project does not compile the implementation.
declare function getFamilies<const O extends GetFamiliesOptions>(options: O): Promise<Array<SelectedFamily<O>>>
declare function getFamily<const O extends GetFamilyOptions>(family: string, options: O): Promise<SelectedFamily<O> | undefined>

type FaceOf<F> = F extends { faces: Array<infer X> } ? X : never

// ---- the options require a selection

getFamilies({ fields: ['faces.weight'] })
getFamilies({ preset: 'names' })
getFamilies({ fields: [], preset: 'styles', families: ['Skia'], languageCoverage: ['ja'] })
getFamily('Skia', { preset: 'names' })
getFamily('Skia', { fields: [], languageCoverage: ['ja'] })

// @ts-expect-error neither fields nor preset
getFamilies({})
// @ts-expect-error neither fields nor preset, only a filter
getFamilies({ families: ['Skia'] })
// @ts-expect-error neither fields nor preset, only a coverage request
getFamilies({ languageCoverage: ['ja'] })
// @ts-expect-error getFamily takes no families option
getFamily('Skia', { preset: 'names', families: ['Skia'] })
// @ts-expect-error getFamily takes no families option, not even an empty one
getFamily('Skia', { fields: [], families: [] })
// @ts-expect-error getFamily still needs a selection
getFamily('Skia', {})
// @ts-expect-error not a field name
getFamilies({ fields: ['faces.nope'] })
// @ts-expect-error not a field name on any platform
getFamilies({ fields: ['faces.platform.nope'] })
// @ts-expect-error not an OpenType entry
getFamilies({ fields: ['faces.opentype.glyf'] })
// @ts-expect-error selection stops at the axis list
getFamilies({ fields: ['faces.axes.min'] })
// @ts-expect-error selection stops at the OS/2 object
getFamilies({ fields: ['faces.opentype.os2.weightClass'] })
// @ts-expect-error `family` is always returned and is not a field name
getFamilies({ fields: ['family'] })
// @ts-expect-error `faces` is not a field name
getFamilies({ fields: ['faces'] })
// @ts-expect-error `languageCoverage` is an option, not a field name
getFamilies({ fields: ['languageCoverage'] })
// @ts-expect-error `faces.opentype` alone is not a field name
getFamilies({ fields: ['faces.opentype'] })
// @ts-expect-error `faces.platform` alone is not a field name
getFamilies({ fields: ['faces.platform'] })
// @ts-expect-error not a preset
getFamilies({ preset: 'everything' })

// ---- the field names are the paths into the complete shapes

type _FamilyFieldsAreFamilyKeys = Expect<
  Equal<Extract<Field, `${string}`> & keyof FontFamily, 'familyNames' | 'supportedLanguages' | 'variable' | 'enabled'>
>
type _CoverageFieldExists = Expect<Equal<'faces.coverage' extends Field ? true : false, true>>
type _ArbitraryDeepPathIsNotAField = Expect<Equal<'faces.axes.min' extends Field ? true : false, false>>
type _PresetsGrow = Expect<
  Equal<PresetFields['names'], 'familyNames'> &
    Equal<PresetFields['names'] extends PresetFields['styles'] ? true : false, true> &
    Equal<PresetFields['styles'] extends PresetFields['details'] ? true : false, true> &
    Equal<PresetFields['details'] extends PresetFields['full'] ? true : false, true>
>
type _FullHasNoCoverage = Expect<Equal<'faces.coverage' extends PresetFields['full'] ? true : false, false>>

// ---- the worked example of docs/api.md "Result typing"

export async function workedExample() {
  const families = await getFamilies({ fields: ['faces.weight', 'faces.platform.features'] })
  type Actual = Flat<(typeof families)[number]>
  type Expected = {
    family: string
    faces: Array<{
      file?: { path: string; index?: number }
      coordinates?: Record<string, number>
      simulations: { bold: boolean; oblique: boolean }
      weight: number
      platform: { os: 'darwin'; features?: DarwinNative['features'] } | { os: 'win32' } | { os: 'linux' }
    }>
  }
  type _Match = Expect<Equal<Flat<Actual>, Flat<Expected>>>
}

// ---- what each selection gives

export async function selections() {
  // Nothing selected: the family name and nothing else.
  const none = await getFamilies({ fields: [] })
  type _None = Expect<Equal<Flat<(typeof none)[number]>, { family: string }>>

  // A family field only: no faces.
  const family = await getFamilies({ fields: ['variable', 'supportedLanguages'] })
  type _Family = Expect<Equal<Flat<(typeof family)[number]>, { family: string; variable: boolean; supportedLanguages: string[] }>>

  // A face field brings the identity along.
  const named = await getFamilies({ fields: ['faces.postscriptName'] })
  type _Named = Expect<
    Equal<
      Flat<(typeof named)[number]>,
      {
        family: string
        faces: Array<{
          file?: { path: string; index?: number }
          coordinates?: Record<string, number>
          simulations: { bold: boolean; oblique: boolean }
          postscriptName: string
        }>
      }
    >
  >

  // preset and fields give the union.
  const both = await getFamilies({ preset: 'names', fields: ['enabled', 'faces.scope'] })
  type _Both = Expect<
    Equal<
      Flat<(typeof both)[number]>,
      {
        family: string
        familyNames: LocalizedString[]
        enabled: boolean
        faces: Array<{
          file?: { path: string; index?: number }
          coordinates?: Record<string, number>
          simulations: { bold: boolean; oblique: boolean }
          scope: 'system' | 'user' | 'process' | 'unknown'
        }>
      }
    >
  >

  // `fields` typed as Field[] falls back to every field.
  const wide: Field[] = []
  const everything = await getFamilies({ fields: wide })
  type EverythingFace = FaceOf<(typeof everything)[number]>
  type _EverythingFamily = Expect<
    Equal<keyof (typeof everything)[number], 'family' | 'familyNames' | 'supportedLanguages' | 'variable' | 'enabled' | 'faces'>
  >
  type _EverythingFace = Expect<Equal<keyof EverythingFace, Exclude<keyof FontFace, 'coverage'> | 'coverage'>>
}

// ---- presets: styles has no `format`, details has it

export async function presets() {
  const styles = await getFamilies({ preset: 'styles' })
  const details = await getFamilies({ preset: 'details' })
  const full = await getFamilies({ preset: 'full' })
  type StylesFace = FaceOf<(typeof styles)[number]>
  type DetailsFace = FaceOf<(typeof details)[number]>
  type FullFace = FaceOf<(typeof full)[number]>

  type _StylesHasNoFormat = Expect<Equal<'format' extends keyof StylesFace ? true : false, false>>
  type _StylesHasNoVariable = Expect<Equal<'variable' extends keyof StylesFace ? true : false, false>>
  type _StylesHasNoPlatform = Expect<Equal<'platform' extends keyof StylesFace ? true : false, false>>
  type _StylesHasName = Expect<Equal<StylesFace['fullName'], string>>
  type _DetailsHasFormat = Expect<Equal<DetailsFace['format'], FontFace['format']>>
  type _DetailsHasNoOpenType = Expect<Equal<'opentype' extends keyof DetailsFace ? true : false, false>>
  type _FullHasOpenType = Expect<Equal<'opentype' extends keyof FullFace ? true : false, true>>
  type _NoCoverageInFull = Expect<Equal<'coverage' extends keyof FullFace ? true : false, false>>
  type _DetailsFamilyKeys = Expect<
    Equal<keyof (typeof details)[number], 'family' | 'familyNames' | 'supportedLanguages' | 'variable' | 'enabled' | 'faces'>
  >
  type _StylesFamilyKeys = Expect<
    Equal<keyof (typeof styles)[number], 'family' | 'familyNames' | 'supportedLanguages' | 'enabled' | 'faces'>
  >
}

// ---- optional stays optional, selected coverage is always present

export async function presence() {
  const coverage = await getFamilies({ fields: ['faces.coverage', 'faces.axes', 'faces.namedInstanceIndex'], languageCoverage: ['ja'] })
  type Family = (typeof coverage)[number]
  type Face = FaceOf<Family>

  type _FamilyCoverageIsRequired = Expect<Equal<Family['languageCoverage'], LanguageCoverage[]>>
  type _FaceCoverageIsRequired = Expect<Equal<Face['coverage'], Array<[number, number]>>>
  type _AxesStayOptional = Expect<Equal<Face['axes'], FontFace['axes']>>
  type _NamedInstanceStaysOptional = Expect<Equal<Face['namedInstanceIndex'], FontFace['namedInstanceIndex']>>
  type _AxesAreOptional = Expect<Equal<undefined extends Face['axes'] ? true : false, true>>
  type _IdentityStaysOptional = Expect<Equal<undefined extends Face['file'] ? true : false, true>>

  // Without the option there is no languageCoverage.
  const plain = await getFamilies({ preset: 'names' })
  type _NoLanguageCoverage = Expect<Equal<'languageCoverage' extends keyof (typeof plain)[number] ? true : false, false>>

  // With the option and nothing else selected, it is still the only addition.
  const only = await getFamilies({ fields: [], languageCoverage: ['zh-CN', 'ja'] })
  type _Only = Expect<Equal<Flat<(typeof only)[number]>, { family: string; languageCoverage: LanguageCoverage[] }>>

  const one = await getFamily('Skia', { fields: [], languageCoverage: ['ja'] })
  type _GetFamily = Expect<Equal<Flat<typeof one>, { family: string; languageCoverage: LanguageCoverage[] } | undefined>>

  // The entries describe the orthography.
  type _Entry = Expect<Equal<LanguageCoverage, { language: string; orthography: string; required: number; covered: number }>>
}

// ---- opentype entries

export async function openTypeEntries() {
  const some = await getFamilies({ fields: ['faces.opentype.os2', 'faces.opentype.names'] })
  type Face = FaceOf<(typeof some)[number]>
  type OpenType = NonNullable<Face['opentype']>
  type _Keys = Expect<Equal<keyof OpenType, 'os2' | 'names' | 'errors'>>
  type _OpenTypeIsOptional = Expect<Equal<undefined extends Face['opentype'] ? true : false, true>>
  // Entries stay optional (a font may lack the table); `names` is the one the full shape requires.
  type _Os2Optional = Expect<Equal<undefined extends OpenType['os2'] ? true : false, true>>
  type _NamesRequired = Expect<Equal<undefined extends OpenType['names'] ? true : false, false>>
  type _NoPlatform = Expect<Equal<'platform' extends keyof Face ? true : false, false>>
}

// ---- faces.platform.<field> narrowing

export async function platformNarrowing() {
  const one = await getFamilies({ fields: ['faces.platform.features'] })
  type Platform = FaceOf<(typeof one)[number]>['platform']
  type _Union = Expect<
    Equal<Flat<Platform>, Flat<{ os: 'darwin'; features?: DarwinNative['features'] } | { os: 'win32' } | { os: 'linux' }>>
  >

  // A field that exists on several platforms is typed per platform.
  const weight = await getFamilies({ fields: ['faces.platform.weight', 'faces.platform.format'] })
  type WeightPlatform = FaceOf<(typeof weight)[number]>['platform']
  type _PerPlatform = Expect<
    Equal<
      Flat<WeightPlatform>,
      Flat<
        | { os: 'darwin'; weight?: number; format?: number }
        | { os: 'win32'; weight: number; format: number }
        | { os: 'linux'; weight: number; format?: string }
      >
    >
  >

  // Narrowing on `os` reaches the platform's own field.
  function use(platform: WeightPlatform) {
    if (platform.os === 'win32') {
      const format: number = platform.format
      return format
    }
    if (platform.os === 'linux') {
      const format: string | undefined = platform.format
      return format
    }
    // @ts-expect-error a CoreText platform has no `index`
    return platform.index
  }
  void use

  // A platform field one platform lacks is not on the others.
  const index = await getFamilies({ fields: ['faces.platform.index'] })
  type IndexPlatform = FaceOf<(typeof index)[number]>['platform']
  type _IndexOnLinuxOnly = Expect<Equal<Flat<IndexPlatform>, Flat<{ os: 'darwin' } | { os: 'win32' } | { os: 'linux'; index: number }>>>
}

// ---- the raw platform shapes carry exactly the fields of docs/api.md

type _Darwin = Expect<
  Equal<
    keyof DarwinNative,
    | 'os'
    | 'weight'
    | 'width'
    | 'slant'
    | 'symbolicTraits'
    | 'format'
    | 'priority'
    | 'registrationScope'
    | 'downloadable'
    | 'downloaded'
    | 'styleName'
    | 'displayName'
    | 'localizedFamilyName'
    | 'localizedStyleName'
    | 'fixedAdvance'
    | 'features'
    | 'metrics'
    | 'shadowed'
  >
>
type _Win32 = Expect<
  Equal<
    Exclude<keyof Win32Native, 'os'>,
    | 'weight'
    | 'width'
    | 'slant'
    | 'format'
    | 'typographicFamilyNames'
    | 'typographicStyleNames'
    | 'designLanguages'
    | 'supportedLanguages'
    | 'inactiveFamilyPreference'
    | 'sourceType'
    | 'sourceName'
    | 'axisValues'
    | 'resourceAxes'
    | 'additionalInformationalStrings'
  >
>
type _Linux = Expect<
  Equal<
    Exclude<keyof LinuxNative, 'os'>,
    | 'weight'
    | 'width'
    | 'slant'
    | 'index'
    | 'spacing'
    | 'format'
    | 'fontWrapper'
    | 'foundry'
    | 'decorative'
    | 'fontHasHint'
    | 'order'
    | 'outline'
    | 'scalable'
    | 'antialias'
    | 'pixelSize'
  >
>
