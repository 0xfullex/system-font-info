# src/core

The platform-independent model and the JS boundary. No file here calls an OS font API; no backend includes `napi.h`.

## Files

| File | Role |
| --- | --- |
| `value.h` | `Value`: a JSON-like tree (null, bool, number, string, array, ordered object) plus `Shared` nodes. |
| `model.h` | `Family`, `Face` and their parts: the one result shape every backend fills. |
| `platform.h` | The interface each backend implements. |
| `field_catalogue.h` | Generated from `../../ts/field-catalogue.ts` by `../../scripts/gen-fields.mjs`; do not edit. Every selectable field with its level, on each platform whether it is provided, whether the JS layer derives it there and which groups of OS work it needs, and the tables of each `opentype` entry. |
| `to_js.h`, `to_js.cpp` | Converts a `std::vector<Family>` and `Value` trees to JS values. |
| `addon.cpp` | `NODE_API_MODULE` entry: `getFonts`, `orthographies` (fc-lang name to code point count) and the `__testing` hooks. |

## Invariants

- **Shared objects stay shared.** Within one conversion, every `Value::Shared` pointer and every `SharedAxes` pointer becomes exactly one JS object, reused by every face that references it (the memo is keyed by pointer). The JS layer relies on this to process a shared `opentype` or `axes` object once.
- **The field catalogue is the one home of field facts.** Which fields exist, their levels, per-platform availability, derivation and groups, and which tables an `opentype` entry is made of are written once, in `../../ts/field-catalogue.ts`, and reach C++ only through the generated `field_catalogue.h`. Change the catalogue and run `npm run generate`; `test/generated.test.mjs` fails when the header is stale. The header is plain standard C++20 and `clang-format off` brackets its table, so a regenerated file passes the format check as generated.
- **Keys and nesting follow `../../ts/types.ts`.** Optional fields (`file`, `file.index`, `axes`, `coordinates`, `namedInstance`, `opentype`, `coverage`) are present only when set. The native face lacks `fullName`, `styleName` and the axes' `name`; the JS layer derives them.
- **Nothing runs on the JS thread that can block.** `enumerateFonts` runs in an `AsyncWorker`. Conversion happens on the JS thread, family by family inside a handle scope, with persistent references for the shared-object memo.
- **No state between calls.** The addon keeps no per-environment or process-wide data: a `getFonts` call is an `AsyncWorker` that owns everything it creates, and nothing it computes outlives the call.
- **Errors.** Backends throw `std::exception`; `getFonts` rejects with an `Error` carrying the message. Argument errors throw `TypeError` / `RangeError` synchronously.
- `__testing.readFontFile(path, faceIndex, entries?)` runs the same table reader a backend uses, over a file, and is for tests only. `entries` is a list of `opentype` entry names to build; every entry when omitted.
