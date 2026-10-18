// Public types of system-font-info. All three platforms return this one
// shape. Where a platform has no such information, an optional field is
// absent or a boolean carries the truthful value (for example macOS never
// synthesizes faces, so `simulations` is always false there); nothing is
// filled in with an invented default. docs/api.md defines every type here.

/** One string in one language. */
export interface LocalizedString {
  /** Canonical BCP 47 tag (Intl.getCanonicalLocales), e.g. "zh-Hans", "zh-CN", "en-US". "" when the source gives none; a tag that cannot be canonicalized is kept as written. */
  language: string
  value: string
}

/** One entry per requested `languageCoverage` tag, in request order. */
export interface LanguageCoverage {
  /** The requested tag, canonicalized (Intl.getCanonicalLocales). */
  language: string
  /** The fc-lang name of the orthography the tag was counted against, such as "zh-cn". */
  orthography: string
  /** The number of code points in the orthography. */
  required: number
  /** How many of those code points the family's default face maps to a glyph. */
  covered: number
}

/** The complete shape of a family; a call returns the part of it that its options select (`SelectedFamily`). */
export interface FontFamily {
  /** The name the platform matcher groups faces by; what goes into CSS `font-family`. Unique among the families of a result. */
  family: string
  /** Union of the faces' `familyNames`, in first-seen order, exact duplicates merged. Within each language the first entry is the platform's preferred name. */
  familyNames: LocalizedString[]
  /** Union of the faces' `supportedLanguages`, first-seen order. */
  supportedLanguages: string[]
  /** Any face is variable. */
  variable: boolean
  /** Any face is enabled. */
  enabled: boolean
  /** Present only when the `languageCoverage` option is given. */
  languageCoverage?: LanguageCoverage[]
  /** In the platform's own member order: the order the platform lists them. */
  faces: FontFace[]
}

/**
 * The complete shape of one face of a font family; a call returns the part of it that its options select (`SelectedFace`).
 *
 * The identity of a face is `(file.path, file.index, coordinates, simulations)`. When the platform reports one
 * identity more than once, the first in its member order is kept and the others are dropped, even if their other
 * fields differ; every other face it lists is returned, so two faces of a family differ in at least one of those four fields. A part a face does not have (`file`,
 * `file.index`, `coordinates`) counts as absent in the comparison.
 */
export interface FontFace {
  // ---- names

  /** What CSS `local()` resolves. "" when the face has none. Not unique and not a key: on Windows several coordinates of one variable font can share a name, and a font installed as two files gives two faces with one name. */
  postscriptName: string
  /** Default string of `fullNames`: en-US, else any en-*, else language "", else the first. */
  fullName: string
  fullNames: LocalizedString[]
  /** Default string of `styleNames`, same rule as `fullName`. */
  styleName: string
  styleNames: LocalizedString[]
  /** Every family name, in every language, the platform matcher associates with this face. */
  familyNames: LocalizedString[]

  // ---- classification, same scale on every platform

  /** CSS font-weight, 1-1000. */
  weight: number
  /** CSS font-stretch percentage, 50-200 (100 = normal). */
  width: number
  style: 'normal' | 'italic' | 'oblique'
  monospace: boolean
  /** The platform reports color glyphs (COLR, sbix, CBDT or SVG). */
  color: boolean
  /** The operating system treats the face as a symbol font. */
  symbol: boolean

  // ---- state and origin

  /** False when the user disabled the font (macOS Font Book). True where the platform has no such state. */
  enabled: boolean
  /** Styles the platform synthesizes for this face (Windows); false on other platforms. */
  simulations: { bold: boolean; oblique: boolean }
  /** BCP 47 tags of the languages or scripts the platform says the face supports. */
  supportedLanguages: string[]
  /** Whom the font is available to, by where it was registered. */
  scope: 'system' | 'user' | 'process' | 'unknown'
  /** The format of the glyph outlines. */
  format: 'truetype' | 'cff' | 'cff2' | 'type1' | 'bitmap' | 'unknown'
  /** Absent for fonts that do not come from a file. */
  file?: {
    path: string
    /** Face index inside a collection (TTC/OTC); 0 for a single-font file. Absent when the platform cannot tell which face of the file this is. */
    index?: number
  }

  // ---- variations

  /** The face comes from a font with variation axes. */
  variable: boolean
  /** Present when variable. */
  axes?: VariationAxis[]
  /** This face's value on every axis, keyed by axis tag; present when variable. */
  coordinates?: Record<string, number>
  /** Index into `opentype.fvar.instances` of the first instance whose coordinates equal this face's; absent when there is none. */
  namedInstanceIndex?: number

  // ---- the font's own tables, and coverage

  /** The selected entries of the font's OpenType tables; present when some `faces.opentype.*` entry is selected and the face is an sfnt. */
  opentype?: OpenTypeInfo
  /** Inclusive code point ranges, ascending. Present when `faces.coverage` is selected. */
  coverage?: Array<[number, number]>

  /** Raw platform values and platform-only information, discriminated by `os`. */
  platform: DarwinNative | Win32Native | LinuxNative
}

export interface VariationAxis {
  /** "wght", "wdth", "opsz", "slnt", "ital" or a custom tag. */
  tag: string
  /** Default string of `names` ("" when there are none), same rule as `FontFace.fullName`. */
  name: string
  /** From the fvar axisNameID. */
  names: LocalizedString[]
  min: number
  max: number
  default: number
  hidden: boolean
}

// ---- OpenType tables -------------------------------------------------------
// Values are as stored in the font (font units, raw flags); only text and
// language identifiers are decoded.

export interface OpenTypeInfo {
  /** Tags of every table in the font, ascending. */
  tables: string[]
  /** Every record of the name table in table order; nothing merged. */
  names: NameRecord[]
  /** Tags of tables that are present but could not be parsed. */
  errors?: string[]
  head?: HeadTable
  hhea?: HorizontalHeaderTable
  vhea?: HorizontalHeaderTable
  os2?: Os2Table
  post?: PostTable
  maxp?: { version: number; numGlyphs: number }
  cmap?: { subtables: Array<{ platformId: number; encodingId: number; format: number }> }
  fvar?: FvarTable
  meta?: MetaTable
  layout?: { gsub?: LayoutTable; gpos?: LayoutTable }
}

export interface NameRecord {
  /** 0 Unicode, 1 Macintosh, 2 ISO, 3 Windows. */
  platformId: number
  encodingId: number
  /** Raw: an LCID (platform 3), a Mac language code (platform 1), or 0x8000+ for a language-tag record. */
  languageId: number
  /** OpenType nameID: 0 copyright, 1 family, 2 subfamily, 3 unique ID, 4 full name, 5 version, 6 PostScript name, ..., 16/17 typographic family/subfamily, 21/22 WWS family/subfamily, 256+ font-specific. */
  nameId: number
  /** Canonical BCP 47 mapped from `languageId`; "" when no mapping exists. */
  language: string
  /** Decoded text; absent when the encoding cannot be decoded. */
  value?: string
  /** Hex of the raw bytes; present only when `value` is absent. */
  bytes?: string
}

export interface HeadTable {
  version: number
  fontRevision: number
  flags: number
  unitsPerEm: number
  /** ISO 8601 (UTC), converted from the stored LONGDATETIME as it is. Absent when the year falls outside 0001-9999. */
  created?: string
  /** ISO 8601 (UTC), same rule as `created`. */
  modified?: string
  /** [xMin, yMin, xMax, yMax] */
  boundingBox: [number, number, number, number]
  macStyle: number
  lowestRecPPEM: number
  fontDirectionHint: number
  indexToLocFormat: number
  glyphDataFormat: number
}

/** hhea, and vhea with the same field layout (ascender/descender/lineGap are vertTypoAscender/... there). */
export interface HorizontalHeaderTable {
  version: number
  ascender: number
  descender: number
  lineGap: number
  advanceMax: number
  minLeadingBearing: number
  minTrailingBearing: number
  maxExtent: number
  caretSlopeRise: number
  caretSlopeRun: number
  caretOffset: number
  metricDataFormat: number
  numberOfMetrics: number
}

export interface Os2Table {
  version: number
  xAvgCharWidth: number
  weightClass: number
  widthClass: number
  fsType: number
  subscript: { xSize: number; ySize: number; xOffset: number; yOffset: number }
  superscript: { xSize: number; ySize: number; xOffset: number; yOffset: number }
  strikeout: { size: number; position: number }
  familyClass: number
  /** 10 bytes. */
  panose: number[]
  unicodeRanges: [number, number, number, number]
  vendorId: string
  fsSelection: number
  firstCharIndex: number
  lastCharIndex: number
  typoAscender: number
  typoDescender: number
  typoLineGap: number
  winAscent: number
  winDescent: number
  /** Version 1+. */
  codePageRanges?: [number, number]
  /** Version 2+. */
  xHeight?: number
  capHeight?: number
  defaultChar?: number
  breakChar?: number
  maxContext?: number
  /** Version 5+, in points. */
  lowerOpticalPointSize?: number
  upperOpticalPointSize?: number
}

export interface PostTable {
  version: number
  italicAngle: number
  underlinePosition: number
  underlineThickness: number
  isFixedPitch: boolean
}

export interface FvarTable {
  axes: Array<{ tag: string; min: number; default: number; max: number; flags: number; nameId: number; names: LocalizedString[] }>
  instances: Array<{
    subfamilyNameId: number
    subfamilyNames: LocalizedString[]
    flags: number
    coordinates: Record<string, number>
    postscriptNameId?: number
    postscriptName?: string
  }>
}

export interface MetaTable {
  /** Design languages (ScriptLangTag list). */
  dlng?: string[]
  /** Supported languages (ScriptLangTag list). */
  slng?: string[]
  /** Tags of every data map in the table. */
  tags: string[]
}

export interface LayoutTable {
  /** script tag -> language system tag ("dflt" for the default one) -> feature tags (the required feature first, when there is one). */
  scripts: Record<string, Record<string, string[]>>
  /** Every distinct feature tag in the FeatureList, in table order. */
  features: string[]
  /** UI names of stylistic sets (ssXX) and character variants (cvXX) that declare one. */
  featureNames?: Record<string, LocalizedString[]>
}

// ---- platform-specific -------------------------------------------------------

/** CoreText's raw values. A value CoreText does not provide for a face is absent, never a default. */
export interface DarwinNative {
  os: 'darwin'
  /** kCTFontWeightTrait, -1...1 */
  weight?: number
  /** kCTFontWidthTrait, -1...1 */
  width?: number
  /** kCTFontSlantTrait, -1...1 */
  slant?: number
  /** kCTFontSymbolicTrait, including the class bits. */
  symbolicTraits?: number
  /** kCTFontFormatAttribute (CTFontFormat: 0 unrecognized, 1 OpenType PostScript, 2 OpenType TrueType, 3 TrueType, 4 PostScript, 5 bitmap). */
  format?: number
  /** kCTFontPriorityAttribute (system 10000 ... process 60000). CoreText makes the value up for a disabled font. */
  priority?: number
  /** kCTFontRegistrationScopeAttribute. CoreText makes the value up for a disabled font. */
  registrationScope?: number
  /** kCTFontDownloadableAttribute */
  downloadable?: boolean
  /** kCTFontDownloadedAttribute */
  downloaded?: boolean
  /** kCTFontStyleNameAttribute (not localized) */
  styleName?: string
  /** kCTFontDisplayNameAttribute, localized in the language the process first used CoreText in. */
  displayName?: string
  /** CTFontDescriptorCopyLocalizedAttribute(kCTFontFamilyNameAttribute), same language caveat. */
  localizedFamilyName?: string
  /** CTFontDescriptorCopyLocalizedAttribute(kCTFontStyleNameAttribute), same language caveat. */
  localizedStyleName?: string
  /** kCTFontFixedAdvanceAttribute, when set. */
  fixedAdvance?: number
  /** CTFontCopyFeatures. Names are in the process language. */
  features?: Array<{
    id: number
    name: string
    exclusive: boolean
    openTypeTag?: string
    selectors: Array<{ id: number; name: string; default: boolean; openTypeTag?: string; openTypeValue?: number }>
  }>
  /** CTFont metrics in font units. */
  metrics?: {
    ascent: number
    descent: number
    leading: number
    capHeight: number
    xHeight: number
    slantAngle: number
    underlinePosition: number
    underlineThickness: number
    unitsPerEm: number
    glyphCount: number
    /** [x, y, width, height] */
    boundingBox: [number, number, number, number]
  }
  /** Other font files the font collection lists under this face's PostScript name. Resolving the name (what CSS local() and family matching do) never reaches them, so they are not faces. */
  shadowed?: Array<{ path: string; variable: boolean }>
}

/** DirectWrite's raw values. */
export interface Win32Native {
  os: 'win32'
  /** DWRITE_FONT_WEIGHT (IDWriteFont::GetWeight), 1-999 */
  weight: number
  /** DWRITE_FONT_STRETCH (IDWriteFont::GetStretch), 1-9 */
  width: number
  /** DWRITE_FONT_STYLE (IDWriteFont::GetStyle): 0 normal, 1 oblique, 2 italic */
  slant: number
  /** DWRITE_FONT_FACE_TYPE */
  format: number
  /** DWRITE_INFORMATIONAL_STRING_TYPOGRAPHIC_FAMILY_NAMES */
  typographicFamilyNames: LocalizedString[]
  /** DWRITE_INFORMATIONAL_STRING_TYPOGRAPHIC_SUBFAMILY_NAMES */
  typographicStyleNames: LocalizedString[]
  /** DWRITE_INFORMATIONAL_STRING_DESIGN_SCRIPT_LANGUAGE_TAG, unsplit. */
  designLanguages: LocalizedString[]
  /** DWRITE_INFORMATIONAL_STRING_SUPPORTED_SCRIPT_LANGUAGE_TAG, unsplit: the raw value of `supportedLanguages`. */
  supportedLanguages: LocalizedString[]
  /** The face's family name is listed under HKCU\Software\Microsoft\Windows NT\CurrentVersion\Font Management\Inactive Fonts. A per-user preference recorded by name, which can be stale; DirectWrite lists the font regardless. */
  inactiveFamilyPreference: boolean
  /** DWRITE_FONT_SOURCE_TYPE of the face's entry in the system font set (IDWriteFontSet3::GetFontSourceType). */
  sourceType?: number
  /** The name of that source (IDWriteFontSet3::GetFontSourceName), when it has one. */
  sourceName?: string
  /** IDWriteFontFaceReference1::GetFontAxisValues: every axis value DirectWrite gives the face. */
  axisValues?: Array<{ tag: string; value: number }>
  /** The axes of the font resource (IDWriteFontResource), DirectWrite's view, which can include axes `fvar` does not define. */
  resourceAxes?: Array<{ tag: string; min: number; max: number; default: number; attributes: number; names: LocalizedString[] }>
  /** The informational strings DirectWrite returns that the font's `name` table does not reproduce, keyed by the DWRITE_INFORMATIONAL_STRING_ID value as a decimal string. */
  additionalInformationalStrings: Record<string, LocalizedString[]>
}

/** fontconfig's raw values. Not final: the Linux backend is not implemented yet. */
export interface LinuxNative {
  os: 'linux'
  /** FC_WEIGHT, 0-215 */
  weight: number
  /** FC_WIDTH, 50-200 */
  width: number
  /** FC_SLANT: 0 roman, 100 italic, 110 oblique */
  slant: number
  /** FC_INDEX: the face index in its file in the low 16 bits, the 1-based named instance number in the high 16 bits. */
  index: number
  /** FC_SPACING: 0 proportional, 90 dual-width, 100 monospace, 110 character cell. Absent when fontconfig gives none. */
  spacing?: number
  /** FC_FONTFORMAT, FreeType's format name (TrueType, CFF, Type 1, PCF, ...). */
  format?: string
  /** FC_FONT_WRAPPER: the container (SFNT, WOFF, WOFF2, CFF). Needs fontconfig 2.15 at run time. */
  fontWrapper?: string
  /** FC_FOUNDRY */
  foundry?: string
  /** FC_DECORATIVE */
  decorative?: boolean
  /** FC_FONT_HAS_HINT. Needs fontconfig 2.14 at run time. */
  fontHasHint?: boolean
  /** FC_ORDER: the position of the font's directory in the configuration. Needs fontconfig 2.14 at run time. */
  order?: number
  /** FC_OUTLINE */
  outline?: boolean
  /** FC_SCALABLE */
  scalable?: boolean
  /** FC_ANTIALIAS, when the font or the configuration sets it. */
  antialias?: boolean
  /** FC_PIXEL_SIZE: the strike size of a bitmap font, when set. */
  pixelSize?: number
}

// ---- field selection -----------------------------------------------------------
// The text between the region markers is copied verbatim into docs/api.md ("Result typing").

// #region selection-types
export type Preset = 'names' | 'styles' | 'details' | 'full'

export type FamilyField = 'familyNames' | 'supportedLanguages' | 'variable' | 'enabled'
export type OpenTypeEntry = 'tables' | 'names' | 'head' | 'hhea' | 'vhea' | 'os2' | 'post' | 'maxp' | 'cmap' | 'fvar' | 'meta' | 'layout'
export type PlatformNative = DarwinNative | Win32Native | LinuxNative
export type PlatformField = Exclude<keyof DarwinNative | keyof Win32Native | keyof LinuxNative, 'os'>
type FaceKey = Exclude<keyof FontFace, 'opentype' | 'platform'>
export type FaceField = `faces.${FaceKey}` | `faces.opentype.${OpenTypeEntry}` | `faces.platform.${PlatformField}`
export type Field = FamilyField | FaceField

type NamesFields = 'familyNames'
type StylesFields =
  | NamesFields
  | 'supportedLanguages'
  | 'enabled'
  | `faces.${'postscriptName' | 'fullName' | 'fullNames' | 'styleName' | 'styleNames' | 'familyNames'}`
  | `faces.${'weight' | 'width' | 'style' | 'monospace' | 'color' | 'symbol'}`
  | `faces.${'enabled' | 'simulations' | 'supportedLanguages' | 'scope' | 'file' | 'coordinates'}`
type DetailsFields =
  | StylesFields
  | 'variable'
  | `faces.${'variable' | 'format' | 'axes' | 'namedInstanceIndex'}`
  | `faces.platform.${PlatformField}`
type FullFields = DetailsFields | `faces.opentype.${OpenTypeEntry}`
export interface PresetFields {
  names: NamesFields
  styles: StylesFields
  details: DetailsFields
  full: FullFields
}

type Selection = { fields: readonly Field[]; preset?: Preset } | { fields?: readonly Field[]; preset: Preset }
export type GetFamiliesOptions = Selection & { families?: readonly string[]; languageCoverage?: readonly string[] }
export type GetFamilyOptions = Selection & { families?: never; languageCoverage?: readonly string[] }

type SelectedFields<O> =
  | (O extends { fields: readonly (infer F)[] } ? F : never)
  | (O extends { preset: infer P extends Preset } ? PresetFields[P] : never)

type Identity = 'file' | 'coordinates' | 'simulations'
type FaceKeysOf<F> = F extends `faces.${infer K extends FaceKey}` ? K : never
type OpenTypeEntriesOf<F> = F extends `faces.opentype.${infer E extends OpenTypeEntry}` ? E : never
type PlatformFieldsOf<F> = F extends `faces.platform.${infer K extends PlatformField}` ? K : never
type PickPlatform<K, N = PlatformNative> = N extends PlatformNative ? Pick<N, 'os' | (K & keyof N)> : never

export type SelectedFace<F> = Pick<FontFace, Identity | Exclude<FaceKeysOf<F>, 'coverage'>> &
  ('coverage' extends FaceKeysOf<F> ? Required<Pick<FontFace, 'coverage'>> : unknown) &
  ([OpenTypeEntriesOf<F>] extends [never] ? unknown : { opentype?: Pick<OpenTypeInfo, OpenTypeEntriesOf<F> | 'errors'> }) &
  ([PlatformFieldsOf<F>] extends [never] ? unknown : { platform: PickPlatform<PlatformFieldsOf<F>> })

export type SelectedFamily<O> = Pick<FontFamily, 'family' | (SelectedFields<O> & FamilyField)> &
  ([Extract<SelectedFields<O>, `faces.${string}`>] extends [never]
    ? unknown
    : { faces: Array<SelectedFace<Extract<SelectedFields<O>, `faces.${string}`>>> }) &
  (O extends { languageCoverage: readonly string[] } ? Required<Pick<FontFamily, 'languageCoverage'>> : unknown)
// #endregion
