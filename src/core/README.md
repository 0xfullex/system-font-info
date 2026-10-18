# src/core

The platform-independent model and the JS boundary. No file here calls an OS font API; no backend includes `napi.h`.

## Files

| File | Role |
| --- | --- |
| `value.h` | `Value`: a JSON-like tree (null, bool, number, string, array, ordered object) plus `Shared` nodes. |
| `model.h` | `Family`, `Face` and their parts: the one result shape every backend fills. |
| `platform.h` | The interface each backend implements. |
| `request.h`, `request.cpp` | `Request`: the fields, families and orthographies one call asks for, with the questions backends and the conversion ask of it. `WorkLog`: what a call did. `makeRequest` builds a request from names. |
| `field_catalogue.h` | Generated from `../../ts/field-catalogue.ts` by `../../scripts/gen-fields.mjs`; do not edit. Every selectable field with its level, on each platform whether it is provided, whether the JS layer derives it there and which groups of OS work it needs, and the tables of each `opentype` entry. |
| `to_js.h`, `to_js.cpp` | Converts a `std::vector<Family>` and `Value` trees to JS values, outputting only what the request wants. |
| `addon.cpp` | `NODE_API_MODULE` entry: `getFamilies`, `orthographies` (fc-lang name to code point count) and the `__testing` hooks. Parses the request object (`parseRequest`). |

## The request

`getFamilies(request)` takes the plan the JS layer made (`../../ts/field-closure.ts`), an internal contract, not a public one:

```ts
interface NativeRequest {
  fields: string[] // catalogue names the platform fetches itself; fields it derives are not in it
  families?: string[] // omitted: every family; []: none
  orthographies: string[] // fc-lang names; the order of Family.languageCoverage
}
```

`parseRequest` (addon.cpp) reads the object and `makeRequest` (request.cpp, plain C++) builds the `Request`. An unknown field or orthography name, or a field the platform derives, is a defect of the caller and makes the call throw a plain `Error` synchronously (the JS layer's `async` wrapper turns it into a rejection). Fields the platform does not provide are dropped (`Request::fields` is the closure intersected with the platform's availability); `platformObject` is decided before that, so a request naming any `faces.platform.*` field gets the `platform` object.

`Request` answers every question a backend or the conversion has: `wants(field)`, `wantsAnyFace()`, `wantsAnyOpenType()`, `wantsFamily(name)` (the one exact, case-sensitive match), `needs(group)` and `mayNeed(group)` (the groups of OS work the requested fields always or perhaps need; A always, every call lists the families), and `openTypeSelection` for `sfnt::openTypeValue`.

## Output gating

`to_js.cpp` is the one place that decides what reaches JS, and the only one: a key is output when `request.wants` its field. `faces` only when `wantsAnyFace()`, `platform` (with `os` and the requested platform fields only) when `platformObject`, `opentype` when `wantsAnyOpenType()`, `languageCoverage` when the request names orthographies, the family-level `familyNames` when requested. Backends never filter output: computing more than asked is harmless (and only slow), computing less is the defect. The one exception is `opentype`, whose entries the backend builds from `Request::openTypeSelection`, because that is what avoids reading the tables, and so that `errors` lists only the tables of the requested entries.

## `WorkLog`

A call's costly OS work, counted by the backend as it does it: `fontResources` (C: a font object created or a font file opened to read a face), `tableEntries` (D: `opentype` entries built) and `characterSets` (E: character sets fetched and walked). Groups A and B are not counted (cheap; the line between them is not observable on every platform). The counts exist for tests: `__testing.getFamiliesWithWork(request)` resolves to `{ families, work }`. A `WorkLog` lives for one call, so the no-state rule below holds.

## Invariants

- **Shared objects stay shared.** Within one conversion, every `Value::Shared` pointer and every `SharedAxes` pointer becomes exactly one JS object, reused by every face that references it (the memo is keyed by pointer). The JS layer relies on this to process a shared `opentype` or `axes` object once.
- **The field catalogue is the one home of field facts.** Which fields exist, their levels, per-platform availability, derivation and groups, and which tables an `opentype` entry is made of are written once, in `../../ts/field-catalogue.ts`, and reach C++ only through the generated `field_catalogue.h`. Change the catalogue and run `npm run generate`; `test/generated.test.mjs` fails when the header is stale. The header is plain standard C++20 and `clang-format off` brackets its table, so a regenerated file passes the format check as generated.
- **Keys and nesting follow `../../ts/types.ts`.** Optional fields (`file`, `file.index`, `axes`, `coordinates`, `namedInstanceIndex`, `opentype`, `coverage`) are present only when set and requested. The native face lacks `fullName`, `styleName` and the axes' `name`; the JS layer derives them.
- **Nothing runs on the JS thread that can block.** `enumerateFonts` runs in an `AsyncWorker`. Conversion happens on the JS thread, family by family inside a handle scope, with persistent references for the shared-object memo.
- **No state between calls.** The addon keeps no per-environment or process-wide data: a `getFamilies` call is an `AsyncWorker` that owns everything it creates, and nothing it computes outlives the call.
- **Errors.** Backends throw `std::exception`; `getFamilies` rejects with an `Error` carrying the message. Argument errors throw `TypeError` / `RangeError` synchronously.
- `__testing.readFontFile(path, faceIndex, entries?)` runs the same table reader a backend uses, over a file, and is for tests only. `entries` is a list of `opentype` entry names to build; every entry when omitted.
