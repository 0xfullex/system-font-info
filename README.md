# system-font-info

List the fonts installed on a system, with full metadata, through the operating system's own font service: CoreText on macOS, DirectWrite on Windows, fontconfig on Linux. It is a Node-API addon with prebuilt binaries; nothing is compiled when you install it.

Every platform returns one shape: localized family, full and style names in every language the font carries, weight, width and style on one scale, supported languages, variable-font axes and named instances, file location, the font's own OpenType tables, and optionally the code point coverage.

## Status

| Platform | State |
| --- | --- |
| macOS | implemented |
| Windows | in progress |
| Linux | in progress |

## Install

```sh
npm install system-font-info
```

Node.js 18 or newer. The package ships a binary per platform and architecture under `prebuilds/<platform>-<arch>/`. If no binary matches (or you built from source, see below), it falls back to `build/Release/`.

## Usage

```js
const { getFonts, pickName, watch } = require('system-font-info')

const families = await getFonts()
for (const family of families) {
  const face = family.faces[0]
  console.log(family.family, face.postscriptName, pickName(face.fullNames, ['zh-CN', 'en']))
}

const stop = watch(() => {
  // The installed fonts may have changed; call getFonts() again.
})
stop()
```

### `getFonts(options?)`

Returns `Promise<FontFamily[]>`, sorted by family name. The work runs on a worker thread, not on the JS thread.

| Option | Default | Meaning |
| --- | --- | --- |
| `tables` | `true` | Read the standard OpenType tables of every face into `face.opentype`. |
| `coverage` | `false` | Include `face.coverage`, the inclusive code point ranges of the face. The ranges are large. |

### `pickName(names, locales)`

Picks the entry of a `LocalizedString[]` (for example `face.fullNames`) that best fits an ordered list of locales, and returns its `value`, or `undefined` when none fits. Locales are tried in order; a locale is tried at four match levels before the next locale is considered: the same canonical tag, the same tag after adding likely subtags (so `zh-CN` matches `zh-Hans-CN`), the same language and script, the same language. Entries whose language is `""` never match.

The default strings `fullName`, `styleName` and `axes[].name` are already chosen for you: `en-US`, else any English, else the entry without a language, else the first.

### `watch(onChange)`

Calls `onChange()` whenever the set of installed fonts may have changed, and returns a function that stops the subscription (calling it again does nothing). The callback carries no detail and may fire when nothing observable changed; call `getFonts()` again to see the new state. Notifications that arrive while one is still waiting to be delivered are merged. Watching never keeps the process alive. An error thrown by one subscriber does not stop the others; it is rethrown from a microtask and surfaces as an uncaught exception.

## The result

`FontFamily` groups `FontFace` objects under the name the platform matcher uses (what goes into CSS `font-family`). The family-level `familyNames`, `languages`, `variable`, `enabled` and `hidden` are derived from its faces. The complete, documented shape is in [`ts/types.ts`](ts/types.ts); the public types are exported from the package.

Highlights of a `FontFace`:

- Identity: `postscriptName`, `fullName` / `fullNames`, `styleName` / `styleNames`, `familyNames`. Language tags are canonical BCP 47 (`Intl.getCanonicalLocales`); localized arrays hold no exact duplicates.
- Normalized traits: `weight` (CSS 1-1000), `width` (CSS percentage), `style`, `monospace`, `color`, `symbol`.
- State: `enabled`, `hidden`, `simulations`, `scope`, `format`, `file`.
- Variations: `variable`, `axes`, `coordinates`, `namedInstance` (an index into `opentype.fvar.instances`). Faces of one font share their `axes` and `opentype` objects.
- `opentype`: `head`, `hhea`, `vhea`, `os2`, `post`, `maxp`, `cmap`, `fvar`, `meta` and the GSUB/GPOS feature lists, as stored in the font, plus every `name` record unmerged.
- `platform`: raw platform values and platform-only information, tagged by `os`.

Nothing is filtered: every language and every name a font carries is kept; only exact duplicates are merged.

## Differences between platforms

- `languages`: on macOS, the languages the font declares (`kCTFontLanguagesAttribute`); on Windows, DirectWrite's supported script and language tags; on Linux, fontconfig's `FC_LANG`, which fontconfig computes from the font's character coverage.
- Availability on macOS:
  - Private system fonts whose names start with "." are not listed by the OS and are not returned.
  - Fonts the user disabled in Font Book are returned with `enabled: false`.
  - Fonts that can be downloaded but have not been downloaded are not returned.
- `simulations` is `false` everywhere except Windows, where DirectWrite synthesizes bold and oblique faces.
- `platform` is a different object per platform; see `DarwinNative`, `Win32Native` and `LinuxNative` in the types.

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

## License

MIT
