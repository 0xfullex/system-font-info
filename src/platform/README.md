# src/platform

One backend per operating system, each in the directory named after
`process.platform`: `darwin/` (CoreText), `win32/` (DirectWrite), `linux/`
(fontconfig). `binding.gyp` compiles exactly one of them. A backend implements
the interface in `../core/platform.h` and fills the structs of
`../core/model.h`; nothing else in the addon knows which OS it runs on.

## The contract

- `enumerateFonts(options)` lists every installed font as `Family` / `Face`
  structs. It runs on a worker thread, never the JS thread, and throws a
  `std::exception` when the OS font service fails. It reflects the fonts
  installed at the moment of the call: a backend keeps nothing between two
  calls, no cache, no handle to the OS font service, no thread.
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

## What a backend must do

- Report what the OS says, in full: every font the OS lists, every language it
  names, every attribute it exposes. Fields the OS has no notion of stay absent
  or carry the truthful value; nothing is filled in with an invented default.
- Read the font's own tables through `../sfnt` (`readFontTables`), feeding it
  the bytes the OS hands out, so the `opentype` object and the names are
  identical on every platform. The platform-specific raw values go into
  `Face::platform`, a `Value` whose first key is `"os"` and whose shape is the
  matching `*Native` type in `ts/types.ts`.
- Keep the platform's own order of families' members (`Family::faces`): it is
  the order a browser on that platform walks them.
- Share what is shared: faces that come from one font resource (named
  instances of a variable font) point at the same `opentype` value and the same
  `SharedAxes`.

## What a backend must not do

- Filter, trim or hide fonts, even ones that look private or broken. Only exact
  duplicates (the same font listed twice by the OS) may be merged.
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
