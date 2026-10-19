# system-font-info

[![npm version](https://img.shields.io/npm/v/system-font-info.svg)](https://www.npmjs.com/package/system-font-info)
[![Node.js](https://img.shields.io/node/v/system-font-info.svg)](https://nodejs.org)
[![CI](https://github.com/0xfullex/system-font-info/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/0xfullex/system-font-info/actions/workflows/ci.yml)
[![License](https://img.shields.io/npm/l/system-font-info.svg)](LICENSE)

[![macOS](https://img.shields.io/badge/macOS-arm64%20%7C%20x64-555555.svg)](#supported-platforms)
[![Windows](https://img.shields.io/badge/Windows-x64%20%7C%20arm64-0078D4.svg)](#supported-platforms)
[![Linux](https://img.shields.io/badge/Linux%20glibc-x64%20%7C%20arm64-E95420.svg)](#supported-platforms)
[![Electron](https://img.shields.io/badge/Electron-main%20process-47848F.svg)](#electron)

List the font families installed on a system, with full metadata, through the operating system's own font service: CoreText on macOS, DirectWrite on Windows, fontconfig on Linux. It is a Node-API addon with prebuilt binaries; nothing is compiled when you install it.

Every platform returns one shape, and the caller declares which fields it wants: localized family, full and style names in every language the font carries, weight, width and style on one scale, supported languages, variable-font axes and named instances, file location, the font's own OpenType tables, character coverage, and each platform's raw values.

## Supported platforms

Node.js 22 or newer, in plain Node.js and in the main process of Electron (see [Electron](#electron)). The package ships a prebuilt binary for each of these targets, and nothing else has one:

| Target | Minimum |
| --- | --- |
| macOS, arm64 and x64 | macOS 11 |
| Windows, x64 and arm64 | Windows 10 version 1809 (build 17763) |
| Linux glibc, x64 and arm64 | glibc 2.28 (Debian 10, Ubuntu 18.10, RHEL 8 or newer) with fontconfig 2.13 and FreeType 2.9 or newer installed |

On Windows the backend uses DirectWrite interfaces that builds before 1809 lack, has no fallback for them, and reports that requirement as its error. On Linux the addon links the system `libfontconfig.so.1` and `libfreetype.so.6` dynamically, so they must be installed (`apt install libfontconfig1` on Debian and Ubuntu, `dnf install fontconfig` on Fedora and RHEL; minimal container images often lack them), and loading without them fails with an error that says so. musl (Alpine) is not supported and is reported as such. Another platform can build the addon from source, see [CONTRIBUTING.md](CONTRIBUTING.md#building-from-source). What each platform lists and how it differs from the others is in [docs/platforms.md](docs/platforms.md).

## Install

```sh
npm install system-font-info
```

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

The addon is a Node-API (version 8) module without install-time compilation or a `binding.gyp` in the package, so `@electron/rebuild` has nothing to rebuild and is not needed. It ships the binaries of every target; an app that packages for one platform can leave the others out of its bundle, for example with the electron-builder `files` pattern `!**/node_modules/system-font-info/prebuilds/{win32,linux}-*`. On Linux the addon binds to the system fontconfig, the one GTK and Pango use inside Electron, so that library must be installed on the user's machine. Every release is loaded and run in the main process of Electron 44 on each target before it is published ([`scripts/ci/electron-smoke.mjs`](scripts/ci/README.md)); on Linux that run also checks that the process maps exactly one `libfontconfig`, the system's.

## Contributing

Building from source (also the way to use the package on a platform without a prebuilt binary), development, continuous integration and releasing are described in [CONTRIBUTING.md](CONTRIBUTING.md).

## License

[MIT](LICENSE)
