# system-font-info

English | [简体中文](README.zh-CN.md)

[![npm version](https://img.shields.io/npm/v/system-font-info.svg)](https://www.npmjs.com/package/system-font-info)
[![Node.js](https://img.shields.io/node/v/system-font-info.svg)](https://nodejs.org)
[![CI](https://github.com/0xfullex/system-font-info/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/0xfullex/system-font-info/actions/workflows/ci.yml)
[![License](https://img.shields.io/npm/l/system-font-info.svg)](LICENSE)

[![macOS](https://img.shields.io/badge/macOS-arm64%20%7C%20x64-555555.svg)](#supported-platforms)
[![Windows](https://img.shields.io/badge/Windows-x64%20%7C%20arm64-0078D4.svg)](#supported-platforms)
[![Linux](https://img.shields.io/badge/Linux%20glibc-x64%20%7C%20arm64-E95420.svg)](#supported-platforms)
[![Electron](https://img.shields.io/badge/Electron-main%20process-47848F.svg)](#electron)

**Font pickers that know as much about a font as the operating system does.**

system-font-info lists the fonts installed on macOS, Windows and Linux through each system's own font service (CoreText, DirectWrite, fontconfig) and returns what that service knows about every face, in one shape on every platform. Most font listers for Node.js return one name and a coarse weight and style per font, enough for a plain menu in one language. This gives an Electron app, or any desktop tool, what a real font menu needs:

| | A typical font lister | system-font-info |
| --- | --- | --- |
| Family and style names | One name, usually English | Every language the font carries: `苹方-简`, `蘋方-簡`, `PingFang SC`; `常规体`, `標準體`, `Regular` |
| Weight, width, style | Coarse labels | CSS numbers on one scale on every platform (`font-weight` 1-1000, `font-width` 50-200%), plus each platform's raw value |
| Languages | Not reported | The languages each face supports, and character coverage for the languages you ask about |
| Variable fonts | Not reported | Axes with their ranges and localized names; named instances as faces |
| Files and tables | A path at most | File path and collection index, the font's own OpenType tables |
| Cost and types | Everything, on every call | Only the fields you ask for, with the result type computed from them |
| Install | Often compiled at install, or shells out to `fc-list` or PowerShell | Prebuilt for six targets; nothing is compiled, nothing is spawned |

### What it looks like

A font menu in Chinese, with only the families that can set Simplified Chinese:

```js
import { getFamilies, getFamily, pickName } from 'system-font-info'

const families = await getFamilies({ fields: ['familyNames', 'supportedLanguages'] })
const menu = families
  .filter((family) => family.supportedLanguages.includes('zh-Hans'))
  .map((family) => ({ family: family.family, label: pickName(family.familyNames, ['zh-CN', 'en']) }))
// [..., { family: 'Hiragino Sans GB', label: '冬青黑体简体中文' }, ..., { family: 'PingFang SC', label: '苹方-简' }, ...]
```

A weight slider for a variable font, with its named instances as stops:

```js
const syriac = await getFamily('Noto Sans Syriac', { fields: ['faces.axes', 'faces.styleName', 'faces.weight'] })
// syriac.faces[0].axes:  [{ tag: 'wght', min: 100, max: 900, default: 400, names: [...] }]
// syriac.faces:          Regular 400, Thin 100, ExtraLight 200, Light 300, Medium 500, ... Black 900
```

(Output from macOS 27; each platform lists what its font service holds.)

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
