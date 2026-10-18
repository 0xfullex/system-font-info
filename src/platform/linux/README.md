# src/platform/linux

The Linux backend uses fontconfig for enumeration and attributes, the shared
sfnt reader for font tables, and FreeType for WOFF / WOFF2 table access. Read
`../README.md` first for the backend contract. Building requires fontconfig
2.13 and FreeType 2.9 or newer (the versions of Ubuntu 22.04 and manylinux_2_28
are the floor) and pkg-config; `binding.gyp` checks them. `fc.h` defines the
object names `FC_FONT_WRAPPER` (2.15), `FC_ORDER` and `FC_FONT_HAS_HINT` (2.14)
when the headers lack them, so a binary built against 2.13 still reports those
platform fields on a newer runtime and leaves them absent on an older one.

## Files

| File | Role |
| --- | --- |
| `enumerate.cpp` | Fresh configuration, object set, family queries, member order and default-face language coverage |
| `fc.h` | Owning fontconfig handles, checked attributes, file / index identities and the lock for old fontconfig |
| `face_reader.*` | Requested face fields, per-call resource sharing, table reads, variations and character ranges |
| `traits.*` | Scope (application set, else path-based) and fontconfig format classification |
| `legacy_text.cpp` | Legacy name decoding with the system's iconv converters |
| `testing.*` | In-process registry of the test fonts, added as application fonts to each private configuration |

## Configuration and order

Every enumeration owns a new `FcInitLoadConfigAndFonts` configuration and
destroys it before returning. It never changes fontconfig's global current
configuration or the environment. The fixed object set contains the scalar
objects used by the public fields plus `FC_FILE`, `FC_INDEX` and `FC_VARIABLE`.
`FC_LANG` is requested only for supported languages; `FC_CHARSET` only for
face or language coverage. Queries set `FC_NAMELANG` to `en`.

A selected family name gets its own `FcFontList` query: several `FC_FAMILY`
values in one query mean intersection. Results are merged by file and packed
index and selected by `Request::wantsFamily`. The list's hash order changes
with the object set, so fixed-coordinate patterns follow the configuration's
system font set, then its application font set. A resource with a range
pattern (`FC_VARIABLE` true) is variable; the range pattern is not a face.

Language coverage matches regular weight, normal width and roman slant
inside the family's original fixed-coordinate configuration patterns. The
originals carry match properties that `FcFontList` does not copy; matching the
projected patterns would let object selection change the default face.
`FcConfigSubstitute` and `FcDefaultSubstitute` prepare the query, and
`FcFontSetMatch` selects a member. Its charset is counted against the package's
built-in orthographies. A match that fails (the set is not empty, so only memory
exhaustion can) fails the call. `FcConfigSubstitute` adds a weak query language
from `FC_LANG`, `LC_ALL`, `LC_CTYPE` or `LANG`, and the matcher ranks language
before slant, weight and width; so the query's language is replaced by `en`
after the substitution, and the default face is the same in every locale.

## Work and sharing

Scalar attributes cost no table reads. A resource is opened only for a field
that needs C or D work, or for a variable face's coordinates / names. An sfnt
or collection is read through `sfnt::FontFile`; a file that fails to open
there but starts with the `wOFF` or `wOF2` signature is a WOFF / WOFF2 file
and is read with `FT_Sfnt_Table_Info` and `FT_Load_Sfnt_Table`. The signature
decides, not `FC_FONT_WRAPPER`, which a fontconfig older than 2.15 does not set. Type 1 and bitmap faces have no
OpenType object. The first read of a resource builds `sfnt::FontTables`, and
`sfnt::openTypeValue` builds only `Request::openTypeSelection`.

The resource cache belongs to one enumeration and is keyed by path and the
low 16 bits of `FC_INDEX`. Its axes and OpenType object are shared by all
instances. The high bits select the fvar coordinates (zero means defaults).
`namedInstanceIndex` is found independently by the first equal coordinate
vector, since a default pattern may also name an fvar instance. An instance's
style names and composed full names come from the shared name and fvar tables.

`WorkLog` counts each opened resource, each selected OpenType entry built,
each face charset walked and each family charset used for language coverage
at the operation that performs the work.

Legacy names are converted by glibc iconv, without approximate substitutions.
`legacy_text.cpp` decides statically only which iconv converter name an
encoding uses, for every `LegacyEncoding` enumerator: Mac Roman, Icelandic,
Central European and Cyrillic, and the Windows code pages 932, 936, 950, 949
and 1361 (Johab) have one; every other Mac encoding has none and returns
`nullopt` without calling iconv. Whether the converter exists on the system and
whether the bytes convert is iconv's answer: an unknown converter or a failed
conversion also returns `nullopt`, and the shared reader then preserves the
bytes.

## Unreadable files and concurrency

A file the backend cannot read (it cannot be opened, is not a font the sfnt
reader or FreeType understands, or its variation table is unusable) never
fails the call. The resource has no tables: `opentype`, `axes` and the
coordinates and named-instance fields that need the file are absent for its
faces, while everything fontconfig reports stays. A table that cannot be
fetched or parsed from a readable file is recorded by the sfnt layer in
`opentype.errors`. A variable resource whose `fvar`
cannot be read gives its named instances no coordinates, and the identity
(`file.path`, `file.index`, `coordinates`, `simulations`) could not tell them
apart; so the resource lists one face, its default instance (no named-instance
bits in `FC_INDEX`, else the first in member order), and omits its other named
instances. `FaceReader::lists` is the one place that rule lives.

On a fontconfig older than 2.14 (`FcGetVersion() < 21400`), which had data
races in loading a configuration and handling patterns, `enumerateFonts`
holds a process-wide mutex for the whole call, so overlapping calls take
turns; registering a test font takes the same mutex around its validation.
Newer versions run concurrently. The mutex lives in `fc.h`, so any code that
loads a configuration or handles patterns takes it from there.

## Test hooks

The registry is a mutex-protected list of absolute paths in the process's
memory; it holds no state in the file system, where another local user could
plant a font directory that a real call would then list. Registering a file
first validates it with `FcConfigAppFontAddFile` on an independent empty
configuration, then adds its path to the list (a path already listed stays
listed once). Unregistering removes the path and throws when it is not listed.
The registered file must stay in place until it is unregistered: each
enumeration re-adds it, and a file that was deleted meanwhile makes
`FcConfigAppFontAddFile` fail, so every later call in the process throws until
the path is unregistered. The failure is deliberate; it exposes a test that
removed its font too early.
Each enumeration adds every listed path to its private configuration with
`FcConfigAppFontAddFile`, which makes them the configuration's application
set, the only fonts that set holds. A child process starts with an empty list.

Fonts of the application set have `process` scope; every other font gets its
scope from its path. `FC_FILE` is the registered path as fontconfig
normalizes it.

`setFontEnabledForTesting` throws "not supported": fontconfig lists a font or
does not, and has no disabled state to turn on and off.
