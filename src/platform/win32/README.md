# src/platform/win32

The Windows backend uses DirectWrite's weight/stretch/style (WSS) family
model. Read `../README.md` first for the backend contract. It requires the
DirectWrite interfaces through `IDWriteFactory6` (Windows 10 version 1709) and
`IDWriteFontSet3` (version 1809); test registration also uses
`IDWriteFontSetBuilder2`. The minimum system is therefore **Windows 10 version
1809 (build 17763)**, and there is no fallback for earlier builds: creating
the factory or querying `IDWriteFontSet3` gets `E_NOINTERFACE` there, which
`checkRequiredInterface` turns into an error naming that requirement. The
backend has been executed on Windows 11 only.

## Files

| File | Role |
| --- | --- |
| `backend.cpp` | `enumerateFonts`: collection/family lookup, exact filtering, member order, duplicate handling, default-face language coverage |
| `collection.*` | One call's collection, optional test-font collection, lazy resource variable classification and system-source joins |
| `face_reader.*` | Requested face fields, lazy resource/file access, shared axes and selected OpenType values |
| `dwrite.*` | COM ownership, checked Windows calls, UTF conversion, localized strings, references, registry preferences and coverage |
| `legacy_text.cpp` | `decodeLegacyText`, using exact Windows code-page counterparts |
| `testing.*` | Process-local test registration paths and DirectWrite validation |

## Collections and identity

Every enumeration acquires a factory interface and a new system collection,
with downloadable fonts excluded and `checkForUpdates = FALSE`. The
`IDWriteFactory3::GetSystemFontCollection` overload fixes the WSS model and
exposes that update-check parameter. The `IDWriteFactory6` overload has a
different signature: its BOOL controls downloadable fonts, not update checks.
No factory, collection, source index, font object or parsed data is kept by
the backend after the call. DirectWrite may retain its own shared state.

The canonical family string is the family's `en-us` name, else its first
name. The family-level and face-level `familyNames` both come from
`GetFamilyNames`. A `families` request first uses `FindFamilyName` for
candidates, then applies `Request::wantsFamily`: DirectWrite also accepts
case-insensitive and localized aliases, which the public request does not.
Selecting families never changes their members or member order.

Every WSS member is a face, including simulations. The physical resource
key is the local file path and collection index; a nonlocal reference instead
uses its loader and opaque reference key internally. Local file paths are
reported exactly as DirectWrite returns them. A system-font-set index maps
each resource to its source metadata, so a simulated member inherits its
base resource's source without trying to match simulation flags in the set.
Conflicting source metadata or variable classifications for one resource
fail explicitly.

Within a family, repeated identities (resource, actual `fvar` coordinates,
simulations) are merged at their first member position only after comparing
all per-member inputs: classification, face names, every supported
informational string and ordered reference axis values. Equal identities
already share their physical table data and coverage inputs. Different
member metadata at one identity is an error, never a reason to choose one.
A family-only aggregate can use hidden intermediate faces without the
public identity; their raw reference axis values distinguish instances
without opening `fvar` merely to compute that aggregate.

## What the request changes

`core/to_js.cpp` gates the output; this backend gates the work. `WorkLog`
counts each costly operation where it occurs.

- **A** lists families and their names. The inactive-family registry
  preference is read once only when requested. A missing `Inactive Fonts`
  key means an empty preference set; no registry value is changed.
- **B** reads requested member attributes. Variable classification is a
  non-empty range from `IDWriteFontSet1::GetFontAxisRanges`, without creating
  a resource or font face. Reference paths, collection indices, simulation
  flags, raw axis values and source metadata also need no opened face.
- **C** creates a font face for the first field that needs one. Table-backed
  fields use the existing `sfnt::FontFile` reader for local directory tags and
  `TryGetFontTable` for bytes, with the DirectWrite tag byte order converted
  at the boundary. Directory readers are cached by file; faces and parsed
  tables by file/index. Raw `platform.format` needs only a face's `GetType`.
  `platform.resourceAxes` creates its `IDWriteFontResource` only on demand.
  Each created face/resource and each directory file opened increments
  `fontsOpened`. Static coordinates are absent and never open tables.
- **D** builds only `Request::openTypeSelection`, once per resource, and
  increments `tableEntries` by the number of selected entries. A failed
  table fetch returns no bytes to the shared reader, which records the
  failure only for selected entries. Instances and simulations share one
  `opentype` object and one `axes` array per resource.
- **E** reads `IDWriteFont1::GetUnicodeRanges`, with no explicit face creation.
  `faces.coverage` costs one character-set read per visited member;
  `languageCoverage` costs one per family, regardless of language count.
  Ranges are inclusive, ascending, and adjacent ranges are joined.

Variable coordinates contain only `fvar` axes, in axis order; standard axes
DirectWrite injects stay in `platform.axisValues` and `resourceAxes`.
The first named instance within 1/65536 on every axis supplies
`namedInstanceIndex`. Names come from DirectWrite as it names each member;
the shared JS layer canonicalizes language tags, merges localized duplicates
and chooses default strings. `additionalInformationalStrings` keeps the
localized DirectWrite values the `name` records do not reproduce, comparing
language tags without ASCII case when finding an existing record.

## Language coverage

`GetFirstMatchingFont(400, NORMAL, NORMAL)` selects the family's default
face. Its `IDWriteFont1::GetUnicodeRanges` is read once; the shared
`orthography::countCovered` counts each requested orthography against those
ranges in request order. A language-coverage-only request creates no face,
resource, directory reader or OpenType value.

## Legacy names

`decodeLegacyText` maps each `LegacyEncoding` to its exact Windows code page
and converts with `MultiByteToWideChar`. The mappings are the
[Microsoft code-page identifiers](https://learn.microsoft.com/en-us/windows/win32/intl/code-page-identifiers),
including Mac Roman, Icelandic, Turkish, Croatian, Romanian and Central
European. A Mac encoding with no exact Windows counterpart, an unavailable
code page, or invalid bytes gives `nullopt`; the shared reader retains the
undecoded bytes. A same-language Windows or ISCII encoding is not substituted.

## Test hooks

The hooks do not install fonts, write the registry, or use
`AddFontResourceEx(FR_PRIVATE)`, which the system collection does not list.
They validate an absolute path with DirectWrite and keep only those test
paths behind a mutex. Each enumeration takes a snapshot and, when nonempty,
uses `IDWriteFontSetBuilder2::AddFontSet` and `AddFontFile` to combine the
system set with those files, then `CreateFontCollectionFromFontSet` with the
WSS model. Every member still passes through the same enumeration and reader.
This follows DirectWrite's
[custom-font-set APIs](https://learn.microsoft.com/en-us/windows/win32/directwrite/custom-font-sets-win10).

`setFontEnabledForTesting` always throws "not supported": DirectWrite has no
disabled state, so a font is either in the font set or absent.

Test resources have `scope: 'process'`; their raw system-source fields are
absent unless that physical resource is also in the system set. Registration
membership uses Windows ordinal case-insensitive path comparison, because
DirectWrite can change path case; it does not rewrite the public path.
The next call sees a hook registration or unregistration immediately.
These test paths are the backend's only process state; production font
enumeration retains none.
