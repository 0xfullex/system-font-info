# system-font-info

List the font families installed on a system, with full metadata, through the operating system's own font service: CoreText on macOS, DirectWrite on Windows, fontconfig on Linux. It is a Node-API addon with prebuilt binaries; nothing is compiled when you install it.

Every platform returns one shape, and the caller declares which fields it wants: localized family, full and style names in every language the font carries, weight, width and style on one scale, supported languages, variable-font axes and named instances, file location, the font's own OpenType tables, character coverage, and each platform's raw values.

## Status

| Platform | State |
| --- | --- |
| macOS | implemented |
| Windows | implemented |
| Linux | in development |

## Install

```sh
npm install system-font-info
```

Node.js 18 or newer. On Windows, Windows 10 version 1809 (build 17763) or later: the backend uses DirectWrite interfaces that earlier builds lack, has no fallback for them, and reports that requirement as its error. The package ships a binary per platform and architecture under `prebuilds/<platform>-<arch>/`. If no binary matches (or you built from source, see below), it falls back to `build/Release/`.

## Usage

```js
const { getFamilies, getFamily, pickName } = require('system-font-info')

async function main() {
  // Family names in every language. (The cheapest call is `fields: []`: the `family` name only.)
  const families = await getFamilies({ preset: 'names' })
  for (const family of families) {
    console.log(family.family, pickName(family.familyNames, ['zh-CN', 'en']))
  }

  // Chosen fields of one family's faces.
  const helvetica = await getFamily('Helvetica Neue', { fields: ['faces.postscriptName', 'faces.weight', 'faces.style'] })
  console.log(helvetica?.faces)
}

main()
```

`fields` or `preset` is required ([why](docs/api.md#options)); `preset: 'full'` returns everything except character coverage.

## Documentation

- [docs/api.md](docs/api.md): the functions, options, presets, every field, and the rules the results follow.
- [docs/platforms.md](docs/platforms.md): where each field comes from on each platform, what is listed, how quickly font changes show, and measured costs.

## Electron

Use the package from the main process only; load it from the renderer through IPC. Native addons cannot be loaded from inside an asar archive, so unpack it:

```json
{
  "build": {
    "asarUnpack": ["**/node_modules/system-font-info/**"]
  }
}
```

(`asarUnpack` is the electron-builder option; use the equivalent setting of your packager.)

## Building from source

Requires Node.js, a C++20 compiler and Python (for node-gyp). On macOS, the Xcode command line tools.

```sh
npm install
npm run build          # node-gyp rebuild, then tsc
npm run copy-prebuild  # optional: build/Release/*.node -> prebuilds/<platform>-<arch>/
npm test
```

`npm run copy-prebuild -- --arch x64` names the target architecture of a cross build.

## Development

```sh
npm run typecheck      # tsc --noEmit for two projects: the package and the type-level tests in test/types
npm run lint           # Biome lint of the TypeScript and JavaScript
npm run format:check   # Biome and clang-format, check only
npm run format         # Biome and clang-format, rewrite the files
npm run generate       # regenerate the generated files: the field catalogue outputs and the orthography data
```

The field catalogue, `ts/field-catalogue.ts`, is the one source for the fields' levels, presets, platform availability and groups of OS work. `npm run generate` compiles the TypeScript sources and runs `scripts/gen-fields.mjs`, which writes from it the native field table `src/core/field_catalogue.h` and the generated regions of `docs/api.md` (the Presets table and the result types, copied from `ts/types.ts`) and `docs/platforms.md` (the group of every field, and the groups each preset reaches); those regions sit between `<!-- BEGIN GENERATED -->` and `<!-- END GENERATED -->` lines and are not edited by hand. `npm test` fails when a generated file is out of date (`node scripts/gen-fields.mjs --check` lists the stale files).

TypeScript and JavaScript are formatted and linted by [Biome](https://biomejs.dev) (`biome.json`), installed with `npm install`. The C, C++ and Objective-C++ sources are formatted by clang-format (`.clang-format`), which is not an npm dependency: install **major version 20** and put it on your `PATH`. `scripts/clang-format.mjs` stops with a message when the version found is another major, because clang-format changes its output between majors.

- macOS: `brew install clang-format` installs the current major, which may be newer than 20 later on; check that `clang-format --version` prints 20.x.
- Other platforms: the LLVM 20 release, or `pipx run clang-format==20.1.8` (the `clang-format` wheel on PyPI).

`src/sfnt/language_tables.cpp` is generated and passes the format check as generated; `scripts/gen-language-tables.c` marks its tables with `clang-format off` / `on` so a regenerated file needs no formatting pass.

To make `git blame` skip the commit that formatted the whole tree, run `git config blame.ignoreRevsFile .git-blame-ignore-revs` once per clone.

## License

MIT. The language orthographies built into the addon are derived from [fontconfig](https://gitlab.freedesktop.org/fontconfig/fontconfig)'s `fc-lang` files; its notice ships with the package as `third_party/fontconfig/COPYING`.
