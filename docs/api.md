# API

system-font-info lists the font families installed on the machine, asking the operating system's own font service each time. The caller declares which fields it wants, one by one or through a preset, and gets exactly those fields back, typed precisely.

This document defines the functions, the options, every field and the rules the results obey. How each operating system provides each field, what it costs there and how quickly it sees font changes is in [platforms.md](platforms.md).

- [Functions](#functions)
- [Options](#options)
- [Field names](#field-names)
- [Presets](#presets)
- [Result typing](#result-typing)
- [Always included](#always-included)
- [Family fields](#family-fields)
- [Face fields](#face-fields)
- [OpenType entries](#opentype-entries)
- [Platform fields](#platform-fields)
- [Invariants](#invariants)
- [What is listed](#what-is-listed)
- [Freshness](#freshness)

## Functions

```ts
function getFamilies<const O extends GetFamiliesOptions>(options: O): Promise<Array<SelectedFamily<O>>>
function getFamily<const O extends GetFamilyOptions>(family: string, options: O): Promise<SelectedFamily<O> | undefined>
function pickName(names: readonly LocalizedString[], locales: readonly string[]): string | undefined
```

### `getFamilies(options)`

Lists the installed font families with the fields `options` selects. The families are sorted by `family`, compared as `family.toLowerCase()` in UTF-16 code units with ties broken by the raw string, so the order is the same on every machine and in every locale. The work runs on a worker thread, not on the JS thread. Every error, including an invalid option, is a rejection of the returned promise.

### `getFamily(family, options)`

Resolves to the family whose `family` is exactly `family`, or `undefined` when there is none. It is `getFamilies({ ...options, families: [family] })` and its first element, so it takes the same options except `families`, and the match is the same exact, case-sensitive comparison.

### `pickName(names, locales)`

Picks the entry of a `LocalizedString[]` (for example `familyNames` or `faces[].fullNames`) that best fits an ordered list of locales, and returns its `value`, or `undefined` when none fits. Locales are tried in order; a locale is tried at four match levels before the next locale is considered: the same canonical tag; the same tag after adding likely subtags (so `zh-CN` matches `zh-Hans-CN`); the same language and script; the same language. Entries whose language is `""` never match.

The default strings `fullName`, `styleName` and `axes[].name` are already chosen: `en-US`, else any `en` or `en-*`, else the entry without a language, else the first entry, else `""`.

## Options

| Option | Type | Meaning |
| --- | --- | --- |
| `fields` | `readonly Field[]` | The fields to return. See [Field names](#field-names). |
| `preset` | `Preset` | A named list of fields: `'names'`, `'styles'`, `'details'` or `'full'`. See [Presets](#presets). |
| `families` | `readonly string[]` | Return only the families whose `family` equals one of these strings exactly (case-sensitive, no trimming, no localized or legacy names). Names that match nothing are ignored; an empty array returns no families. Omitted: every family. Not accepted by `getFamily`. |
| `languageCoverage` | `readonly string[]` | Language tags to measure character coverage for; adds `languageCoverage` to every family. See [`LanguageCoverage`](#languagecoverage). |

`fields` and `preset` may be given together; the result has the union of both. **At least one of them is required.** The TypeScript types reject a call that gives neither, and a JavaScript call that gives neither rejects with a `TypeError`. `fields: []` is a valid declaration: it returns each family's `family` and nothing else.

Most interfaces that take a field list return everything when the list is omitted (Google's field masks, JSON:API sparse fieldsets). This one deliberately does not. The cheapest and the most expensive requests differ by one to two orders of magnitude: on the measured macOS machine the family list takes about 12 ms and the full result about 556 ms, on the measured Linux machine about 8 ms and 230 ms, the family list measured as family names alone ([platforms.md](platforms.md#measured-cost) has the conditions). A default would pick that cost for the caller without the caller seeing it in the code. Asking for everything costs one word: `preset: 'full'`.

The other runtime errors, all rejections:

| Condition | Error |
| --- | --- |
| `options` that is not an object | `TypeError` |
| A string in `fields` that is not a field name of any platform | `TypeError` naming it |
| `preset` that is not one of the four names | `TypeError` |
| `fields` that is not an array of strings | `TypeError` |
| `families` or `languageCoverage` that is not an array of strings | `TypeError` |
| `getFamily` given a `families` option (possible only from JavaScript; the types reject it) | `TypeError` |
| `getFamily` given a `family` that is not a string (possible only from JavaScript) | `TypeError` |
| A `languageCoverage` tag that [resolves to no orthography](#languagecoverage) | `RangeError` naming it |
| The operating system's font service fails | `Error` carrying the OS message |

## Field names

A field name is a path into the result:

| Form | Example | Selects |
| --- | --- | --- |
| `<field>` | `familyNames` | A family-level field. |
| `faces.<field>` | `faces.weight` | A face-level field, on every face. |
| `faces.opentype.<entry>` | `faces.opentype.os2` | One entry of the face's OpenType tables. |
| `faces.platform.<field>` | `faces.platform.features` | One platform field. |

- Selection stops at those levels: `faces.axes` selects the whole axis list, `faces.opentype.os2` the whole OS/2 object; deeper paths such as `faces.axes.min` are not field names.
- `family` and `faces` are not field names. `family` is always returned; `faces` is returned when any `faces.*` field is selected.
- `languageCoverage` is not a field name either: it takes a parameter, so it is the option of that name.
- `faces.coverage` and `languageCoverage` are in no preset and must be asked for by name.
- A field that the current platform does not provide is absent from the result and is not an error: `faces.platform.features` on Linux gives `platform: { os: 'linux' }`. The same code runs on every platform. A string that is a field name on no platform is an error.
- Listing a field twice, or listing a field the preset already contains, is allowed.

## Presets

A preset is a fixed list of fields, a shortcut for writing them in `fields`. Each preset contains the one before it. Their names describe what they contain; [platforms.md](platforms.md#source-groups) shows which kind of OS work each one reaches on each platform.

<!-- BEGIN GENERATED: presets (scripts/gen-fields.mjs) -->
| Preset | Contains |
| --- | --- |
| `names` | `familyNames` |
| `styles` | `names`, and `supportedLanguages`, `enabled`, `faces.postscriptName`, `faces.fullName`, `faces.fullNames`, `faces.styleName`, `faces.styleNames`, `faces.familyNames`, `faces.weight`, `faces.width`, `faces.style`, `faces.monospace`, `faces.color`, `faces.symbol`, `faces.enabled`, `faces.simulations`, `faces.supportedLanguages`, `faces.scope`, `faces.file`, `faces.coordinates` |
| `details` | `styles`, and `variable`, `faces.variable`, `faces.format`, `faces.axes`, `faces.namedInstanceIndex`, and every `faces.platform.<field>` |
| `full` | `details`, and every `faces.opentype.<entry>` |
<!-- END GENERATED: presets -->

`full` is every field except `faces.coverage` and `languageCoverage`. `details` is `full` without the OpenType tables.

## Result typing

The result type is exactly the selected fields. `FontFamily`, `FontFace`, `OpenTypeInfo` and the three platform types are the complete shapes defined by the tables in this document; the selection types narrow them.

<!-- BEGIN GENERATED: result-typing (scripts/gen-fields.mjs) -->
```ts
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
```
<!-- END GENERATED: result-typing -->

For example, `getFamilies({ fields: ['faces.weight', 'faces.platform.features'] })` is typed `Promise<Array<{ family: string; faces: Array<{ file?: …; coordinates?: …; simulations: …; weight: number; platform: { os: 'darwin'; features?: … } | { os: 'win32' } | { os: 'linux' } }> }>>`: the platform object is a union discriminated by `os`, and a platform field exists only on the member of the platform that defines it. When `fields` is a variable of type `Field[]` rather than a literal, the type falls back to every field.

`languageCoverage` (on the family) and `faces.coverage` are optional in the complete shapes because they are in no preset, but in a result type they are always present once asked for: the `languageCoverage` option adds a required `languageCoverage`, and selecting `faces.coverage` adds a required `coverage` to every face. Fields that can be absent for a given family or face (`file`, `axes`, `coordinates`, `namedInstanceIndex`, `opentype` entries) stay optional.

## Always included

- Every family has `family`.
- When any `faces.*` field is selected, every face has its identity: `file` (with `file.index`), `coordinates` and `simulations`, so faces can be told apart and matched with the faces of another call. An identity part a face does not have (`file`, `file.index`, `coordinates`) is absent, as in the full result.
- When any `faces.platform.*` field is selected, `platform` has `os`, the discriminant of its type.
- When any `faces.opentype.*` entry is selected, `opentype` has `errors` when a selected table failed to parse.

This rule is this package's own; field-selection interfaces such as JSON:API do not prescribe it.

## Family fields

| Field | Type | Definition |
| --- | --- | --- |
| `family` | `string` | The name the platform's font matcher groups the faces under: the name to write in CSS `font-family`. Unique among the families of a result, and always the `value` of one entry of `familyNames`. |
| `familyNames` | `LocalizedString[]` | The names the platform gives this family, in every language: the union of its faces' `familyNames`, in first-seen order, exact duplicates merged. Within each language the first entry is the platform's preferred name. |
| `supportedLanguages` | `string[]` | The union of the faces' `supportedLanguages`, in first-seen order. |
| `variable` | `boolean` | Some face is variable. |
| `enabled` | `boolean` | Some face is enabled. |
| `languageCoverage` | `LanguageCoverage[]` | Present only when the `languageCoverage` option is given: one entry per requested tag, in request order. |
| `faces` | `FontFace[]` | Present when any face field is selected. In the platform's own member order: the order the platform lists them. |

Each platform groups faces into families its own way (macOS by typographic family, Windows by weight/width/slope family, Linux by fontconfig's first family name), so the same fonts can form a different number of families on two platforms; [platforms.md](platforms.md#family-grouping) has the rule of each.

`familyNames` is what the platform says, not a list of names that work in CSS:

- macOS: the font's typographic family name (`name` ID 16, else ID 1, per language). Legacy names split by weight (`name` ID 1, such as `NanumGothic`) are not included, although CSS and CoreText accept many of them; some included localized names are not accepted by either.
- Windows: the family names DirectWrite's matcher recognizes.
- Linux: every family name fontconfig lists for the family, aliases included (legacy weight-split names such as `IBM Plex Sans Thin`). fontconfig tags English names `en` where the `name` table says `en-US`.

`pickName(familyNames, ['en'])` usually equals `family` but is not guaranteed to: a family without an English name has `pickName(..., ['en']) === undefined`, and a font whose Windows and Macintosh records carry different English names can give the other one.

### `LanguageCoverage`

| Field | Type | Definition |
| --- | --- | --- |
| `language` | `string` | The requested tag, canonicalized (`Intl.getCanonicalLocales`). |
| `orthography` | `string` | The identifier of the orthography the tag was counted against, such as `'zh-cn'`: its fc-lang name. |
| `required` | `number` | The number of code points in the language's orthography. |
| `covered` | `number` | How many of those code points the family's default face maps to a glyph. |

- **Default face**: the face the platform's matcher picks from this family for weight 400, normal width and normal style. It is one face, not the union of the family's faces.
- **Orthography**: fontconfig's orthography files (`fc-lang/*.orth`, fontconfig commit `a4e25ec391d417e4bca052fbfa5cd7ce5f7fd39e`), built into the package so every platform counts against the same sets. Every orthography in fontconfig's fc-lang set is available, identified by its fc-lang name. A set is exactly what its file lists, with the files it includes: the characters fontconfig considers needed to write the language, which are mostly letters. For example `zh-cn` has 6,765 code points (the 6,763 GB2312 hanzi and the tone marks U+02C7 and U+02C9), `zh-tw` 13,063, `ja` 2,314. It is not the CLDR exemplar set or Hyperglot's base set, which list different characters.
- **Matching a tag to an orthography**: a requested tag is resolved by trying, in order, (0) the tag itself, lowercased with `_` read as `-`, when that is exactly an fc-lang name (`sh`, `tl`, `zh_TW` → `zh-tw`); (a) the tag's own language and region, lowercased as fontconfig names its orthographies (`zh-HK` → `zh-hk`; any script is ignored); (b) the language and region of the tag after adding likely subtags with `Intl.Locale.prototype.maximize()`; (c) the language alone (`en-US` → `en`). The first that names an orthography is used. A tag that is not a well-formed BCP 47 tag (after `_` is read as `-`), and a tag none of the steps resolves, is a `RangeError` naming it. Step (0) makes every fc-lang name reachable by its own name, including those that `Intl` would rewrite into another tag (`sh`, `tl`, `mo`, `pap-AN`); `language` in the result is still the canonicalized tag, and `orthography` says which set the tag was counted against. The script does not choose the set, so a tag whose script differs from the language's default orthography is counted against the default: `sr-Latn` resolves to `sr`, fontconfig's Cyrillic set; to count against fontconfig's Latin Serbo-Croatian set, request `sh`.

  | Requested tag | Resolution | `orthography` |
  | --- | --- | --- |
  | `sh` | (0) `sh` | `sh` |
  | `zh_TW` | (0) `zh-tw` | `zh-tw` |
  | `zh-HK` | (a) `zh-hk` | `zh-hk` |
  | `zh-Hans` | no region; (b) `zh-Hans-CN` → `zh-cn` | `zh-cn` |
  | `zh-Hant` | no region; (b) `zh-Hant-TW` → `zh-tw` | `zh-tw` |
  | `zh` | no region; (b) `zh-Hans-CN` → `zh-cn` | `zh-cn` |
  | `pa-Arab` | no region; (b) `pa-Arab-PK` → `pa-pk` | `pa-pk` |
  | `en-US` | (a) and (b) give `en-us`, which is not an orthography; (c) `en` | `en` |
  | `sr-Latn` | no region; (b) `sr-Latn-RS` → `sr-rs`, which is not an orthography; (c) `sr` | `sr` |
- The counts say which code points the face maps, nothing about shaping or typographic quality, and whether a count is good enough is the caller's decision. That is why the field says coverage, not support.

## Face fields

### Names

| Field | Type | Definition |
| --- | --- | --- |
| `postscriptName` | `string` | The face's PostScript name as the platform reports it, instance-aware for variable fonts; `""` when it has none. |
| `fullNames` | `LocalizedString[]` | The face's full name, family and style together (OpenType `name` ID 4), in every language the platform gives. A named instance of a variable font has no full-name record of its own; where the platform does not name it, its names are composed from the typographic family name and the instance's subfamily name. |
| `fullName` | `string` | The default string of `fullNames` ([the rule](#picknamenames-locales)). |
| `styleNames` | `LocalizedString[]` | The face's style name in every language, such as `Bold Italic`. For a named instance, the instance's subfamily name. |
| `styleName` | `string` | The default string of `styleNames`. |
| `familyNames` | `LocalizedString[]` | The family names the platform associates with this face, in every language. |

`postscriptName` is not a key. CSS `local()` matches a face by its PostScript name or its full name, which one depends on the platform and the font. The WICG Local Font Access API and CSS treat the PostScript name as identifying one face; this package deliberately does not: on Windows several coordinates of one variable font share a name, and a font installed as two files gives two faces with one name. The identity of a face is described under [Invariants](#invariants).

`style` and `styleName` are different things that other interfaces name alike:

| This package | Same as | Not the same as |
| --- | --- | --- |
| `style` (`normal` / `italic` / `oblique`) | CSS `font-style`, DirectWrite `DWRITE_FONT_STYLE`, fontconfig `slant`, Skia `Slant` | WICG `FontData.style`, fontconfig `style` |
| `styleName` (`Bold Italic`) | WICG `FontData.style`, fontconfig `style`, CoreText `kCTFontStyleNameAttribute`, DirectWrite face name | CSS `font-style` |

Which `name` IDs `styleNames` comes from depends on the platform ([platforms.md](platforms.md)): macOS takes the typographic subfamily (ID 17, else ID 2), which differs from ID 2 for fonts like Arial Black.

### Classification

| Field | Type | Definition |
| --- | --- | --- |
| `weight` | `number` | CSS `font-weight`, 1-1000, the scale of OpenType `usWeightClass` and the `wght` axis. |
| `width` | `number` | CSS `font-width` (formerly `font-stretch`) percentage, 50-200, 100 being normal. Not OpenType's 1-9 `usWidthClass`, which is `opentype.os2.widthClass`. |
| `style` | `'normal' \| 'italic' \| 'oblique'` | The CSS `font-style` keyword. No oblique angle: that is `opentype.post.italicAngle` or `coordinates.slnt`. |
| `monospace` | `boolean` | The platform classifies the face as fixed-pitch. A classification the font declares or the OS makes, not a guarantee that every glyph has the same advance: CJK fonts commonly use two advances. The platform's answer is kept where it differs from `opentype.post.isFixedPitch`. |
| `color` | `boolean` | The platform reports that the face has color glyphs (COLR, sbix, CBDT or SVG). |
| `symbol` | `boolean` | The operating system treats the face as a symbol font. Whether the font has a symbol-encoded `cmap` subtable is in `opentype.cmap` on every platform. |

Per-platform meaning:

- `weight`: macOS reports multiples of 100, the weight a browser on macOS classifies the face as; Windows and Linux report the OS value unrounded (350 and 550 occur). The raw value is `platform.weight`.
- `width`: macOS is continuous (to a thousandth); Windows has nine steps (50, 62.5, 75, 87.5, 100, 112.5, 125, 150, 200); Linux reports fontconfig's value, whose steps for static fonts put semi-condensed at 87 and semi-expanded at 113. The raw value is `platform.width`.
- `style`: macOS never reports `oblique` (see [platforms.md](platforms.md#macos)).
- `symbol`: Windows and Linux report the OS's own flag; macOS has none, so a face is a symbol font there when its `cmap` has a Windows symbol (3,0) subtable.

### State and origin

| Field | Type | Definition |
| --- | --- | --- |
| `enabled` | `boolean` | `false` when the user turned the font off in the operating system (Font Book on macOS). `true` where the platform has no such state to list. |
| `simulations` | `{ bold: boolean; oblique: boolean }` | The styles the platform synthesizes for this face, when the platform lists synthesized faces as faces of their own (DirectWrite does). `false` on platforms that list none. |
| `supportedLanguages` | `string[]` | BCP 47 tags of the languages or scripts the platform says the face supports. A script without a language is written `und-` and the script (`und-Hans`). |
| `scope` | `'system' \| 'user' \| 'process' \| 'unknown'` | Whom the font is available to, by where it was registered: `system`, installed for the whole machine or with the OS; `user`, installed for the current user; `process`, registered for the current process only; `unknown`, the platform gives no answer (on macOS this includes every disabled font that was not registered for the process: CoreText no longer knows who installed it). |
| `format` | `'truetype' \| 'cff' \| 'cff2' \| 'type1' \| 'bitmap' \| 'unknown'` | The format of the glyph outlines: TrueType (`glyf`), CFF, CFF2, PostScript Type 1, bitmap only. |
| `file` | `{ path: string; index?: number }` (optional) | The font file, and the face's index inside a collection (TTC/OTC), 0 for a single-font file. `index` is absent when the platform cannot tell which face of the file this is. `file` is absent for a face that does not come from a local file; none has been seen on the platforms measured. |

- `enabled`: only macOS has a disabled state it can list. A disabled font is listed like any other, but on macOS asking for `enabled` or `scope` costs one more listing of the fonts (about 9 ms for the whole system, a fraction of a millisecond per family with `families`). A Windows font marked as hidden is still listed with `enabled: true`, and the raw registry entry is `platform.inactiveFamilyPreference`. A font fontconfig rejects is not listed on Linux at all.
- `simulations` are not CSS `font-synthesis`. CSS synthesis, like Skia's and Blink's, is a decision made while rendering when a requested style is missing; a simulation here is a face the platform enumerates. macOS synthesizes no listed faces, and fontconfig synthesizes bold and oblique only when matching, never in the font list.
- `supportedLanguages` is "supported" in the sense of OpenType `meta.slng` and DirectWrite's supported tags, not the design languages (`opentype.meta.dlng`). The three platforms' sources differ in kind, so the lists are not comparable across platforms: macOS gives CoreText's list of covered languages, Windows the script and language tags the font declares (often none), Linux the languages fontconfig computes from the face's character coverage.
- `scope` is not DirectWrite's locality (whether a font's data has been downloaded): only locally available fonts are listed at all. In normal use `process` occurs only on macOS. The test hooks register fonts for the current process on all three platforms, and those fonts have scope `process`.
- `format` is not a CSS `format()` hint: CSS treats `truetype` and `opentype` as the same hint and names containers (`woff2`, `collection`) that are not outline formats here. Do not copy it into an `@font-face` `src`.

### Variations

| Field | Type | Definition |
| --- | --- | --- |
| `variable` | `boolean` | The face comes from a font with variation axes (an `fvar` table). |
| `axes` | `VariationAxis[]` (optional) | The font's axes as its `fvar` table defines them; present when `variable`. Faces from one font share one array. |
| `coordinates` | `Record<string, number>` (optional) | This face's value on every axis, keyed by axis tag; present when `variable`. |
| `namedInstanceIndex` | `number` (optional) | The 0-based index into `opentype.fvar.instances` of the first instance record whose coordinates equal this face's `coordinates`, within the platform's coordinate precision. |

- Axis ranges and coordinates are in user-scale units (the values `fvar` stores, such as `wght` 100-900), not normalized coordinates.
- Every face is one fixed point of the design space: a variable font is listed as its default instance and its named instances, and its ranges are in `axes`.
- `namedInstanceIndex` is absent when the face's coordinates match no instance record; when several records have the same coordinates, it is the first. A default instance without a record of its own has none: OpenType counts the default as a named instance even then, but there is no index to give. The index is not the 1-based instance number FreeType and fontconfig pack into a face index (on Linux that raw index is `platform.index`).

#### `VariationAxis`

| Field | Type | Definition |
| --- | --- | --- |
| `tag` | `string` | The axis tag: `wght`, `wdth`, `opsz`, `slnt`, `ital` or a custom tag. |
| `name` | `string` | The default string of `names` (`""` when there are none). |
| `names` | `LocalizedString[]` | The axis name from the `fvar` axis's `axisNameID`, in every language. |
| `min`, `default`, `max` | `number` | The axis range and default, user scale. |
| `hidden` | `boolean` | The `fvar` `HIDDEN_AXIS` flag: the axis should not be shown in user interfaces. |

### Tables, coverage, platform

| Field | Type | Definition |
| --- | --- | --- |
| `opentype` | `OpenTypeInfo` (optional) | The selected entries of the font's own OpenType tables. See [OpenType entries](#opentype-entries). |
| `coverage` | `Array<[number, number]>` (optional) | Inclusive code point ranges the face maps, ascending. Present when `faces.coverage` is selected. The ranges are large. |
| `platform` | `DarwinNative \| Win32Native \| LinuxNative` | Raw platform values and platform-only information, discriminated by `os`. See [Platform fields](#platform-fields). |

### `LocalizedString`

| Field | Type | Definition |
| --- | --- | --- |
| `language` | `string` | Canonical BCP 47 tag (`Intl.getCanonicalLocales`), such as `zh-Hans`, `zh-CN`, `en-US`. `""` when the source gives none; a tag that cannot be canonicalized is kept as written. |
| `value` | `string` | The string. |

Localized arrays never contain two entries with the same `language` and `value`; nothing else is merged or filtered, so every language and every name a font carries is kept.

## OpenType entries

`opentype` holds the font's own tables, read by one parser on every platform, so the values are identical across platforms for the same font file. Values are as stored (font units, raw flags); only text, language identifiers and dates are decoded. "OpenType" here includes TrueType-flavoured sfnt fonts.

`opentype` is present on a face when some `faces.opentype.*` entry is selected and the face is an sfnt font whose tables the platform can read. An entry is present when it is selected and the font has that table. When `tables` or `names` is selected, it is present whenever `opentype` is. `opentype` is `{}` when the font has none of the selected tables and none of them failed to parse (`errors`, below, is otherwise present). Faces from one font share one `opentype` object.

| Entry | Field name | Type | Content |
| --- | --- | --- | --- |
| `tables` | `faces.opentype.tables` | `string[]` | The tag of every table in the font, ascending. |
| `names` | `faces.opentype.names` | `NameRecord[]` | Every record of the `name` table, in table order, nothing merged. |
| `head` | `faces.opentype.head` | `HeadTable` | The `head` table. |
| `hhea` | `faces.opentype.hhea` | `HorizontalHeaderTable` | The `hhea` table. |
| `vhea` | `faces.opentype.vhea` | `HorizontalHeaderTable` | The `vhea` table, in the same layout (`ascender` is `vertTypoAscender`, and so on). |
| `os2` | `faces.opentype.os2` | `Os2Table` | The `OS/2` table. |
| `post` | `faces.opentype.post` | `PostTable` | The `post` table header; no glyph names. |
| `maxp` | `faces.opentype.maxp` | `{ version: number; numGlyphs: number }` | The `maxp` table. |
| `cmap` | `faces.opentype.cmap` | `{ subtables: Array<{ platformId: number; encodingId: number; format: number }> }` | The list of `cmap` subtables, not the characters they map (that is `coverage`). |
| `fvar` | `faces.opentype.fvar` | `FvarTable` | The `fvar` axes and named instances, with their names decoded. |
| `meta` | `faces.opentype.meta` | `MetaTable` | The `meta` table's design and supported languages and its data map tags. |
| `layout` | `faces.opentype.layout` | `{ gsub?: LayoutTable; gpos?: LayoutTable }` | The script, language system and feature lists of `GSUB` and `GPOS`, and the UI names of stylistic sets and character variants; not the lookups. |

`errors` (`string[]`, optional) lists the tags of selected tables that are present but could not be parsed, ascending. A table that fails is left out and listed there. A malformed `name` table gives empty `names` (and empty `fvar` and layout names). `STAT` is not read.

```ts
export interface NameRecord {
  /** 0 Unicode, 1 Macintosh, 2 ISO, 3 Windows. */
  platformId: number
  encodingId: number
  /** Raw: an LCID (platform 3), a Mac language code (platform 1), or 0x8000+ for a language-tag record. */
  languageId: number
  /** 0 copyright, 1 family, 2 subfamily, 3 unique ID, 4 full name, 5 version, 6 PostScript name, ..., 16/17 typographic family/subfamily, 21/22 WWS family/subfamily, 256+ font-specific. */
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
```

## Platform fields

`platform` carries the operating system's raw values and the information only that platform has. A platform field holds what the OS API returns, so its definition is the API it names. Raw values of the unified weight, width, style and format share the names `weight`, `width`, `slant` and `format` on every platform; every other platform field uses the platform's own term. A value the OS does not provide for a face is absent, never filled with a default.

### macOS (`DarwinNative`)

| Field | Type | Definition |
| --- | --- | --- |
| `os` | `'darwin'` | The discriminant. |
| `weight` | `number` (optional) | `kCTFontWeightTrait`, -1...1. |
| `width` | `number` (optional) | `kCTFontWidthTrait`, -1...1. |
| `slant` | `number` (optional) | `kCTFontSlantTrait`, -1...1. Non-zero for some upright script faces too (Zapfino, Apple Chancery). |
| `symbolicTraits` | `number` (optional) | `kCTFontSymbolicTrait`, including the stylistic class bits. |
| `format` | `number` (optional) | `kCTFontFormatAttribute` (`CTFontFormat`: 0 unrecognized, 1 OpenType PostScript, 2 OpenType TrueType, 3 TrueType, 4 PostScript, 5 bitmap). |
| `priority` | `number` (optional) | `kCTFontPriorityAttribute`: 10000 system, 20000 network, 30000 computer, 40000 user, 50000 dynamic, 60000 process. For a disabled font CoreText makes the value up (60000), so it says nothing about the font. |
| `registrationScope` | `number` (optional) | `kCTFontRegistrationScopeAttribute` (`CTFontManagerScope`: 0 none, 1 process, 2 persistent, 3 session). For a disabled font CoreText makes the value up (2, except for a font registered for the process, which keeps 1), so it says nothing about the font. |
| `downloadable` | `boolean` (optional) | `kCTFontDownloadableAttribute`: the font is one of Apple's downloadable font assets. Only downloaded ones are listed. |
| `downloaded` | `boolean` (optional) | `kCTFontDownloadedAttribute`. |
| `styleName` | `string` (optional) | `kCTFontStyleNameAttribute`, not localized. |
| `displayName` | `string` (optional) | `kCTFontDisplayNameAttribute`, CoreText's composed full name, localized in the language the process first used CoreText in. |
| `localizedFamilyName` | `string` (optional) | `CTFontDescriptorCopyLocalizedAttribute(kCTFontFamilyNameAttribute)`, in the same language as `displayName`. |
| `localizedStyleName` | `string` (optional) | `CTFontDescriptorCopyLocalizedAttribute(kCTFontStyleNameAttribute)`, in the same language. |
| `fixedAdvance` | `number` (optional) | `kCTFontFixedAdvanceAttribute`, when set. |
| `features` | `Array<{ id: number; name: string; exclusive: boolean; openTypeTag?: string; selectors: Array<{ id: number; name: string; default: boolean; openTypeTag?: string; openTypeValue?: number }> }>` (optional) | `CTFontCopyFeatures`: the font's features as CoreText describes them, names in the process language. |
| `metrics` | `{ ascent: number; descent: number; leading: number; capHeight: number; xHeight: number; slantAngle: number; underlinePosition: number; underlineThickness: number; unitsPerEm: number; glyphCount: number; boundingBox: [number, number, number, number] }` (optional) | CoreText's metrics of the font (`CTFontGetAscent` and the like) scaled to font units; `slantAngle` in degrees; `boundingBox` is `[x, y, width, height]`. CoreText's values, which need not equal those of `hhea` or `OS/2`. |
| `shadowed` | `Array<{ path: string; variable: boolean }>` (optional) | Other font files the font collection lists under this face's PostScript name. Resolving the name (what CSS `local()` and family matching do) never reaches them, so they are not faces. |

The localized strings follow the process language, which on macOS is set when the process starts: the command-line arguments `-AppleLanguages '(en)'` placed after the script name change it; `LANG`, `LC_ALL` and `-AppleLocale` do not. `familyNames` and every other localized array are independent of it.

### Windows (`Win32Native`)

| Field | Type | Definition |
| --- | --- | --- |
| `os` | `'win32'` | The discriminant. |
| `weight` | `number` | `DWRITE_FONT_WEIGHT` (`IDWriteFont::GetWeight`), 1-999. |
| `width` | `number` | `DWRITE_FONT_STRETCH` (`IDWriteFont::GetStretch`), 1-9. |
| `slant` | `number` | `DWRITE_FONT_STYLE` (`IDWriteFont::GetStyle`): 0 normal, 1 oblique, 2 italic. |
| `format` | `number` | `DWRITE_FONT_FACE_TYPE`. |
| `typographicFamilyNames` | `LocalizedString[]` | Informational string `DWRITE_INFORMATIONAL_STRING_TYPOGRAPHIC_FAMILY_NAMES`: the family this face belongs to in DirectWrite's typographic family model. |
| `typographicStyleNames` | `LocalizedString[]` | Informational string `DWRITE_INFORMATIONAL_STRING_TYPOGRAPHIC_SUBFAMILY_NAMES`. |
| `designLanguages` | `LocalizedString[]` | Informational string `DWRITE_INFORMATIONAL_STRING_DESIGN_SCRIPT_LANGUAGE_TAG`, unsplit: the scripts and languages the font was designed for. |
| `supportedLanguages` | `LocalizedString[]` | Informational string `DWRITE_INFORMATIONAL_STRING_SUPPORTED_SCRIPT_LANGUAGE_TAG`, unsplit: the raw value of `supportedLanguages`. |
| `inactiveFamilyPreference` | `boolean` | The face's family name is listed under `HKCU\Software\Microsoft\Windows NT\CurrentVersion\Font Management\Inactive Fonts`, where Windows records fonts the user hid. A per-user preference recorded by name, which can be stale; DirectWrite lists the font regardless. |
| `sourceType` | `number` (optional) | `DWRITE_FONT_SOURCE_TYPE` of the face's entry in the system font set (`IDWriteFontSet3::GetFontSourceType`). |
| `sourceName` | `string` (optional) | The name of that source (`IDWriteFontSet3::GetFontSourceName`), when it has one. |
| `axisValues` | `Array<{ tag: string; value: number }>` (optional) | `IDWriteFontFaceReference1::GetFontAxisValues`: every axis value DirectWrite gives the face, including standard axes it supplies that the font's `fvar` does not define. |
| `resourceAxes` | `Array<{ tag: string; min: number; max: number; default: number; attributes: number; names: LocalizedString[] }>` (optional) | The axes of the font resource (`IDWriteFontResource`: `GetFontAxisRanges`, `GetDefaultFontAxisValues`, `GetFontAxisAttributes`, `GetAxisNames`), DirectWrite's view, which can include axes `fvar` does not define. |
| `additionalInformationalStrings` | `Record<string, LocalizedString[]>` | The informational strings DirectWrite returns that the font's `name` table does not reproduce, keyed by the `DWRITE_INFORMATIONAL_STRING_ID` value as a decimal string. |

### Linux (`LinuxNative`)

| Field | Type | Definition |
| --- | --- | --- |
| `os` | `'linux'` | The discriminant. |
| `weight` | `number` | `FC_WEIGHT`, fontconfig's scale 0-215. |
| `width` | `number` | `FC_WIDTH`, 50-200. |
| `slant` | `number` | `FC_SLANT`: 0 roman, 100 italic, 110 oblique. |
| `index` | `number` | `FC_INDEX` as fontconfig gives it: the face index in its file in the low 16 bits, the 1-based named instance number in the high 16 bits (0 for a face that is not a named instance). |
| `spacing` | `number` (optional) | `FC_SPACING`: 0 proportional, 90 dual-width, 100 monospace, 110 character cell. Absent when fontconfig gives none, which it does for most proportional fonts. |
| `format` | `string` (optional) | `FC_FONTFORMAT`, FreeType's format name (`TrueType`, `CFF`, `Type 1`, `PCF`, ...). |
| `fontWrapper` | `string` (optional) | `FC_FONT_WRAPPER`: the container (`SFNT`, `WOFF`, `WOFF2`, `CFF`). |
| `foundry` | `string` (optional) | `FC_FOUNDRY`: the foundry name fontconfig derives from the font's vendor ID or notices. |
| `decorative` | `boolean` (optional) | `FC_DECORATIVE`: fontconfig classifies the style as decorative. |
| `fontHasHint` | `boolean` (optional) | `FC_FONT_HAS_HINT`: the font contains hinting instructions. |
| `order` | `number` (optional) | `FC_ORDER`: the position of the font's directory in the configuration, which the matcher uses to rank otherwise equal fonts. |
| `outline` | `boolean` (optional) | `FC_OUTLINE`: the glyphs are outlines. |
| `scalable` | `boolean` (optional) | `FC_SCALABLE`: the glyphs can be scaled (true for outline and for scalable color bitmap fonts). |
| `antialias` | `boolean` (optional) | `FC_ANTIALIAS`, when the font or the configuration sets it. |
| `pixelSize` | `number` (optional) | `FC_PIXEL_SIZE`: the strike size of a bitmap font, when set. |

## Invariants

- **Projection.** Every result equals the full result projected onto the selected fields: the result of `preset: 'full'` (plus `faces.coverage` when selected) with the same `families` and `languageCoverage`, keeping `family`, the selected family fields, and on each face the identity and the selected face fields, the selected `opentype` entries (with `errors` limited to their tables) and the selected `platform` fields (with `os`). Every array keeps its order: families, faces, localized names, languages, axes. Asking for fewer fields never changes, reorders or adds a value.
- **`family` is unique** among the families of a result, and it is always the `value` of an entry of `familyNames`.
- **Face identity is unique within a family.** The identity of a face is `(file.path, file.index, coordinates, simulations)`; a part a face does not have counts as absent. Two faces of a family differ in at least one of them.
- **A repeated identity is one face.** When the OS reports one identity more than once (this occurs on Windows, see [platforms.md](platforms.md#windows)), the result holds the first of those members in the platform's member order and drops the others without an error, even if their other fields differ. Every other face the OS lists is returned; nothing is filtered or hidden. On macOS a PostScript name the font collection lists for several files is one face (the file the name resolves to); the other files are in `platform.shadowed`.
- **Shared objects stay shared.** Faces from one font file and index share one `opentype` object and one `axes` array. Do not mutate them unless the change is meant for every face.

## What is listed

Only fonts that the operating system lists through its public enumeration and whose data is on this machine are returned. Every platform lists only locally available fonts, so there is no field saying whether a font is local.

On macOS this leaves out two kinds of system fonts that apps can still use. Apple's Document-support fonts (Iowan Old Style, Times, Courier, Rockwell, Seravek, the first-generation STIX fonts and about a hundred Noto families for rare scripts, among others) are available to documents and apps that request them by name, and CSS accepts them, but CoreText marks them invisible and lists them nowhere. The system UI fonts (names beginning with `.`, such as the SF fonts) are not listed either; they are reached through the system font APIs and CSS `system-ui`, not by name. The package uses no private API and scans no directories to find either kind. [platforms.md](platforms.md#macos) has the details and the other platforms' scope.

## Freshness

The package runs nothing between calls: no background thread, no OS listener, no timer, and it keeps no cache of its own. Each call asks the operating system at that moment and keeps nothing, so it reflects the fonts the OS has registered when it is called.

The OS needs time to register a font that another process installs or removes. Measured: on Linux the next call sees it; on Windows about 1 second after the change; on macOS under a second to about 2.5 seconds in some series of changes and 4 to 8.5 seconds in others. None of the platforms promises an upper bound. [platforms.md](platforms.md) has the measurements.

On macOS, a process that does not run the main run loop (plain Node, `worker_threads`) never has CoreText's own caches invalidated, and two things can differ from what a new process reports until the process runs its main run loop or restarts. When several files share one PostScript name (a system update can leave two), the file reported for the face can differ from the one a new process reports; a file that has been removed is not reported for the face. Raw platform values such as `platform.priority` may be stale. Electron renderer and utility processes were not verified.

To follow font changes, call again at a moment that suits the application, for example when a window regains focus or a font picker opens. In the main process of an Electron app on macOS you can instead observe `kCTFontManagerRegisteredFontsChangedNotification` yourself. CoreText delivers it only on the main thread's run loop, which plain Node does not run, so this package cannot receive it without running an event loop of its own.
