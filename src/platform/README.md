# src/platform

One backend per operating system, each in the directory named after
`process.platform`: `darwin/` (CoreText), `win32/` (DirectWrite), `linux/`
(fontconfig). `binding.gyp` compiles exactly one of them. A backend implements
the interface in `../core/platform.h` and fills the structs of
`../core/model.h`; nothing else in the addon knows which OS it runs on.

## The contract

- `enumerateFonts(request, work)` lists the installed fonts as `Family` /
  `Face` structs. It runs on a worker thread, never the JS thread, and throws a
  `std::exception` when the OS font service fails. It reflects the fonts
  installed at the moment of the call: a backend keeps nothing between two
  calls, no cache, no handle to the OS font service, no thread.
  - It returns only the families for which `request.wantsFamily` is true, and
    every one of them with the faces the full listing gives it: which families
    are asked for never changes a family's faces. A backend that gets the faces
    of the asked families from a narrower OS query than the full listing uses
    (darwin's family lookup) relies on that query naming every face of the
    family. The one known exception, faces the lookup does not name (none was
    found on the machine measured), is in `docs/platforms.md`, and the test
    "asking for every family name of a full run gives the full run" in
    `test/darwin.test.mjs` is the check.
  - It fills what the request wants: `request.wants(field)`,
    `wantsAnyOpenType()` (the `opentype` object is built from
    `request.openTypeSelection`, never `OpenTypeSelection::all()`: it is the
    one thing `core/to_js.cpp` does not cut to the request, see below),
    `request.orthographies` (the family's
    `languageCoverage`, in that order). A field the platform derives is never
    requested. Work of a group (`docs/platforms.md`, "Source groups") that no
    requested field reaches is wasted, and work a requested field needs is the
    defect to avoid skipping. `request.needs(group)` and `mayNeed(group)` say
    which groups the request always or perhaps calls for; they are offered for
    decisions that are made per group (a backend whose work is one object set
    per group, such as the fontconfig object set on Linux). The darwin backend
    decides per field with `wants` and does not call them. The output is cut to
    the request by `core/to_js.cpp`, not by the backend, so extra work costs
    time but never changes a result.
  - It adds to `work` each costly unit of work as it does it: a font object or
    file opened for a face (`fontsOpened`), an `opentype` entry built
    (`tableEntries`), a character set fetched and walked (`characterSets`).
- `decodeLegacyText(encoding, bytes)` converts bytes in a legacy character
  encoding (`sfnt::LegacyEncoding`: Mac script encodings, Windows multi-byte
  code pages) to UTF-8 with the OS's own converter. It is only the converter:
  which encoding a `name` record uses, and how its bytes are stored, is decided
  once in `../sfnt` for all platforms. It returns `nullopt` for an encoding the
  OS has no converter for, and must be thread-safe. A backend must handle every
  enumerator, so a new one is a compile error until each backend maps it.
- `registerFontForTesting` / `unregisterFontForTesting` make a font file
  available to the current process only, through the service enumeration reads
  from, and throw `std::runtime_error` with the OS error text on failure.
- `setFontEnabledForTesting(path, enabled)` turns the fonts of a file registered
  that way off or on again, for the current process only, so a test can have a
  disabled font without touching the machine's fonts. A platform with no
  disabled state (DirectWrite, fontconfig) throws "not supported".

## What a backend must do

- Report what the OS says, in full: every font the OS lists (of the requested
  families), every language it names, every attribute it exposes. Fields the OS has no notion of stay absent
  or carry the truthful value; nothing is filled in with an invented default.
- Read the font's own tables through `../sfnt` (`readFontTables`, `openTypeValue`), feeding it
  the bytes the OS hands out, so the `opentype` object and the names are
  identical on every platform. The platform-specific raw values go into
  `Face::platform`, a `Value` whose first key is `"os"` and whose shape is the
  matching `*Native` type in `ts/types.ts`.
- Keep the platform's own order of families' members (`Family::faces`): the
  order the platform lists them, never a sort of the backend's own.
- Share what is shared: faces that come from one font resource (named
  instances of a variable font) point at the same `opentype` value and the same
  `SharedAxes`.

## What a backend must not do

- Filter, trim or hide fonts, even ones that look private or broken. Only exact
  duplicates (the same font listed twice by the OS) may be merged. Selecting
  families by name is the one filter, and it is `request.wantsFamily`'s.
- Cut the output to the request: `core/to_js.cpp` does, once, for all platforms.
  The exception is the `opentype` object, which `to_js.cpp` outputs whole: the
  backend builds it from `request.openTypeSelection`, because that is the cut
  (it is also what avoids reading the tables of unrequested entries and keeps
  their errors out of `errors`).
- Canonicalize language tags, merge or de-duplicate names across records, pick
  default strings, compute family-level aggregates or sort. The JS layer owns
  all of that, so the rules live in one place.
- Throw away information the platform layer cannot judge: if two OS entries
  carry the same identity, the one that is not used is reported (see
  `platform.shadowed` on macOS), not dropped.

## Layout

| Directory | Status |
| --- | --- |
| `darwin/` | implemented; see its README |
| `win32/`, `linux/` | stubs that throw "not implemented yet" |
