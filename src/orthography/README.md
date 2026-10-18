# src/orthography

The orthographies of fontconfig's `fc-lang`, built into the addon: for each of 284 languages and scripts, the code points its writing system needs. `languageCoverage` counts how many of them a family's default face maps to a glyph. Platform-independent and free of OS and JS calls, so all three backends use the same data; only `../core/addon.cpp` exposes it to JS.

## Files

| File | Role |
| --- | --- |
| `orthography.h` | The API: `Orthography`, `find(name)`, `all()`, `countCovered(orthography, has)`. |
| `orthography.cpp` | `find` and `all`. |
| `orthography_data.cpp` | Generated: the table of orthographies and their ranges. Never edited by hand. |

## Invariants

- **The data is fontconfig's.** `orthography_data.cpp` is compiled from `../../third_party/fontconfig/fc-lang/*.orth` by `../../scripts/gen-orthographies.mjs`; nothing else defines an orthography. A name is the file name with `_` replaced by `-` (`zh_cn.orth` is `zh-cn`), as fontconfig's `fc-lang.py` names it.
- **Ranges are canonical.** The table is ascending by name (`find` is a binary search) and each orthography's ranges are inclusive, ascending, non-overlapping and non-adjacent; `codePointCount` is the sum of their sizes. `includes` are expanded and deleting lines (`- XXXX`) applied in file order.
- **Counting calls the predicate once per code point.** `countCovered` takes the predicate as a template argument, so a backend passes its own lookup (a `CFCharacterSet` test, a cmap, an `FcCharSet`) without a copy of the code points.
- **The generator asserts the counts the contract names** (`zh-cn` 6,765, `zh-tw` 13,063, `ja` 2,314) and fails generation otherwise.
- Standard C++20 only, so every backend compiles it.
- The compiled data is redistributed with the package, so fontconfig's notice ships with it: `third_party/fontconfig/COPYING` is in the package's `files`. The `.orth` files are not.

## Regenerating

```sh
npm run generate:orthographies          # rewrite orthography_data.cpp
node scripts/gen-orthographies.mjs --check   # fail (exit 1) when it is stale; writes nothing
```

`test/generated.test.mjs` runs the check. To move to another fontconfig commit, replace the `.orth` files and the commit in `third_party/fontconfig/README.md` (the generator reads the commit from there), then regenerate. The generated file brackets its tables with `clang-format off` / `on`, so it passes `npm run format:check` as generated.
