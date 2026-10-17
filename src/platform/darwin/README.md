# src/platform/darwin

The macOS backend: CoreText for everything, plus one small Objective-C++ file
(`autorelease_pool.mm`) for a thread's autorelease pool. Read `../README.md`
first for the contract.

## Files

| File | Role |
| --- | --- |
| `enumerate.cpp` | `enumerateFonts`: collection, grouping by PostScript name and family, member order |
| `face_reader.*` | builds one `Face`; caches the parsed tables, the axes and the collection face lookups for one enumeration |
| `traits.*` | pure conversions: CSS weight and width, style, scope, format |
| `variations.*` | axes, coordinates, named instance |
| `platform_value.*` | the `platform` value (`DarwinNative`) |
| `coverage.*` | character set to code point ranges |
| `legacy_text.cpp` | `decodeLegacyText` |
| `testing.cpp` | the register / unregister test hooks |
| `autorelease_pool.*` | the autorelease pool the worker thread runs in (the only Objective-C++) |
| `cf.*` | owning CF reference and checked accessors |

## One face per PostScript name

`CTFontCollectionCreateFromAvailableFonts` (disabled fonts included, CoreText's
own duplicate removal left off: the grouping below does that work) lists a font
once for each file that provides it.
A system update can leave two files with the same PostScript name: on current
macOS the static `PingFang.ttc` from the font assets and the variable
`PingFangUI.ttc` of the private FontServices framework. Everything that takes a
PostScript name (CSS `local()`, `CTFontCreateWithName`, family matching)
resolves it to exactly one of them, so the result has one face per name: the
one `CTFontDescriptorCreateWithNameAndSize` + `CTFontDescriptorCreateMatchingFontDescriptor`
resolves to. When resolution finds nothing (a disabled font is not matched),
the first collection descriptor of that name is used. The files of the other
descriptors go to `platform.shadowed` (`path`, `variable`); a descriptor that
names the chosen file again is the same font and is merged without a trace.

## Families and member order

A family is keyed by the chosen descriptor's `kCTFontFamilyNameAttribute`, in
first-seen order (the JS layer sorts). The faces of a family follow the order of
`CTFontCollectionCreateWithFontDescriptors` over one descriptor that names the
family (disabled fonts included), a PostScript name counted once at its first
position; this is the order CoreText's own matcher returns and a browser walks.
Faces that list does not name come after, in collection order. It equals
`NSFontManager availableMembersOfFontFamily` for every family on the machine it
was checked on (252 of 252), so AppKit is not needed. The enumeration runs in an
autorelease pool because a worker thread has none and CoreText autoreleases
objects internally.

## Source of each field

- `postscriptName`: `kCTFontNameAttribute`.
- `weight`: `kCTFontWeightTrait` through the bucket table in `traits.cpp`.
  Browsers map CoreText's continuous -1..1 weights to CSS 100..900 with fixed
  boundaries; using the same ones means this library names the weight a browser
  would pick the face for. `width`: `100 + w * (w < 0 ? 50 : 100)` clamped to
  50..200 and rounded to a thousandth (CoreText keeps the trait as a 32-bit
  float, so 90% arrives as 89.99999985).
- `style`: italic symbolic bit is `italic`, everything else `normal`, as a
  browser classifies a macOS face. `oblique` is never reported: CoreText's
  slant trait is non-zero for upright script faces too (Zapfino, Apple
  Chancery), so it is left raw in `platform.slant`.
- `monospace`, `color`: symbolic bits. `symbol`: the cmap has a (3,0) subtable
  (from the tables). `enabled`: `kCTFontEnabledAttribute` (a number or a
  boolean). `hidden` and the simulations are always false: macOS has neither.
- `languages`: `kCTFontLanguagesAttribute`.
- `scope`, first rule that applies: `kCTFontRegistrationScopeAttribute` is the
  process scope: process. `kCTFontPriorityAttribute` 40000: user; 10000 /
  20000 / 30000: system; 60000 with the persistent registration scope: system;
  anything else: unknown. Priority 60000 alone is ambiguous. Measured on
  macOS 27: a font registered for the process has priority 60000 and
  registration scope 1, while the downloaded system font assets (under
  `/System/Library/AssetsV2`, `downloadable` and `downloaded` both true) have
  priority 60000 and registration scope 2 (persistent).
- `format`: `kCTFontFormatAttribute`; an OpenType PostScript font is `cff2`
  when it has a `CFF2` table.
- `file.path`: `kCTFontURLAttribute`, when it is a file URL (any other URL gives
  no path). `file.index`: CoreText does not say which
  face of a collection a font is, so it is the face whose `name` table equals
  the font's `name` table (the collection is read through `sfnt::FontFile`, once
  per file). A collection face that cannot be told apart, and an sfnt font in
  a file the reader cannot open (a `.dfont`), get no `file.index`: it is
  omitted, never guessed. A font without tables (not an sfnt) is a single-face
  file and has index 0. A face without an index shares no `opentype` or axes
  with other faces, because the font resource is keyed by file and index.
- `coverage`: `kCTFontCharacterSetAttribute` through
  `CFCharacterSetCreateBitmapRepresentation` (the BMP bitmap, then a plane
  number byte and bitmap for every further plane).
- `platform`: see `ts/types.ts` (`DarwinNative`) for the meaning of each key.
  A value CoreText does not provide for a face (an attribute that is missing) is
  left out of the object; no default is written in its place. The traits that
  feed `weight`, `width`, `style`, `monospace` and `color` count as 0 then.
  `features` come from `CTFontCopyFeatures` because the attribute carries
  identifiers without names; the call is made once per font resource (the
  named instances of a variable font report the same list). `metrics` are CoreText's lengths scaled from the
  creation size to font units (the slant angle is in degrees and is not
  scaled); they are attached only when `tables` is on.

## Tables and sharing

CoreText can create another font than the descriptor names (it substitutes one
when it cannot use the file). The PostScript name of the created CTFont is
compared with the descriptor's; when they differ, the CTFont is not trusted:
the tables are read from the descriptor's file through `sfnt::FontFile`, in the
face that has the descriptor's PostScript name, either as its own `name`
record (nameID 6) or as the PostScript name of one of its `fvar` named
instances (so a substituted named instance gets the tables of its variable
font; `sfnt::FontFile::faceWithPostscriptName` finds it), the coordinates come from the descriptor's `kCTFontVariationAttribute`,
and `platform.features` and `platform.metrics` are left out because only a
CTFont provides them. This was not observed on macOS 27; it is a guard.

Every face is read through `sfnt::readFontTables` (tag list from
`CTFontCopyAvailableTables` in CoreText's own order, which the reader sorts;
bytes from `CTFontCopyTable`) whatever
`options.tables` says: the names, `fvar` and `cmap` feed fields that exist
anyway. The option only decides whether `opentype` and `platform.metrics` are
attached. The result is cached per font resource (file path and face index) for the length of one enumeration, so
the named instances of a variable font share one `opentype` value and one axes
list.

## Names and variations

- `familyNames`: nameID 16 with 1 as fallback per language; `styleNames`: 17
  with 2; `fullNames`: 4.
- A named instance of a variable font has no `name` records of its own for its
  full name. Its `styleNames` are the instance's subfamily name from `fvar`, and
  its `fullNames` are composed from the font's own names by
  `sfnt::instanceFullNames` (family and subfamily name in every language both
  have). CoreText's own composed string is `platform.displayName`; it is in one
  language and its family part is not always the font's typographic family.
- `axes`: `kCTFontVariationAxesAttribute` (tag, range, default); the names are
  those of the `fvar` axis with the same tag, and `hidden` falls back to the
  `fvar` axis flag when CoreText's dictionary has none. `coordinates`:
  `CTFontCopyVariation`, which lists only axes away from their default, filled
  up to every axis. `namedInstance`: the first `fvar` instance whose
  coordinates all match. CoreText reports coordinates cut to four decimals, so
  the match tolerance is 5e-4.

## Legacy names

`decodeLegacyText` is only the converter: it maps each `sfnt::LegacyEncoding`
to the CoreFoundation string encoding of the same character set and converts
with `CFStringCreateWithBytes`. The choice of encoding for a record, and the
dropping of the zero bytes of Windows multi-byte strings, belong to `../../sfnt`.
Johab (`Windows1361`) is not available in CoreFoundation and gives `nullopt`
(`CFStringIsEncodingAvailable`).

## Test hooks

`CTFontManagerRegisterFontsForURL` / `UnregisterFontsForURL` with process scope.
A failure throws `std::runtime_error` with the `CFError` description.
