# src

The native addon: C++20, Node-API through node-addon-api, built by node-gyp (`binding.gyp` at the package root). The JS half lives in `../ts`.

## Layout

| Directory | Contents |
| --- | --- |
| `core/` | The platform-independent model, the interface a backend implements, and everything that touches JS (module init, `getFamilies`, test hooks, the request, conversion to JS values). |
| `sfnt/` | A platform-independent reader for the standard OpenType tables, and the generated language-ID tables. Backends feed it table bytes, so all platforms report the font's own metadata identically. |
| `orthography/` | The orthographies of fontconfig's fc-lang (the code points each language needs), compiled into the addon, and the lookup and counting functions over them. The three backends share them. |
| `platform/darwin`, `platform/win32`, `platform/linux` | One backend per OS. `binding.gyp` compiles exactly one of them. |

`scripts/gyp-sources.js` lists the sources of a directory for `binding.gyp`, so adding a `.cpp` file needs no build-file change.

## Contracts

These headers are the seams between the parts; change one only as its own piece of work, with every dependent checked:

- `core/value.h`: the JSON-like `Value` tree for open-ended data, with `Shared` subtrees.
- `core/model.h`: `Family`, `Face` and the small structs and enums of the result.
- `core/platform.h`: what a backend implements (`enumerateFonts`, `decodeLegacyText`, the test hooks).
- `core/request.h`: the `Request` a backend is given and the `WorkLog` it fills.
- `core/field_catalogue.h`: the generated field table (levels, per-platform availability and groups of OS work, the tables of each `opentype` entry); its source is `../ts/field-catalogue.ts`.
- `sfnt/sfnt.h`: the table reader API.
- `../ts/types.ts`: the public TypeScript types, which the native result and the JS layer must agree with.

## Data flow

1. The JS layer plans the call (which fields the platform has to fetch, `../ts/field-closure.ts`) and calls the native `getFamilies(request)`. `core/addon.cpp` parses it into a `Request` and queues an `AsyncWorker`; `platform::enumerateFonts(request, work)` runs on a worker thread, returns the requested families and fills `std::vector<sfi::Family>` and the `WorkLog`.
2. Back on the JS thread, `core/to_js.cpp` converts the vector to plain JS objects (`RawFamily[]`), outputting only what the request wants, and resolves the promise.
3. `../ts/normalize.ts` turns that into the public `FontFamily[]`.

## Who does what

The native side reports facts, exactly as the platform and the font give them. The JS side canonicalizes language tags, merges exact duplicates, derives the default strings (`fullName`, `styleName`, axis `name`), computes the family-level aggregates and sorts the families. A fact that is not available is left absent; nothing is invented, and nothing is filtered or trimmed natively. Only the output is cut to the request, in `core/to_js.cpp`.

Platform backends never talk to JS directly: they fill the model. Only `core/` includes `napi.h`.
