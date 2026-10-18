# src/platform/darwin

The macOS backend: CoreText for everything, plus one small Objective-C++ file
(`autorelease_pool.mm`) for a thread's autorelease pool. Read `../README.md`
first for the contract.

## Files

| File | Role |
| --- | --- |
| `enumerate.cpp` | `enumerateFonts`: the collection or the family lookups, grouping by PostScript name and family, member order, the per-family `languageCoverage` |
| `face_reader.*` | builds one `Face`, doing only the work its requested fields need; caches the parsed tables, the `opentype` value, the axes and the collection face lookups for one enumeration |
| `traits.*` | pure conversions: CSS weight and width, style, scope, format |
| `variations.*` | axes, coordinates, named instance |
| `platform_value.*` | the `platform` value (`DarwinNative`) |
| `coverage.*` | character set to code point ranges |
| `language_coverage.*` | a family's default face and the count of an orthography's code points it covers |
| `legacy_text.cpp` | `decodeLegacyText` |
| `testing.cpp` | the register, unregister and enable / disable test hooks |
| `autorelease_pool.*` | the autorelease pool the worker thread runs in (the only Objective-C++) |
| `cf.*` | owning CF reference and checked accessors |

## What the request changes

The backend does the work of the groups the request reaches
(`docs/platforms.md`, "Source groups") and no more; what reaches JS is cut to
the request by `core/to_js.cpp`. `WorkLog` counts the work of groups C, D and E
as it happens, and the tests hold it to the catalogue: `fontsOpened` (a
`CTFont` created for a face, C), `tableEntries` (an `opentype` entry built per
font resource, D), `characterSets` (a character set fetched and walked: one
per face for `faces.coverage`, one per family for `languageCoverage`, E).

- **A** is always done: the descriptors, grouped by PostScript name, resolved
  and grouped by family (`family` needs the resolution). `enabled` and `scope`
  add a second listing without the disabled fonts (below).
- **Faces** are built, and the members put in order, only when the request wants
  a face-level field (`wantsAnyFace()`).
- **B**: a face reads only the descriptor attributes of its requested fields.
  The traits are read once for `weight`, `width`, `style`, `monospace`,
  `color` and the raw platform values; `faces.variable` is a non-empty
  `kCTFontVariationAxesAttribute`; the `shadowed` files are found only for
  `platform.shadowed`.
- **C**: the `CTFont` and the tables are opened for a face by the first field
  that needs them and not before: `file.index`, `symbol`, the names, `axes`,
  `coordinates`, `namedInstanceIndex`, `features`, `metrics` and the `opentype`
  entries. `format` needs them only for an OpenType PostScript face, to tell
  CFF2 from CFF (`formatOf` asks lazily). A request of descriptor attributes
  alone creates no font. The names, `coordinates` and `namedInstanceIndex` go
  together because a named instance has names of its own.
- **D**: `sfnt::openTypeValue` builds the entries of the request, once per font
  resource.
- **E**: `faces.coverage` reads each face's character set; `languageCoverage`
  matches one default face per family (below).

## Language coverage

The default face of a family is what `CTFontDescriptorCreateMatchingFontDescriptor`
returns for the family name (a mandatory attribute) with the weight and width
traits 0. Its `kCTFontCharacterSetAttribute` is walked once per requested
orthography with `orthography::countCovered` and
`CFCharacterSetIsLongCharacterMember`; the counts follow
`Request::orthographies`. When the matcher returns nothing or a face of another
family, the family's first face in member order stands in (it was never needed
on the machine measured: all 252 families matched). A family costs one
`characterSets`, whatever the number of orthographies.

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

CoreText's matcher caches its answers in the process, and their invalidation
runs on the main dispatch queue, which a process that does not run the main run
loop (plain Node, a `worker_threads` worker) never services. Such a process
keeps getting a file that was removed or disabled since, and the face and its
`shadowed` files would come from a file that is not listed any more. So the
resolution is used only when its file is among those the listing of this call
has for that name (`listsFileOf` in `enumerate.cpp`); otherwise the first
descriptor of that listing is used, as when resolution finds nothing, and
`shadowed` is computed from the listing's files. With `request.families` the
check is made against the family lookup's group, and against the whole
collection's group only when the lookup does not list the file (it can list
just one file for a name). The check cannot tell which of several listed files
a fresh process would resolve to, so with two files that both exist the
cached answer stands; the attributes of the resolved descriptor,
`platform.priority` among them, are the matcher's cached ones too.

## Disabled fonts

A disabled font is listed, but CoreText does not mark it: its descriptor reports
`kCTFontEnabledAttribute` 1, a priority of 60000 and registration scope 2 that
are made up, and it is absent from name matching and from the
`CTFontManagerCopyAvailable*` lists. The one reliable sign is that the face is
absent from the same listing made without `kCTFontCollectionIncludeDisabledFontsOption`.
So when the request wants `enabled` or `scope` (`needsEnabledState` in
`enumerate.cpp`), `enumerate.cpp` makes that second listing and hands each face its
verdict as an argument of `FaceReader::read`: for the whole collection one extra
`CTFontCollectionCreateFromAvailableFonts`, with `request.families` one extra
family lookup per requested name, so the verdict comes from the listing the faces
came from. A face is enabled when its PostScript name and file are both in that
listing. Disabled fonts the collection lists twice (identical descriptors) are one
face, merged by the grouping above. A font registered for the process and then
disabled is listed once and keeps registration scope 1. The raw `platform.priority`
and `platform.registrationScope` of a disabled font are CoreText's invention and
are reported as they are.

## Families and member order

A family is keyed by the chosen descriptor's `kCTFontFamilyNameAttribute`, in
first-seen order (the JS layer sorts). The faces of a family follow the order of
`CTFontCollectionCreateWithFontDescriptors` over one descriptor that names the
family (disabled fonts included), a PostScript name counted once at its first
position; this is the order CoreText's own matcher returns.
Faces that list does not name come after, in collection order. It equals
`NSFontManager availableMembersOfFontFamily` for every family on the machine it
was checked on (252 of 252), so AppKit is not needed. The enumeration runs in an
autorelease pool because a worker thread has none and CoreText autoreleases
objects internally.

With `request.families` the descriptors are not the whole collection but, for
each distinct requested name, the family lookup that gives the member order
(disabled fonts included). The lookup is case-insensitive and can return another
family, so the families it yields are selected with `Request::wantsFamily` (the
one exact match), and one that several lookups yield (`Kefa` and `Kefa III`
requested) is kept once, from the lookup of its own name. The groups of that
lookup come out in its order, which is the member order, so the lookup is made
once. `families: []` returns before any CoreText call. The lookup lists
one file for a PostScript name, so the collection settles the names where that
is not enough: it is listed (once per call, on first use) when the request wants
`platform.shadowed`, whose files only the collection knows, and when resolution
finds nothing for a name or names a file the lookup does not list, so that the
fallback is the collection's first descriptor of that name here as in the full
listing (the `file` of a face does not depend on whether `families` was given). A face the lookup does not name
(the tail the full listing appends) is not found on this path.

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
  (from the tables). `enabled`: the face is in the collection listed without the
  disabled fonts (see "One face per PostScript name"); the descriptor's
  `kCTFontEnabledAttribute` is 1 for a disabled font. The simulations are always
  false: macOS has none.
- `supportedLanguages`: `kCTFontLanguagesAttribute`.
- `scope`, first rule that applies: `kCTFontRegistrationScopeAttribute` is the
  process scope: process. The face is disabled: unknown, because CoreText makes
  up the priority (60000) and the registration scope (2) of a disabled font, so
  they say nothing about who installed it. `kCTFontPriorityAttribute` 40000 or 50000 (dynamic: a
  font an application registered for the user, with session or persistent
  scope; macOS 27 reports a session registration this way, checked by hand: no
  test registers at session scope, since that registration is visible to every
  process, the concurrently running test files included): user; 10000 / 20000 / 30000: system; 60000 with the persistent
  registration scope: system; anything else: unknown. Priority 60000 alone is
  ambiguous. Measured on
  macOS 27: a font registered for the process has priority 60000 and
  registration scope 1, while the downloaded system font assets (under
  `/System/Library/AssetsV2`, `downloadable` and `downloaded` both true) have
  priority 60000 and registration scope 2 (persistent).
- `format`: `kCTFontFormatAttribute`; an OpenType PostScript font is `cff2`
  when it has a `CFF2` table (the table directory is read for that format only).
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
  named instances of a variable font report the same list). `metrics` are
  CoreText's lengths scaled from the creation size to font units (the slant
  angle is in degrees and is not scaled). Each key is read only when the
  request wants it.

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

A face whose request needs a font (group C) is read through
`sfnt::readFontTables` (tag list from `CTFontCopyAvailableTables` in CoreText's
own order, which the reader sorts; bytes from `CTFontCopyTable`): the names,
`fvar` and `cmap` feed the fields that need them. The `opentype` value is built
from it by `sfnt::openTypeValue` with the entries of the request
(`Request::openTypeSelection`); `opentype` is attached when the request wants an
entry. The result is cached per font resource (file path and face index) for the
length of one enumeration, so the named instances of a variable font share one
`opentype` value and one axes list. The face index is found only for
`faces.file`, so a request without it shares nothing between faces; the JS layer
adds the file to the request of any face-level field.

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
  up to every axis. `namedInstanceIndex`: the first `fvar` instance whose
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

`CTFontManagerRegisterFontsForURL` / `UnregisterFontsForURL` with process scope;
`setFontEnabledForTesting` is `CTFontManagerEnableFontDescriptors` over
`CTFontManagerCreateFontDescriptorsFromURL` of the file. For a font registered for
the process the disabling holds in this process only. A failure throws `std::runtime_error` with the `CFError` description.
