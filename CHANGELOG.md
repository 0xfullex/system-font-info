# Changelog

All notable changes to `system-font-info` are listed here, newest first. Versions before 1.0.0 may change the API in a minor release.

## Unreleased

- macOS: a face's `supportedLanguages`, and the family list built from them, could come back empty for a font that has languages when two calls ran at the same time.
- The README leads with what the package returns compared with a typical font lister, and has a Simplified Chinese translation (`README.zh-CN.md`, also in the package).

## 0.9.0 - 2026-10-18

First release.

- `getFamilies` and `getFamily` list the installed font families through CoreText on macOS, DirectWrite on Windows and fontconfig on Linux, and return one result shape on every platform.
- The caller chooses the fields it pays for, with `fields` or a `preset`: localized family, full and style names in every language the font carries, weight, width and style on one scale, supported languages, variable-font axes and named instances, file location, the font's own OpenType tables, character coverage per language, and each platform's raw values.
- `pickName` chooses a localized name for an ordered list of locales.
- Prebuilt Node-API binaries for macOS (arm64, x64), Windows (x64, arm64) and Linux glibc (x64, arm64), so nothing is compiled at install time. Node.js 22 or newer; works in the Electron main process.
- Type declarations compute the result type from the requested fields.
