# ts

The JS half of the package, compiled by `tsc` to `../dist` (CommonJS, with declarations; `npm run build:ts` empties it first, so output of deleted sources never ships). `types.ts` is the public type contract and is never edited as a side effect of other work. `../test/types/` holds its type-level tests (a separate TypeScript project, compiled by `npm run typecheck`).

## Files

| File | Role |
| --- | --- |
| `types.ts` | Public types: the complete shapes (`FontFamily`, `FontFace`, `LanguageCoverage`, `OpenTypeInfo` and its tables, `DarwinNative`, `Win32Native`, `LinuxNative`) and the selection types (`Preset`, `Field`, `GetFamiliesOptions`, `GetFamilyOptions`, `SelectedFamily`, `SelectedFace`) that compute a result type from the options. The selection types sit between `// #region selection-types` and `// #endregion`; that text is the "Result typing" code block of `docs/api.md`, and the two stay identical. |
| `field-catalogue.ts` | `FIELD_CATALOGUE`: every field name with its level, smallest preset, identity flag, `opentype` tables and per-platform source: groups of OS work, `constant`, or `derivedFrom` (the package computes the field from other fields on that platform; derivation is per platform because a platform may get the same field from the OS). The one source of field facts: `../scripts/gen-fields.mjs` generates `../src/core/field_catalogue.h` and the generated regions of `../docs/api.md` and `../docs/platforms.md` from it, and `../test/types/catalogue.test.ts` asserts that its names are exactly `Field` and its presets exactly `PresetFields`. |
| `field-closure.ts` | `closureOf(selected, platform)`: the fields a call fetches on a platform (selected, plus the face identity when a `faces.*` field is selected, plus the inputs of derived fields transitively, minus the derived fields themselves), and `groupsOf(closure, platform)`: the groups of OS work that costs. The one home of the closure rule: the request builder calls it, and `../scripts/gen-fields.mjs` calls it to generate the table of groups each preset reaches. `../test/catalogue.test.mjs` tests it. |
| `native.ts` | The raw shapes the addon returns (`RawFamily`, `RawFace`, `RawAxis`), the binding interface, and the loader. |
| `normalize.ts` | `normalizeFamilies`: raw native result to the public `FontFamily[]`. |
| `pick-name.ts` | `pickName`: choose a localized string for an ordered list of locales. |
| `index.ts` | The public entry: `getFonts`, `pickName`, and all public types. |

## Invariants

- **The addon is loaded lazily**, on the first `getFonts` call, from `prebuilds/<platform>-<arch>/system-font-info.node` and then `build/Release/`; the error lists the paths tried. `normalize.ts` and `pick-name.ts` never import the loader, so they can be tested without a binary.
- **Normalization mutates in place.** Faces that share an `opentype` or `axes` object in the native result share it afterwards, so each shared object is processed once (tracked with a `WeakSet`). Do not clone.
- **Canonicalize, then merge.** Every language tag goes through `Intl.getCanonicalLocales`, one tag at a time and cached; `""` stays `""`, a tag Intl rejects is kept as written. After that, localized arrays merge exact `(language, value)` duplicates and string arrays merge equal values, keeping the first occurrence and the source order. `opentype.names` is a raw view and is never merged.
- **Nothing is dropped.** Only exact duplicates are merged and non-standard forms are normalized.
- **Sorting is locale-independent**: families compare by `family.toLowerCase()` in UTF-16 code units, ties by the raw string. No `localeCompare`, so the order is the same on every machine.
- Errors from `getFonts` are promise rejections (the function is `async`).
