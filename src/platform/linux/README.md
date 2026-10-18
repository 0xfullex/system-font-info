# src/platform/linux

The Linux backend uses fontconfig for enumeration and attributes, the shared
sfnt reader for font tables, and FreeType for WOFF / WOFF2 table access. Read
`../README.md` first for the backend contract. Building requires fontconfig
2.15 or newer for the `FC_FONT_WRAPPER` attribute, plus FreeType and pkg-config.

## Files

| File | Role |
| --- | --- |
| `enumerate.cpp` | Fresh configuration, object set, family queries, member order and default-face language coverage |
| `fc.h` | Owning fontconfig handles, checked attributes and file / index identities |
| `face_reader.*` | Requested face fields, per-call resource sharing, table reads, variations and character ranges |
| `traits.*` | Path-based scope and fontconfig format classification |
| `legacy_text.cpp` | Legacy name decoding with the system's iconv converters |
| `testing.*` | Process-specific test directory and application-font registration in each private configuration |

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
built-in orthographies. If matching fails, the first member stands in.

## Work and sharing

Scalar attributes cost no table reads. A resource is opened only for a field
that needs C or D work, or for a variable face's coordinates / names. An sfnt
or collection is read through `sfnt::FontFile`; only WOFF / WOFF2 use
`FT_Sfnt_Table_Info` and `FT_Load_Sfnt_Table`. Type 1 and bitmap faces have no
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

Legacy names use iconv without approximate substitutions. The converter maps
all `LegacyEncoding` enumerators explicitly; a Mac encoding the system lacks
returns `nullopt`, so the shared reader preserves its bytes.

## Test hooks

Registration first validates the file with `FcConfigAppFontAddFile` on an
independent empty configuration. It then creates a symlink in the temporary
directory `system-font-info-test-<uid>-<pid>`. Each enumeration adds that
directory, if present, through `FcConfigAppFontAddDir` to its private
configuration. Unregister removes the link and removes an empty directory.
No static registration list or environment change is needed; a child has a
different PID and does not inherit the registrations. `FC_FILE` is the link's
path, so these fonts have `unknown` scope under the usual path rule.

`setFontEnabledForTesting` throws "not supported": fontconfig lists a font or
does not, and has no disabled state to turn on and off.
