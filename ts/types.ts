// Public types of system-font-info. All three platforms return this one
// shape. Where a platform has no such information, an optional field is
// absent or a boolean carries the truthful value (for example macOS never
// synthesizes faces, so `simulations` is always false there); nothing is
// filled in with an invented default.

/** One string in one language. */
export interface LocalizedString {
  /** Canonical BCP 47 tag (Intl.getCanonicalLocales), e.g. "zh-Hans", "zh-CN", "en-US". "" when the source gives none; a tag that cannot be canonicalized is kept as written. */
  language: string
  value: string
}

export interface GetFontsOptions {
  /** Read the standard OpenType tables of every face into `FontFace.opentype`. Default true. */
  tables?: boolean
  /** Include `FontFace.coverage`. Default false: the ranges are large. */
  coverage?: boolean
}

export interface FontFamily {
  /** The name the platform matcher groups faces by; what goes into CSS `font-family`. */
  family: string
  /** Union of the faces' `familyNames`, source order, exact duplicates merged. */
  familyNames: LocalizedString[]
  /** Union of the faces' `languages`, first-seen order. */
  languages: string[]
  /** Any face is variable. */
  variable: boolean
  /** Any face is enabled. */
  enabled: boolean
  /** Every face is hidden. */
  hidden: boolean
  /** In the platform's own member order (the order a browser on that platform walks them). */
  faces: FontFace[]
}

export interface FontFace {
  // ---- identity, as the platform reports this face (instance-aware for variable fonts)

  /** What CSS `local()` resolves. "" when the face has none. `(postscriptName, simulations)` is unique across the result. */
  postscriptName: string
  /** Default string of `fullNames`: en-US, else any en-*, else language "", else the first. */
  fullName: string
  fullNames: LocalizedString[]
  /** Default string of `styleNames`, same rule as `fullName`. */
  styleName: string
  styleNames: LocalizedString[]
  /** Every family name, in every language, the platform matcher associates with this face. */
  familyNames: LocalizedString[]

  // ---- normalized, same scale on every platform

  /** CSS font-weight, 1-1000. */
  weight: number
  /** CSS font-stretch percentage, 50-200 (100 = normal). */
  width: number
  style: 'normal' | 'italic' | 'oblique'
  monospace: boolean
  /** The face has color glyphs (COLR, sbix, CBDT or SVG). */
  color: boolean
  /** The cmap has a symbol-encoded (3,0) subtable. */
  symbol: boolean
  /** False when the user disabled the font (macOS Font Book). True where the platform has no such state. */
  enabled: boolean
  /** Installed and usable, but the platform hides it from font lists. */
  hidden: boolean
  /** Styles the platform synthesizes for this face (Windows); false on other platforms. */
  simulations: { bold: boolean; oblique: boolean }
  /** BCP 47 tags the platform says the face supports. macOS: declared by the font (kCTFontLanguagesAttribute). Windows: DirectWrite's supported script/language tags. Linux: fontconfig's FC_LANG, computed from character coverage. */
  languages: string[]
  /** Who installed or registered the font. */
  scope: 'system' | 'user' | 'process' | 'unknown'
  format: 'truetype' | 'cff' | 'cff2' | 'type1' | 'bitmap' | 'unknown'
  /** Absent for fonts that do not come from a file. */
  file?: {
    path: string
    /** Face index inside a collection (TTC/OTC); 0 for a single-font file. */
    index: number
  }

  // ---- variations

  /** The face comes from a font with variation axes. */
  variable: boolean
  /** Present when variable. */
  axes?: VariationAxis[]
  /** This face's value on every axis; present when variable. */
  coordinates?: Record<string, number>
  /** Index into `opentype.fvar.instances` this face corresponds to; absent when it is not a named instance. */
  namedInstance?: number

  // ---- the font's own tables, and coverage

  /** Present when `tables` is on and the face is an sfnt. */
  opentype?: OpenTypeInfo
  /** Inclusive code point ranges, ascending. Present when `coverage` is on. */
  coverage?: Array<[number, number]>

  /** Raw platform values and platform-only information. */
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
  /** ISO 8601 (UTC). */
  created: string
  /** ISO 8601 (UTC). */
  modified: string
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

export interface DarwinNative {
  os: 'darwin'
  /** kCTFontWeightTrait, -1...1 */
  weight: number
  /** kCTFontWidthTrait, -1...1 */
  width: number
  /** kCTFontSlantTrait, -1...1 */
  slant: number
  /** kCTFontSymbolicTrait, including the class bits. */
  symbolicTraits: number
  /** kCTFontFormatAttribute */
  format: number
  /** kCTFontPriorityAttribute (system 10000 ... process 60000) */
  priority: number
  /** kCTFontRegistrationScopeAttribute */
  registrationScope: number
  /** kCTFontDownloadableAttribute */
  downloadable: boolean
  /** kCTFontDownloadedAttribute */
  downloaded: boolean
  /** kCTFontStyleNameAttribute (not localized) */
  styleName: string
  /** kCTFontDisplayNameAttribute, localized in the language the process first used CoreText in. */
  displayName: string
  /** CTFontDescriptorCopyLocalizedAttribute(kCTFontFamilyNameAttribute), same language caveat. */
  localizedFamilyName: string
  /** CTFontDescriptorCopyLocalizedAttribute(kCTFontStyleNameAttribute), same language caveat. */
  localizedStyleName: string
  /** kCTFontFixedAdvanceAttribute, when set. */
  fixedAdvance?: number
  /** kCTFontFeaturesAttribute. Names are in the process language. */
  features?: Array<{
    id: number
    name: string
    exclusive: boolean
    openTypeTag?: string
    selectors: Array<{ id: number; name: string; default: boolean; openTypeTag?: string; openTypeValue?: number }>
  }>
  /** NSFontManager availableMembersOfFontFamily: weight, 0-15. */
  appKitWeight?: number
  /** NSFontManager availableMembersOfFontFamily: NSFontTraitMask. */
  appKitTraits?: number
  /** CTFont metrics in font units. Present when `tables` is on (it needs a CTFont). */
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
  /** Other font files the font collection lists under this PostScript name. Resolving the name (what CSS local() and family matching do) never reaches them. */
  shadowed?: Array<{ path: string; variable: boolean }>
}

/** Not final: the Windows backend is not implemented yet. */
export interface Win32Native {
  os: 'win32'
  /** DWRITE_FONT_WEIGHT */
  weight: number
  /** DWRITE_FONT_STRETCH, 1-9 */
  width: number
  /** DWRITE_FONT_STYLE: 0 normal, 1 oblique, 2 italic */
  slant: number
  /** DWRITE_FONT_SIMULATIONS bits */
  simulations: number
  /** DWRITE_FONT_FACE_TYPE */
  faceType: number
}

/** Not final: the Linux backend is not implemented yet. */
export interface LinuxNative {
  os: 'linux'
  /** FC_WEIGHT, 0-215 */
  weight: number
  /** FC_WIDTH, 50-200 */
  width: number
  /** FC_SLANT: 0, 100, 110 */
  slant: number
  /** Raw FC_INDEX (named instance in the high 16 bits). */
  index: number
}
