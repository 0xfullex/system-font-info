# Platforms

How each operating system provides the fields that [api.md](api.md) defines: the OS API behind every field (the API defines what the value means), the kind of work it costs, how each platform forms families, what it lists, how quickly it sees font changes, and what calls cost on the machines measured.

| Platform | Font service | Status |
| --- | --- | --- |
| macOS | CoreText | implemented |
| Windows | DirectWrite | in development |
| Linux | fontconfig, with FreeType for WOFF tables | in development |

For Windows and Linux the sources below are the ones the backends are built on, and the measured figures come from standalone implementations of those same sources. Where a platform's behaviour has not been established, this document says it is not yet documented.

- [Source groups](#source-groups)
- [Family grouping](#family-grouping)
- [macOS](#macos)
- [Windows](#windows)
- [Linux](#linux)
- [Measured cost](#measured-cost)

## Source groups

Every field belongs, on each platform, to one group of OS work. A call does the union of the work of its fields' groups, and within a group most of the cost is paid once: the first C field opens every font resource, after which another C field adds little (on Windows, C covers two kinds of work whose costs have not been measured separately; see below).

| Group | Work |
| --- | --- |
| A | The family list: the OS's list of families and their names, no work per face. |
| B | Per-face OS attributes: one OS query per face (on Linux, more objects in the same `FcFontList` call). |
| C | Opening each font resource: reading its `name`, `fvar` and `cmap` tables and its table directory (on macOS, creating a `CTFont`). |
| D | Reading and parsing the OpenType tables; each `faces.opentype` entry is its own table. |
| E | Character coverage: the family's default face against an orthography (`languageCoverage`), or every face's code point ranges (`faces.coverage`). |
| constant | No OS work: the platform has no such notion and the value is fixed. |
| derived | Computed by the package from another field, which it fetches: `fullName`, `styleName`, `axes[].name`, and the family-level aggregates. |

### The group of every field

The group of OS work each field needs on each platform. A cell is read as follows:

- `B, C` is both groups, always. ``B; C for `cff2` `` is B always and C in the case named; `C, for variable faces only` is C in that case alone.
- `constant` and `derived` are the two kinds of field in the table above. A `derived` cell costs what the fields it is computed from cost on that platform: a family-level field the face field of the same name (`variable` from `faces.variable`; `familyNames` too, except on Windows, where DirectWrite gives a family's names directly and the field is group A), `faces.fullName` the field `faces.fullNames`, and `faces.styleName` the field `faces.styleNames`.
- `not yet documented`: the platform provides the field and its source has not been established.
- `—`: the platform does not provide the field.

`family` and the `languageCoverage` option are not field names and have no row: they are groups A and E.

<!-- BEGIN GENERATED: field-groups (scripts/gen-fields.mjs) -->
| Field | macOS | Windows | Linux |
| --- | --- | --- | --- |
| `familyNames` | derived | A | derived |
| `supportedLanguages` | derived | derived | derived |
| `variable` | derived | derived | derived |
| `enabled` | derived | derived | derived |
| `faces.postscriptName` | B | B | B |
| `faces.fullName` | derived | derived | derived |
| `faces.fullNames` | C | B | B; C for named instances |
| `faces.styleName` | derived | derived | derived |
| `faces.styleNames` | C | B | B; C for named instances |
| `faces.familyNames` | C | A | A |
| `faces.weight` | B | B | B |
| `faces.width` | B | B | B |
| `faces.style` | B | B | B |
| `faces.monospace` | B | B | B |
| `faces.color` | B | B | B |
| `faces.symbol` | C | B | B |
| `faces.enabled` | B | constant | constant |
| `faces.simulations` | constant | B | constant |
| `faces.supportedLanguages` | B | B | B |
| `faces.scope` | B | B | B |
| `faces.file` | B, C | B | B |
| `faces.variable` | B | C | B |
| `faces.format` | B; C for `cff2` | C | B; C for `cff2` |
| `faces.axes` | C | C | C |
| `faces.coordinates` | C | C | C, for variable faces only |
| `faces.namedInstanceIndex` | C | C | C, for variable faces only |
| `faces.coverage` | E | E | E |
| `faces.opentype.tables` | D | D | D |
| `faces.opentype.names` | D | D | D |
| `faces.opentype.head` | D | D | D |
| `faces.opentype.hhea` | D | D | D |
| `faces.opentype.vhea` | D | D | D |
| `faces.opentype.os2` | D | D | D |
| `faces.opentype.post` | D | D | D |
| `faces.opentype.maxp` | D | D | D |
| `faces.opentype.cmap` | D | D | D |
| `faces.opentype.fvar` | D | D | D |
| `faces.opentype.meta` | D | D | D |
| `faces.opentype.layout` | D | D | D |
| `faces.platform.weight` | B | B | B |
| `faces.platform.width` | B | B | B |
| `faces.platform.slant` | B | B | B |
| `faces.platform.symbolicTraits` | B | — | — |
| `faces.platform.format` | B | not yet documented | B |
| `faces.platform.priority` | B | — | — |
| `faces.platform.registrationScope` | B | — | — |
| `faces.platform.downloadable` | B | — | — |
| `faces.platform.downloaded` | B | — | — |
| `faces.platform.styleName` | B | — | — |
| `faces.platform.displayName` | B | — | — |
| `faces.platform.localizedFamilyName` | B | — | — |
| `faces.platform.localizedStyleName` | B | — | — |
| `faces.platform.fixedAdvance` | B | — | — |
| `faces.platform.features` | C | — | — |
| `faces.platform.metrics` | C | — | — |
| `faces.platform.shadowed` | B | — | — |
| `faces.platform.typographicFamilyNames` | — | B | — |
| `faces.platform.typographicStyleNames` | — | B | — |
| `faces.platform.designLanguages` | — | B | — |
| `faces.platform.supportedLanguages` | — | B | — |
| `faces.platform.inactiveFamilyPreference` | — | not yet documented | — |
| `faces.platform.sourceType` | — | B | — |
| `faces.platform.sourceName` | — | B | — |
| `faces.platform.axisValues` | — | B | — |
| `faces.platform.resourceAxes` | — | C | — |
| `faces.platform.additionalInformationalStrings` | — | C | — |
| `faces.platform.index` | — | — | B |
| `faces.platform.spacing` | — | — | B |
| `faces.platform.fontWrapper` | — | — | B |
| `faces.platform.foundry` | — | — | B |
| `faces.platform.decorative` | — | — | B |
| `faces.platform.fontHasHint` | — | — | B |
| `faces.platform.order` | — | — | B |
| `faces.platform.outline` | — | — | B |
| `faces.platform.scalable` | — | — | B |
| `faces.platform.antialias` | — | — | B |
| `faces.platform.pixelSize` | — | — | B |
<!-- END GENERATED: field-groups -->

The groups each preset reaches. A selection costs the groups of the fields it selects, of the face identity when it selects a `faces.*` field ([api.md](api.md#always-included)), and of the fields the package computes the selected ones from:

<!-- BEGIN GENERATED: preset-groups (scripts/gen-fields.mjs) -->
| Selection | macOS | Windows | Linux |
| --- | --- | --- | --- |
| `fields: []` (`family` only) | A | A | A |
| `names` | A, C | A | A |
| `styles` | A, B, C | A, B, C | A, B; C for named instances; C for variable faces only |
| `details` | A, B, C | A, B, C | A, B, C |
| `full` | A, B, C, D | A, B, C, D | A, B, C, D |
<!-- END GENERATED: preset-groups -->

A selection that includes a `faces.*` field gives every face its identity ([api.md](api.md#always-included)), so it reaches C on macOS (`file.index`, `coordinates`) and on Windows (`coordinates`: telling whether a face is variable), and on Linux only for variable faces (`coordinates`). `names` selects no face field and so carries no identity: on Windows `familyNames` is group A, and on macOS and Linux the package computes it from the faces' `familyNames`, which cost C on macOS and A on Linux.

On macOS `names` is C because CoreText has no API for a family's names in every language, and `styles` reaches C because the face names, `symbol` and `file.index` are C there. The face identity includes `coordinates`, which a variable font's named instances take from its `fvar` table; a static face has no coordinates. On Linux a face is known to be variable without reading its file (see `variable` in the Linux field sources), so `styles` reaches A and B, and C only for the faces of variable fonts (their `coordinates`, and the names of the named instances). On Windows telling whether a face is variable is itself C (`IDWriteFontResource::HasVariations`), so `styles` reaches C for every font resource.

`faces.format` is in `details`, not `styles`, because on Windows telling a face's format needs each font resource's table directory (C): `CFF2`, `CFF` and `glyf` are told apart there. On Windows C covers two kinds of work: creating each font resource object (enough for `HasVariations`) and reading its table directory or tables (needed for `format`). Their costs have not been measured separately. `format` is kept out of `styles` so that `styles` needs only the first kind; how much the second adds is not measured. On Linux and macOS `format` is B, except that telling CFF2 from CFF needs the table directory (C).

## Family grouping

| | Family model | How `family` is chosen |
| --- | --- | --- |
| macOS | CoreText's family: the typographic family (`name` ID 16, else ID 1). ID 21 is ignored, so faces that share ID 16 but are split by ID 1 or 21 (optical sizes, legacy weight names) form one family. | `kCTFontFamilyNameAttribute` of the descriptor each PostScript name resolves to. Within one name ID, which of a font's Windows and Macintosh records CoreText takes is its own behaviour, and it differs between ID 16 and ID 1. |
| Windows | DirectWrite's weight/stretch/style family model (`DWRITE_FONT_FAMILY_MODEL_WEIGHT_STRETCH_STYLE`; OpenType calls it WWS, weight/width/slope, `name` ID 21): faces that differ in anything but weight, width and slope are separate families, so the six optical sizes of Sitka are six families. | The `en-us` value of the family's `IDWriteFontFamily::GetFamilyNames`, else its first value. |
| Linux | fontconfig: faces are grouped by their first family name. fontconfig orders a font's names by platform record (Windows, Unicode, Macintosh, ISO) and, within a platform, by `name` ID 21, 16, 1, so WWS families are split as on Windows. | The first `FC_FAMILY` of `FcFontList` with `FC_NAMELANG` set to `en`, which moves an English name first; a font without one keeps fontconfig's first name. Without `FC_NAMELANG` the first name follows the user's locale, and a family could split or merge depending on it. |

Because the models differ, the same fonts give different families and family counts on different platforms; comparing counts across platforms means nothing.

Using `family` in CSS, as measured (each figure is for the stated machine and browser, not a guarantee):

| Platform | Browser | Result |
| --- | --- | --- |
| macOS 27.0.1 | Chrome 155 | 252 of 252 families selected their own faces (for 22 of them the evidence is the width test alone). |
| Windows 11, 10.0.26200 | Chrome 155.0.8059.80 | 444 of 462 passed the width test; 458 of 462 rendered an expected PostScript name with both fallback chains (monospace and serif) in every run, 460 after correcting two recorded encoding errors, and 461 with at least partial evidence of the expected name (in the main test or in a separate character-set test; part of the rendered glyphs, not necessarily the whole string). `Gill Sans` did not select its faces, for a reason not established. |
| Ubuntu 26.04.1 | Chrome 144.0.7559.96 | Every family selected its faces except `Bitstream Charter` and `Courier 10 Pitch`, which exist only as Type 1 fonts. |

## macOS

### What is listed

- The backend lists `CTFontCollectionCreateFromAvailableFonts` with disabled fonts included and CoreText's own duplicate removal off.
- **One face per PostScript name.** A system update can leave two files with one PostScript name (on macOS 27 the static `PingFang.ttc` and the variable `PingFangUI.ttc`). Everything that takes a PostScript name (CSS `local()`, `CTFontCreateWithName`, family matching) resolves it to one of them, so the face is the one `CTFontDescriptorCreateWithNameAndSize` and `CTFontDescriptorCreateMatchingFontDescriptor` resolve to (the first collection entry when resolution finds nothing, as for a disabled font). The other files are listed in `platform.shadowed`.
- **Member order** is that of `CTFontCollectionCreateWithFontDescriptors` over a descriptor naming the family, a PostScript name counted once at its first position: the order CoreText's matcher returns. Faces that list does not name come after, in collection order. It equals `NSFontManager availableMembersOfFontFamily` for all 252 families of the machine measured.
- **Variable fonts** give one face per named instance, plus the default instance as a face of its own when no named instance has its coordinates.
- **Not listed: invisible fonts.** CoreText marks some installed system fonts invisible and skips them in every public enumeration; name lookup still finds most of them. On macOS 27.0.1 (26A434) 2,364 faces are invisible: 2,150 whose names begin with `.` (the SF, New York and other system UI fonts) and 214 with ordinary names in 131 families (Iowan Old Style, Athelas, Marion, Seravek, Superclarendon, Rockwell, Times, Courier, the first-generation STIX fonts, about 100 Noto families for rare scripts, and older names of Hiragino faces). Apple's support article "Fonts included with macOS" ([support.apple.com/en-us/127491](https://support.apple.com/en-us/127491)) lists 127 of those families as Document-support fonts (the other 4 are internal families whose names start with `.`), "available only to documents that already use the font, or to apps that request the font by name"; CoreText resolves them by name and Chrome's and WebKit's CSS accept them. The `.` fonts cannot be created by name (CoreText substitutes another font and says to use the system UI font APIs); CSS reaches them through `system-ui`. Only a private CoreText option lists invisible fonts, and the package uses no private API and scans no directories, so neither kind is returned. Chrome's `queryLocalFonts()` returns the same 654 PostScript names as this enumeration on that machine.
- **Not listed: fonts not downloaded.** A font Apple offers for download appears only once it is downloaded (`platform.downloadable` and `platform.downloaded` are then both true).
- **Disabled fonts** (turned off in Font Book) are listed, with `enabled: false`. No disabled font was present on the machine measured, so this has not been observed.
- **`families`**: CoreText's family lookup is case-insensitive, accepts some localized and legacy names and can return another family (`Kefa` returns `Kefa III`), so the backend takes its candidates from the lookups and then keeps only the families whose `family` is exactly one of the requested strings, each once. The faces are in the order of the lookup of the family's own name. The lookup lists one file for a PostScript name, so the backend also lists the collection, once per call and only when needed, for a request for `platform.shadowed` (which takes the files of each name from it) and for a name whose resolution finds nothing (which falls back to the collection's first entry, exactly as in the full listing, so `file` does not depend on `families`). Faces the lookup does not name (those the full listing puts after the lookup's members) are not looked for; none was found on the machine measured.

### Field sources

The group of each field is in [the matrix of Source groups](#the-group-of-every-field).

| Field | Source |
| --- | --- |
| `family` | See [Family grouping](#family-grouping). It comes from the descriptors of the font collection the backend enumerates. `CTFontManagerCopyAvailableFontFamilyNames` gives the same set only while no font is registered for the process: after `CTFontManagerRegisterFontsForURL` with process scope, the collection had 253 families and the call still 252 one second later. |
| `familyNames` | The union of the faces' `familyNames`. |
| `supportedLanguages`, `variable`, `enabled` (family) | Aggregates of the faces' values. |
| `languageCoverage` | Default face: `CTFontDescriptorCreateMatchingFontDescriptor` for the family name (a mandatory attribute) with traits weight 0 and width 0; its `kCTFontCharacterSetAttribute` against the orthography, member by member with `CFCharacterSetIsLongCharacterMember`. When the matcher returns no face of this family (for example when every face is disabled, which the matcher does not match), the family's first face in member order stands in. |
| `postscriptName` | `kCTFontNameAttribute`. |
| `fullNames` | `name` ID 4. A named instance: composed in every language that has both, from the typographic family name (ID 16, else 1) and the instance's `fvar` subfamily name. CoreText's own composed string is `platform.displayName`, in one language. |
| `styleNames` | `name` ID 17, else ID 2, per language. A named instance: its `fvar` subfamily name. |
| `familyNames` (face) | `name` ID 16, else ID 1, per language. |
| `weight` | `kCTFontWeightTrait`, mapped to 100...900 with the fixed boundaries browsers use for CoreText's weights, so the value is the weight a browser on macOS classifies the face as. |
| `width` | `kCTFontWidthTrait` `w` as `100 + w * (w < 0 ? 50 : 100)`, clamped to 50-200, rounded to a thousandth. |
| `style` | The italic symbolic trait: `italic`, else `normal`. |
| `monospace` | `kCTFontMonoSpaceTrait` symbolic trait. |
| `color` | `kCTFontColorGlyphsTrait` symbolic trait. |
| `symbol` | The `cmap` has a (3,0) subtable: macOS has no OS flag for symbol fonts. |
| `enabled` | `kCTFontEnabledAttribute` (a number or a boolean; a font without it is enabled). |
| `simulations` | Always false: CoreText lists no synthesized faces. |
| `supportedLanguages` | `kCTFontLanguagesAttribute`. |
| `scope` | The first rule that applies: `registrationScope` process: `process`; `registrationScope` session: `user`; `priority` 40000: `user`; 10000, 20000 or 30000: `system`; 60000 with the persistent registration scope (downloaded system font assets under `/System/Library/AssetsV2`): `system`; anything else: `unknown`. Priority 60000 alone is ambiguous: a font registered for the process has it too. |
| `format` | `kCTFontFormatAttribute`: OpenType PostScript is `cff`, or `cff2` when the font has a `CFF2` table; OpenType TrueType and TrueType are `truetype`; PostScript is `type1`; bitmap is `bitmap`; anything else `unknown`. |
| `file.path` | `kCTFontURLAttribute`, when it is a file URL. |
| `file.index` | CoreText does not say which face of a collection a font is: it is the collection face whose `name` table equals the font's. A face that cannot be told apart, and a font in a file the table reader cannot open (`.dfont`), get no index. |
| `variable` | The descriptor has a non-empty `kCTFontVariationAxesAttribute`. |
| `axes` | `kCTFontVariationAxesAttribute` (tag, range, default, hidden); the names are those of the `fvar` axis with the same tag, and `hidden` falls back to the `fvar` flag. |
| `coordinates` | `CTFontCopyVariation`, which lists only axes away from their default, filled up to every axis. CoreText reports coordinates cut to four decimals. |
| `namedInstanceIndex` | The first `fvar` instance whose coordinates equal `coordinates`, within 5e-4. |
| `opentype` | Tag list from `CTFontCopyAvailableTables`, bytes from `CTFontCopyTable`. When CoreText creates another font than the descriptor names, the tables come from the descriptor's file instead, and `platform.features` and `platform.metrics` are left out. |
| `coverage` | `kCTFontCharacterSetAttribute` through `CFCharacterSetCreateBitmapRepresentation`. |

Semantics specific to macOS:

- `weight` is a multiple of 100, unlike on the other platforms; the continuous CoreText value is `platform.weight`.
- `style` is never `oblique`: CoreText's slant trait is non-zero for upright script faces too (Zapfino, Apple Chancery), so a slant does not mean an oblique face. The raw value is `platform.slant`.
- `monospace` follows CoreText; it differed from `post.isFixedPitch` for 6 of 654 faces measured (`Monaco`, among others, is monospace for CoreText with `isFixedPitch` 0).
- `familyNames` is the `name` table's family name, not the set of names the matcher accepts. Measured with Chrome 155: of the 194 entries other than `family`, 46 were not selected in CSS and 20 only ambiguously; of the 175 legacy `name` ID 1 names that are not in `familyNames`, 99 were selected.
- `family` and `familyNames` do not depend on the process language; the `platform` strings `displayName`, `localizedFamilyName`, `localizedStyleName` and the `features` names do.
- How CoreText derives `kCTFontLanguagesAttribute`, and which color glyph formats set `kCTFontColorGlyphsTrait`, is not yet documented.

### Platform fields

The definitions are in [api.md](api.md#macos-darwinnative). Every one is a descriptor attribute, except: `features` (`CTFontCopyFeatures`, which needs a `CTFont`; read once per font resource) and `metrics` (needs a `CTFont`), and `shadowed`, which comes from grouping the collection by PostScript name.

### Freshness

Measured on macOS 27.0.1: a font copied into `~/Library/Fonts` by another process became visible to an observing process after 4.16-4.28 s (5 runs), and a removed one disappeared after 4.08-4.17 s (4 runs) and once after 8.16 s. At 0, 1 and 3 s after the change it was mostly not yet visible; at 10 s it was in all 5 runs. A process started after the change saw it no earlier, so the delay is in the system font service, not in the process. There is no upper bound.

## Windows

### What is listed

- The backend reads the system font collection, `IDWriteFactory3::GetSystemFontCollection` with downloadable fonts excluded and without an update check (`checkForUpdates` FALSE). With the update check the first call in a process spends about 300 ms and later calls about 190 ms (measured) asking the font cache service to rescan; without it the call reads the service's registered state, which trails a change by about a second ([Freshness](#freshness-1)).
- **Every member of every family is a face**, including the members DirectWrite synthesizes (`simulations`; 1,333 of 2,568 members on the machine measured). Simulated faces share the tables of their base font.
- **Exact duplicates**: 6 members had the same identity as another member on the machine measured; whether their other fields were identical has not been established. A member listed twice with identical content is one face.
- **Member order** is the collection's member order.
- **Not listed**: fonts whose data is not on the machine; fonts a process registered privately with `AddFontResourceEx(FR_PRIVATE)`, which are not in the system collection.
- **Fonts marked as hidden** are listed, with `enabled: true` and `platform.inactiveFamilyPreference: true`.
- **`families`**: `IDWriteFontCollection::FindFamilyName` (case-insensitive, accepts localized names) gives the candidates; the backend keeps only the families whose `family` is exactly the requested string. `scope` still needs the whole system font set (1,235 entries on the machine measured), even for one family.

### Field sources

The group of each field is in [the matrix of Source groups](#the-group-of-every-field).

| Field | Source |
| --- | --- |
| `family` | See [Family grouping](#family-grouping). |
| `familyNames` | `IDWriteFontFamily::GetFamilyNames`, every locale: the names DirectWrite's matcher recognizes. Other names in the font's `name` table are in `opentype.names`. |
| `supportedLanguages`, `variable`, `enabled` (family) | Aggregates of the faces' values. |
| `languageCoverage` | Default face: `IDWriteFontFamily::GetFirstMatchingFont(400, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL)`; its `IDWriteFont1::GetUnicodeRanges` against the orthography. |
| `postscriptName` | Informational string `DWRITE_INFORMATIONAL_STRING_POSTSCRIPT_NAME`, the `en-us` value, else the first. |
| `fullNames` | Informational string `DWRITE_INFORMATIONAL_STRING_FULL_NAME`, every locale, as DirectWrite gives it for the member (named instances and simulated members included). |
| `styleNames` | `IDWriteFont::GetFaceNames`, every locale: the member's name within its WSS family. The typographic style names are `platform.typographicStyleNames`. |
| `familyNames` (face) | `GetFamilyNames` of the member's family. |
| `weight` | `IDWriteFont::GetWeight`, unrounded. |
| `width` | `IDWriteFont::GetStretch` 1-9 mapped to 50, 62.5, 75, 87.5, 100, 112.5, 125, 150, 200. |
| `style` | `IDWriteFont::GetStyle`: normal, oblique, italic. |
| `monospace` | `IDWriteFont1::IsMonospacedFont`. |
| `color` | `IDWriteFont2::IsColorFont`. |
| `symbol` | `IDWriteFont::IsSymbolFont`. |
| `enabled` | Always true: DirectWrite has no disabled state. |
| `simulations` | `IDWriteFont::GetSimulations` bits. |
| `supportedLanguages` | Informational string `DWRITE_INFORMATIONAL_STRING_SUPPORTED_SCRIPT_LANGUAGE_TAG`, split into tags; a script alone becomes `und-` and the script (`Hans` is `und-Hans`). |
| `scope` | `IDWriteFontSet3::GetFontSourceType` of the member's entry in the collection's font set (a simulated member takes its base font's entry): per-machine is `system`, per-user `user`, anything else (an app package, for example) `unknown`. |
| `format` | The table directory: `CFF2` is `cff2`, `CFF` is `cff`, `glyf` is `truetype`; without those, `DWRITE_FONT_FACE_TYPE` Type 1 is `type1` and bitmap is `bitmap`; anything else `unknown`. A collection is a container, not a format. |
| `file.path` | `IDWriteFont3::GetFontFaceReference`, `IDWriteFontFaceReference::GetFontFile`, and the path from `IDWriteLocalFontFileLoader::GetFilePathFromKey`. |
| `file.index` | `IDWriteFontFaceReference::GetFontFaceIndex`. |
| `variable` | `IDWriteFontResource::HasVariations`. |
| `axes` | The `fvar` axes only. The standard axes DirectWrite adds for a font that does not define them (a fixed `wdth` of 100, for example) are in `platform.resourceAxes`. |
| `coordinates` | `IDWriteFontFaceReference1::GetFontAxisValues`, limited to the `fvar` axes. |
| `namedInstanceIndex` | The first `fvar` instance whose coordinates equal `coordinates`, within 1/65536. |
| `opentype` | One `IDWriteFontFace` per font resource, bytes from `IDWriteFontFace::TryGetFontTable`; the tag list from the file's sfnt directory, because `TryGetFontTable` cannot enumerate tables. |
| `coverage` | `IDWriteFontFace1::GetUnicodeRanges`. |

Semantics specific to Windows:

- `weight` can be any value DirectWrite reports (350 and 550 occur); `width` has nine steps.
- `monospace` and `symbol` follow DirectWrite: `IsMonospacedFont` differed from `post.isFixedPitch` for 40 faces and `IsSymbolFont` from the `cmap` (3,0) subtable for 8 faces measured.
- `supportedLanguages` holds the tags the font declares and is often empty: in an earlier snapshot of the system measured (Windows 11 25H2, dwrite.dll 6.2.26100.9711), 2,086 of 2,568 members had none, and those were the members without a `meta` table (not a general fact). Whether DirectWrite derives tags for a font that declares none is not yet documented. Most declared tags are scripts (`und-Hans`, `und-Latn`) rather than languages.
- `scope`: the 24 fonts from app packages on the machine measured (Windows Terminal's, for example) are `unknown`.
- `pickName(familyNames, ['en']) === family` has not been measured on Windows; `family` is one of `familyNames` because it is chosen from the same list.

### Platform fields

The definitions are in [api.md](api.md#windows-win32native).

| Fields | Source |
| --- | --- |
| `weight`, `width`, `slant` | The raw `IDWriteFont` values (`GetWeight`, `GetStretch`, `GetStyle`). |
| `typographicFamilyNames`, `typographicStyleNames`, `designLanguages`, `supportedLanguages` | Informational strings. |
| `sourceType`, `sourceName` | `IDWriteFontSet3::GetFontSourceType` and `GetFontSourceName` of the face's entry in the whole system font set. |
| `axisValues` | `IDWriteFontFaceReference1::GetFontAxisValues`. |
| `inactiveFamilyPreference` | One registry read per call; its group is not yet documented. |
| `resourceAxes` | The axes of the font resource (`IDWriteFontResource`). |
| `additionalInformationalStrings` | Every informational string, compared with the `name` table. |
| `format` | Not yet documented. |

### Freshness

Measured on the machine below, with a test font installed into the user's font directory, registered under `HKCU` and announced with `WM_FONTCHANGE`. Without the update check, immediately after the change 0 of 5 new processes saw an installed font and 1 of 5 saw a removal; 1 s and 5 s after it, 5 of 5 did. A long-running process behaved the same (0 of 5 at once, 5 of 5 at 1 s and 5 s). With the update check every first observation was correct, at about 240-300 ms per call. About a second is what was measured, not an upper bound.

## Linux

### What is listed

- **Each call loads a fresh configuration** (`FcInitLoadConfigAndFonts`), owns it alone and destroys it at the end, so the call sees the fonts on disk now and shares no fontconfig state with the rest of the process. With warm fontconfig caches this costs about 5 ms.
- **The list** is one `FcFontList` call over that configuration, with `FC_NAMELANG` set to `en` and one fixed object set whatever fields are selected: the scalar fontconfig objects the field table below uses (including the [platform fields](#platform-fields-2)) plus `FC_FILE`, `FC_INDEX` and `FC_VARIABLE`. `FC_CHARSET` and `FC_LANG` are added only when a field needing them is selected: `languageCoverage` and `faces.coverage` (`FC_CHARSET`), `supportedLanguages` of a family or a face (`FC_LANG`). `FcFontList` returns its patterns in the order of a hash of the requested objects' values, so its order changes with the object set; the backend does not use it (see member order below).
- **A face is a pattern with fixed coordinates.** fontconfig also lists, for each variable font, a pattern whose weight and width are ranges (`FC_VARIABLE` true); it describes the whole design space, so it is not a face, and its ranges are the face's `axes`. A variable font is listed as its default instance and its named instances. On the machine measured this leaves 806 of 816 patterns, with 806 distinct identities.
- **Not listed**: fonts the configuration rejects (`<rejectfont>`); fontconfig offers no enumeration of them. Fonts an application adds to its own configuration (`FcConfigAppFontAddFile`) are not in the configuration the call loads, so `scope` is never `process` on Linux.
- **Type 1 fonts** are listed with the attributes fontconfig gives and no `opentype`.
- **Member order** is the faces' order in the configuration's font sets (`FcConfigGetFonts`: the system set, then the application set), the order fontconfig loaded them and the order `FcFontList` walks before hashing. It does not depend on the fields selected, so the order of faces, and of a family's names collected from them, is the same in every projection, as the [invariants](api.md#invariants) require.
- **`families`**: `FcFontList` with a family condition gives the candidates (it matches any of a font's family names); the backend keeps only the families whose `family` is exactly the requested string.

### Field sources

The group of each field is in [the matrix of Source groups](#the-group-of-every-field).

| Field | Source |
| --- | --- |
| `family` | See [Family grouping](#family-grouping). |
| `familyNames` | The union of the faces' `familyNames`. |
| `supportedLanguages`, `variable`, `enabled` (family) | Aggregates of the faces' values. |
| `languageCoverage` | Default face: `FcFontSetMatch` over the family's own patterns for regular weight, normal width and roman slant, after `FcConfigSubstitute` and `FcDefaultSubstitute`; its `FC_CHARSET` against the orthography. Matching within the family keeps an alias rule from substituting another family. |
| `postscriptName` | `FC_POSTSCRIPT_NAME`. |
| `fullNames` | `FC_FULLNAME` with `FC_FULLNAMELANG`. A named instance: composed from the `name` and `fvar` tables as on macOS. |
| `styleNames` | `FC_STYLE` with `FC_STYLELANG`. A named instance: its `fvar` subfamily name. |
| `familyNames` (face) | Every `FC_FAMILY` value with its `FC_FAMILYLANG`, aliases included. |
| `weight` | `FC_WEIGHT` through `FcWeightToOpenTypeDouble`. A named instance has the instance's weight, which can differ from the font's `OS/2` weight. |
| `width` | `FC_WIDTH`. |
| `style` | `FC_SLANT`: roman is `normal`, italic `italic`, oblique `oblique`. |
| `monospace` | `FC_SPACING` mono (100), dual-width (90) or character cell (110); proportional, or no value, is false. |
| `color` | `FC_COLOR`. |
| `symbol` | `FC_SYMBOL`. |
| `enabled` | Always true: a font fontconfig rejects is not listed. |
| `simulations` | Always false: fontconfig synthesizes bold and oblique only when matching (`FC_EMBOLDEN`, `FC_MATRIX` in a match result), never in the list. |
| `supportedLanguages` | `FC_LANG`. |
| `scope` | The `FC_FILE` path, compared at directory boundaries: under `/usr/share/fonts` or `/usr/local/share/fonts` is `system`; under `$XDG_DATA_HOME/fonts` (by default `~/.local/share/fonts`) or `~/.fonts` is `user`; anything else `unknown`. fontconfig has no field saying who installed a font, so this is the path's convention, not a fact the OS reports. |
| `format` | `FC_FONTFORMAT`: `TrueType` and `Type 42` are `truetype`; `CFF` is `cff`, or `cff2` when the font has a `CFF2` table; `Type 1` and `CID Type 1` are `type1`; `PCF`, `BDF` and `Windows FNT` are `bitmap`; anything else `unknown`. |
| `file.path` | `FC_FILE`. |
| `file.index` | The low 16 bits of `FC_INDEX`. |
| `variable` | The face's file and index (the low 16 bits of `FC_INDEX`) have a range pattern: a pattern whose weight, width or other axis is a range (`FC_VARIABLE` true), which is not a face itself. Every variable resource has exactly one. |
| `axes` | `fvar`. |
| `coordinates` | A named instance: its `fvar` instance's coordinates; the default instance: the `fvar` defaults. A static face has none. |
| `namedInstanceIndex` | The first `fvar` instance whose coordinates equal `coordinates`. fontconfig does not list a named instance whose coordinates equal the default instance's: it lists that face once, with the high 16 bits of `FC_INDEX` zero, so `FC_INDEX` alone cannot give the default face its index. |
| `opentype` | An sfnt file is read directly by the table parser. A WOFF or WOFF2 file is read through FreeType (`FT_Sfnt_Table_Info` for the tag list, `FT_Load_Sfnt_Table` for the bytes). A Type 1 or bitmap font has none. |
| `coverage` | `FC_CHARSET`. |

Semantics specific to Linux:

- `familyNames` contains aliases: the legacy weight-split names (`IBM Plex Sans Thin`) next to the typographic name. Their language tags are fontconfig's, so English is mostly `en` rather than the `name` table's `en-US`. The preferred name of each language is the first, and `pickName(familyNames, ['en'])` equalled `family` for all 301 families measured (the machine's 296 plus 5 test families added for the measurement).
- `width` is fontconfig's value. For a static font fontconfig derives it from `OS/2` `usWidthClass` with its own constants, which put semi-condensed at 87 and semi-expanded at 113 where CSS uses 87.5 and 112.5. Which values occur in practice is not yet documented.
- `monospace` counts dual-width fonts (a Latin advance and a double CJK advance, such as Sarasa Term), which CSS `monospace` can use; the raw value is `platform.spacing`. It differed from `post.isFixedPitch` for 13 fonts measured.
- `supportedLanguages` is computed by fontconfig from the face's character coverage against its orthographies, not declared by the font.
- `scope` for fonts inside Snap or Flatpak sandboxes is not yet documented.
- Which color glyph formats set `FC_COLOR` is not yet documented. No CFF2 or bitmap font was present on the machine measured.

### Platform fields

The definitions are in [api.md](api.md#linux-linuxnative). Every Linux platform field is an object of the same `FcFontList` call.

### Freshness

Measured on Ubuntu 26.04.1 with fontconfig 2.17.1: a font added to or removed from `~/.local/share/fonts` by another process was seen by the next call of the same process, without running `fc-cache`; fontconfig rescans a directory whose modification time changed when the configuration is loaded. A configuration kept across calls would not see it (still stale after 31 s), and fontconfig's own global configuration rescans at most every 30 seconds, which is why each call loads its own. Files rewritten in place with their times preserved, and network file systems, are not covered.

## Measured cost

Indicative figures, not guarantees. Each figure is the median of 5 freshly started Node processes, with the minimum and maximum in brackets, in milliseconds, from the call until the normalized result is ready in JS; Node start-up and module loading are excluded. OS file caches were warm. All were measured in October 2026. The three machines and font sets differ, so a row is not a comparison of the backends. The macOS rows for the family list, `familyNames` and `languageCoverage` come from measurement functions built against the backend's sources, not from the backend itself, as do the single-family rows; the others come from the backend (the breakdown below from a synchronous copy of it instrumented with timers); the Windows and Linux figures from standalone implementations of the sources above.

### macOS

Apple M4, 10 cores, 16 GB, macOS 27.0.1 (26A434), Node 24.20.0. 252 families, 654 faces, 298 files.

| Selection | Groups | ms |
| --- | --- | --- |
| Family list: the font collection with disabled fonts included, and its descriptors | A | 12.39 [12.05-13.35] |
| `family`, `familyNames` | A, C | 59.55 [56.49-61.90] |
| `family`, `familyNames`, `languageCoverage` for one language: `zh-CN` / `zh-TW` / `ja` | A, C, E | 119.74 [116.47-130.06] / 119.07 [116.37-130.18] / 116.93 [115.03-130.24] |
| Every field except `opentype`, `coverage` and `platform.metrics` | A, B, C | 526.9 [504.7-631.2] |
| Every field except `coverage` | A-D | 556.0 [545.2-685.5] |
| Every field except `coverage`, one family (`families`), 6-14 faces | A-D | 14.6-21.6 |
| `faces.coverage` | E | not yet documented |

- The backend's `fields: []` path, which also resolves duplicate PostScript names, is not measured.
- Per-face attributes (B) have no figure of their own. In a synchronous copy of the backend instrumented with timers (509.4 ms in total), `CTFontCopyFeatures` (`platform.features`) took about 142 ms.
- The first call in a process is slower than later ones: about 507 ms, then about 120 ms for every field except `opentype`, `coverage` and `platform.metrics`.
- On the same machine under another workload every row took 2.5-3 times as long.
- JSON size: 2.80 MB without `opentype`, 8.97 MB with it.

### Windows

Intel Core i7-8700K, 12 logical CPUs, 31.93 GiB, Windows 11 Pro 10.0.26200, dwrite.dll 10.0.26100.8875, Node 24.20.0. 462 families, 2,568 members (1,333 simulated), 906 font resources, 882 files.

The backend calls `GetSystemFontCollection` without the update check (`checkForUpdates` FALSE). Most figures were measured with the update check (TRUE), which costs about 300 ms per call in a fresh process. The FALSE column says which figures were measured that way; the others are derived as the TRUE figure minus that run's collection step under TRUE, plus the 1.556 ms the collection step takes under FALSE. The update check's cost depends on DirectWrite's internal state (later calls in one process spend about 190 ms on it), so a derived figure is an order of magnitude only.

| Selection | Groups | TRUE (measured) | FALSE |
| --- | --- | --- | --- |
| `family`, `familyNames` | A | 301.56 [298.12-331.56] | 4.747 [4.704-5.425], measured |
| `family`, `familyNames`, `languageCoverage` for `zh-CN` | A, E | 385.20 [378.44-398.34] | about 66, derived |
| The same for `zh-TW` / `ja` | A, E | 385.50 [381.11-489.88] / 382.86 [373.38-398.49] | not derived |
| Per member: names, `weight`, `width`, `style`, `simulations` | A, B | 570.62 [562.57-837.94] | about 257, derived |
| The same and `monospace`, `color`, `symbol`, `supportedLanguages` | A, B | 575.87 [570.19-582.75] | not derived |
| The same and `file`, raw axis values | A, B | 593.97 [583.50-601.32] | about 277, derived |
| Every field except `opentype` and `coverage` | A, B, C | 1,798.86 [1,778.13-1,831.50] | about 1,484, derived |
| Every field except `coverage` | A-D | 2,054.43 [2,017.80-2,095.01] | about 1,740, derived |
| Every field except `coverage`, one family | A-D | 310-328 | not measured |
| Names and the code point ranges of one default face per family (273,621 ranges) | A, E | 463.85 [447.88-470.07] | not derived |
| `faces.coverage` for every member | E | not yet documented | not yet documented |

- Without `opentype` the call still reads 906 `name` tables and 20 `fvar` tables (3.2 MB); with it, 7,804 table reads (61.5 MB).
- JSON size: 4.65 MB without `opentype`, 29.04 MB with it.

### Linux

Intel Core i5-1135G7, 4 cores / 8 threads, about 15 GiB, Ubuntu 26.04.1, fontconfig 2.17.1, FreeType 2.14.2, Node 24.13.0. 296 families, 816 patterns (806 faces), 710 font resources, 684 files.

| Selection | Groups | ms |
| --- | --- | --- |
| Loading the configuration only | | 5.99 [5.97-11.91] |
| `family`, `familyNames`, with an object set of family names only (`FC_FAMILY`, `FC_FAMILYLANG`); the backend's fixed set is larger and its `fields: []` call is not measured | A | 8.13 [8.01-8.26] |
| `family`, `familyNames`, `languageCoverage` for one language: `zh-CN` / `zh-TW` / `ja` | A, E | 95.06 [92.13-195.10] / 92.42 [90.57-194.60] / 91.90 [90.89-95.59] |
| Every fontconfig object of every pattern, raw, no tables | A, B | 65.23 [52.05-85.85] |
| Every field except `opentype` and `coverage`, tables of 592 sfnt files read directly | A, B, C | 135.43 [132.17-183.91] |
| The same, tables of all 632 sfnt and WOFF/WOFF2 resources through FreeType | A, B, C | 199.72 [196.63-294.90] |
| Every field except `coverage`, 592 sfnt files read directly | A-D | 229.97 [229.69-324.55] |
| The same, all 632 resources through FreeType | A-D | 287.57 [286.93-349.37] |
| Every field except `coverage`, one family (2-32 faces) | A-D | 11.3-20.5 |
| `faces.coverage` | E | not yet documented |

- The backend reads sfnt files directly and only WOFF and WOFF2 files through FreeType, a mix neither pair of rows measures exactly.
- The same configuration measured again gave 168.50 [132.36-203.62] for the row of 135.43, so differences of that size between runs are noise.
- With fontconfig's caches empty, loading the configuration took 380-520 ms instead of about 5.
- JSON size: 1.95 MB without `opentype`, 6.92 MB with it.
