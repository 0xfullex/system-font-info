# src/platform/darwin

The macOS backend: CoreText for everything, one small AppKit file
(`appkit.mm`) for the member order. Read `../README.md` first for the contract.

## Files

| File | Role |
| --- | --- |
| `enumerate.cpp` | `enumerateFonts`: collection, grouping by PostScript name and family, member order |
| `face_reader.*` | builds one `Face`; caches the parsed tables, the axes and the collection face lookups |
| `traits.*` | pure conversions: CSS weight and width, style, scope, format |
| `variations.*` | axes, coordinates, named instance |
| `platform_value.*` | the `platform` value (`DarwinNative`) |
| `coverage.*` | character set to code point ranges |
| `legacy_name.cpp` | `decodeLegacyName` |
| `watch.cpp` | `watchFonts` |
| `testing.cpp` | the register / unregister test hooks |
| `appkit.*` | NSFontManager member lists and the autorelease pool (the only Objective-C++) |
| `cf.*` | owning CF reference and checked accessors |

## One face per PostScript name

`CTFontCollectionCreateFromAvailableFonts` (with duplicates removed and
disabled fonts included) lists a font once for each file that provides it.
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
first-seen order (the JS layer sorts). The faces of a family follow
`NSFontManager availableMembersOfFontFamily`, which is the order CoreText's own
matcher returns and the order a browser walks; faces AppKit does not list come
after, in collection order. `appKitWeight` / `appKitTraits` are the member's
values. The AppKit call was measured to return the same list on a background
thread as on the main thread, so it is made from the worker; it sits in its
own file so the rest stays C++, and runs in an autorelease pool because a
worker thread has none.

## Source of each field

- `postscriptName`: `kCTFontNameAttribute`.
- `weight`: `kCTFontWeightTrait` through the bucket table in `traits.cpp`.
  Browsers map CoreText's continuous -1..1 weights to CSS 100..900 with fixed
  boundaries; using the same ones means this library names the weight a browser
  would pick the face for. `width`: `100 + w * (w < 0 ? 50 : 100)` clamped to
  50..200 and rounded to a thousandth (CoreText keeps the trait as a 32-bit
  float, so 90% arrives as 89.99999985).
- `style`: italic symbolic bit, else a non-zero slant is oblique.
- `monospace`, `color`: symbolic bits. `symbol`: the cmap has a (3,0) subtable
  (from the tables). `enabled`: `kCTFontEnabledAttribute` (a number or a
  boolean). `hidden` and the simulations are always false: macOS has neither.
- `languages`: `kCTFontLanguagesAttribute`.
- `scope`: `kCTFontPriorityAttribute` 10000 / 20000 / 30000 is system, 40000
  user, 60000 process; any other priority falls back to
  `kCTFontRegistrationScopeAttribute` being the process scope, else unknown.
  Measured on macOS 27: the downloaded system font assets (under
  `/System/Library/AssetsV2`, `downloadable` and `downloaded` both true) also
  carry priority 60000, together with registration scope 2 (persistent);
  a font registered for the process has priority 60000 and registration scope 1.
- `format`: `kCTFontFormatAttribute`; an OpenType PostScript font is `cff2`
  when it has a `CFF2` table.
- `file.path`: `kCTFontURLAttribute`. `file.index`: CoreText does not say which
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
  `features` come from `CTFontCopyFeatures` because the attribute carries
  identifiers without names. `metrics` are CoreText's lengths scaled from the
  creation size to font units (the slant angle is in degrees and is not
  scaled); they are attached only when `tables` is on.

## Tables and sharing

Every face is read through `sfnt::readFontTables` (tag list from
`CTFontCopyAvailableTables`, bytes from `CTFontCopyTable`) whatever
`options.tables` says: the names, `fvar` and `cmap` feed fields that exist
anyway. The option only decides whether `opentype` and `platform.metrics` are
attached. The result is cached per font resource (file path and face index), so
the named instances of a variable font share one `opentype` value and one axes
list.

## Names and variations

- `familyNames`: nameID 16 with 1 as fallback per language; `styleNames`: 17
  with 2; `fullNames`: 4.
- A named instance of a variable font has no `name` records of its own for its
  full name. Its `styleNames` are the instance's subfamily name from `fvar`, and
  its `fullNames` the single full name CoreText composes for it
  (`CTFontCopyLocalizedName`, language mapped to a BCP 47 tag).
- `axes`: `kCTFontVariationAxesAttribute` (tag, range, default); the names are
  those of the `fvar` axis with the same tag, and `hidden` falls back to the
  `fvar` axis flag when CoreText's dictionary has none. `coordinates`:
  `CTFontCopyVariation`, which lists only axes away from their default, filled
  up to every axis. `namedInstance`: the first `fvar` instance whose
  coordinates all match. CoreText reports coordinates cut to four decimals, so
  the match tolerance is 5e-4.

## Legacy names

`decodeLegacyName` uses CoreFoundation string encodings and picks the encoding
the way fontTools does: the Mac encoding ID is the script code, and with
encoding 0 the record's language selects the Roman variant (Icelandic,
Turkish, Croatian, Romanian, Central European languages). Windows encodings
2-6 are stored as 16-bit units whose single-byte characters have a zero high
byte; the zero bytes are dropped and the Windows code pages (932, 936, 950,
949) decode the rest. Johab is not available in CoreFoundation and gives
`nullopt`, like every unsupported combination.

## Change notifications

`kCTFontManagerRegisteredFontsChangedNotification` is observed on the local and
on the distributed notification center, by a dedicated thread running its own
`CFRunLoop` (plain Node has no usable main run loop). The run loop stays alive
through a signalled-on-demand stop source; the destructor signals it, so a
destructor racing the thread start still ends the loop, and joins.

Measured on macOS 27 (a C++ driver, fonts registered from another thread):

- The notification arrives on the **local** center, **on the main thread**, and
  only while the main thread services its run loop (an app with an event loop,
  such as Electron). Registering or unregistering a font each posts one
  notification, for process, session and persistent scope alike.
- In a process whose main thread never runs its run loop (plain Node) nothing
  is delivered at all, to an observer on any thread and on either center.
- The distributed center never fired in these tests, including for session and
  persistent registrations made by another process.

So the watcher works where the host runs a main run loop and is silent
elsewhere; the callbacks run on the main thread, not on the watcher thread.
Callbacks run under a lock that the destructor also takes, so `onChange` must
not create or destroy a watcher synchronously.

## Test hooks

`CTFontManagerRegisterFontsForURL` / `UnregisterFontsForURL` with process scope.
A failure throws `std::runtime_error` with the `CFError` description.
