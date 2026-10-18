# system-font-info

List the font families installed on a system, with full metadata, through the operating system's own font service: CoreText on macOS, DirectWrite on Windows, fontconfig on Linux. It is a Node-API addon with prebuilt binaries; nothing is compiled when you install it.

Every platform returns one shape, and the caller declares which fields it wants: localized family, full and style names in every language the font carries, weight, width and style on one scale, supported languages, variable-font axes and named instances, file location, the font's own OpenType tables, character coverage, and each platform's raw values.

## Status

| Platform | State |
| --- | --- |
| macOS | implemented |
| Windows | implemented |
| Linux | implemented |

## Install

```sh
npm install system-font-info
```

Node.js 18 or newer. The package ships a prebuilt binary for each of these targets, and nothing else is supported yet:

| Target | Minimum |
| --- | --- |
| macOS, arm64 and x64 | macOS 11 |
| Windows, x64 and arm64 | Windows 10 version 1809 (build 17763) |
| Linux glibc, x64 and arm64 | glibc 2.28 (Debian 10, Ubuntu 18.10, RHEL 8 or newer) with fontconfig 2.13 and FreeType 2.9 or newer installed |

On Windows the backend uses DirectWrite interfaces that builds before 1809 lack, has no fallback for them, and reports that requirement as its error. On Linux the addon links the system `libfontconfig.so.1` and `libfreetype.so.6` dynamically, so they must be installed (`apt install libfontconfig1` on Debian and Ubuntu, `dnf install fontconfig` on Fedora and RHEL; minimal container images often lack them), and loading without them fails with an error that says so. musl (Alpine) is not supported yet and is reported as such.

The binaries live under `prebuilds/<platform>-<arch>/` (`prebuilds/<platform>-<arch>-glibc/` on Linux). If none matches, or you built from source (see below), the package falls back to `build/Release/`.

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

(`asarUnpack` is the electron-builder option; use the equivalent setting of your packager. electron-builder unpacks modules that contain `.node` files on its own; with Electron Forge, use `@electron-forge/plugin-auto-unpack-natives`.)

The addon is a Node-API (version 8) module without install-time compilation or a `binding.gyp` in the package, so `@electron/rebuild` has nothing to rebuild and is not needed. It ships the binaries of every target; an app that packages for one platform can leave the others out of its bundle, for example with the electron-builder `files` pattern `!**/node_modules/system-font-info/prebuilds/{win32,linux}-*`. On Linux the addon binds to the system fontconfig, the one GTK and Pango use inside Electron, so that library must be installed on the user's machine.

## Building from source

Requires Node.js, a C++20 compiler and Python (for node-gyp). On macOS, the Xcode command line tools. On Linux, pkg-config, fontconfig 2.13 or newer and FreeType 2.9 or newer, with their development packages (`pkg-config libfontconfig-dev libfreetype-dev` on Debian / Ubuntu).

```sh
npm install
npm run build          # node-gyp rebuild, then tsc
npm run copy-prebuild  # optional: build/Release/*.node -> prebuilds/<target>/
npm test
```

`copy-prebuild` names the directory from the target table (`ts/targets.ts`, compiled by `npm run build:ts`); `npm run copy-prebuild -- --arch x64` names the target architecture of a cross build.

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
