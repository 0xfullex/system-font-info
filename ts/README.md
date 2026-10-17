# ts

The JS half of the package, compiled by `tsc` to `../dist` (CommonJS, with declarations; `npm run build:ts` empties it first, so output of deleted sources never ships). `types.ts` is the public type contract and is never edited as a side effect of other work.

## Files

| File | Role |
| --- | --- |
| `types.ts` | Public types: `FontFamily`, `FontFace`, `OpenTypeInfo`, the platform-native shapes. |
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
