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

Node.js 22 or newer. The package ships a prebuilt binary for each of these targets, and nothing else is supported yet:

| Target | Minimum |
| --- | --- |
| macOS, arm64 and x64 | macOS 11 |
| Windows, x64 and arm64 | Windows 10 version 1809 (build 17763) |
| Linux glibc, x64 and arm64 | glibc 2.28 (Debian 10, Ubuntu 18.10, RHEL 8 or newer) with fontconfig 2.13 and FreeType 2.9 or newer installed |

On Windows the backend uses DirectWrite interfaces that builds before 1809 lack, has no fallback for them, and reports that requirement as its error. On Linux the addon links the system `libfontconfig.so.1` and `libfreetype.so.6` dynamically, so they must be installed (`apt install libfontconfig1` on Debian and Ubuntu, `dnf install fontconfig` on Fedora and RHEL; minimal container images often lack them), and loading without them fails with an error that says so. musl (Alpine) is not supported yet and is reported as such.

The binaries live under `prebuilds/<platform>-<arch>/` (`prebuilds/<platform>-<arch>-glibc/` on Linux). A build from source (see below) in `build/Release/` takes precedence over them, so a fresh local build is never shadowed by an older copy in `prebuilds/`; the installed package has no `build/`, so it loads the prebuilt binary of its platform.

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

The addon is a Node-API (version 8) module without install-time compilation or a `binding.gyp` in the package, so `@electron/rebuild` has nothing to rebuild and is not needed. It ships the binaries of every target; an app that packages for one platform can leave the others out of its bundle, for example with the electron-builder `files` pattern `!**/node_modules/system-font-info/prebuilds/{win32,linux}-*`. On Linux the addon binds to the system fontconfig, the one GTK and Pango use inside Electron, so that library must be installed on the user's machine. Every release is loaded and run in the main process of Electron 44 on each target before it is published (`scripts/ci/electron-smoke.mjs`); on Linux that run also checks that the process maps exactly one `libfontconfig`, the system's.

## Building from source

Requires Node.js, a C++20 compiler and Python (for node-gyp). On macOS, the Xcode command line tools. On Linux, pkg-config, fontconfig 2.13 or newer and FreeType 2.9 or newer, with their development packages (`pkg-config libfontconfig-dev libfreetype-dev` on Debian / Ubuntu).

```sh
npm install
npm run build          # node-gyp rebuild, then tsc
npm run copy-prebuild  # optional: build/Release/*.node -> prebuilds/<target>/
npm test
```

The package loads `build/Release/system-font-info.node` before any `prebuilds/` binary, so `npm run build` is enough to use your own build; `copy-prebuild` is for assembling the directories a package ships. `copy-prebuild` names the directory from the target table (`ts/targets.ts`, compiled by `npm run build:ts`); `npm run copy-prebuild -- --arch x64` names the target architecture of a cross build. `npm run copy-prebuild -- --strip` strips the copy, as a release build does.

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

### Continuous integration

`.github/workflows/ci.yml` runs on every push to every branch (and on demand). One job runs the platform-independent checks: typecheck, lint, format check and the generators' `--check`. The test matrix is made from the target table (`node scripts/targets.mjs --matrix`): each target builds the addon and runs `npm test` on its own runner (or only loads it, when the target's `tests` flag is off; `.github/actions/build-addon` makes that decision for every platform alike), never cross-compiled. Linux targets build and test inside a `manylinux_2_28` container; `scripts/ci/linux-container.sh` is the one script that does it, so a Linux failure can be reproduced anywhere Docker runs:

```sh
scripts/ci/linux-container.sh quay.io/pypa/manylinux_2_28_x86_64 24 \
  fontconfig-devel freetype-devel fontconfig dejavu-sans-fonts google-noto-sans-cjk-ttc-fonts
```

The arguments are the image, the Node.js major version and the packages to install: the headers to build against and the fonts the tests need, since a container has none. The workflow's `LINUX_PACKAGES` is the list CI uses. `--verify tests|load|none` before the image says what runs after the build: the test suite (the default), only a load of the package and a listing of the families (for a target whose `tests` flag in the target table is off), or nothing. `--prebuild-out <directory>` also writes the stripped addon to `<directory>/<target id>/`, owned by the invoking user; a release build is `--prebuild-out <directory> --verify none`.

The steps the CI and release workflows share live in two local actions, `.github/actions/target-matrix` (the matrix from the target table) and `.github/actions/build-addon` (build one target natively); `scripts/ci/README.md` lists the scripts.

### Releasing

`.github/workflows/release.yml` builds, verifies and publishes the package. It runs when a `v*` tag is pushed, and on demand as a dry run: everything except the publish. The jobs, in order:

1. `verify`: the tag is `v` plus the version in `package.json`, and the tagged commit is on `main`.
2. `build`: every target of the table is built natively (Linux in the manylinux container), stripped, and uploaded as an artifact.
3. `test-prebuilds`: the prebuilds are assembled with no `build/` anywhere and `npm test` runs on every target's runner. A Linux addon built in the container is therefore tested on the runner's newer glibc, fontconfig and FreeType.
4. `pack`: `npm pack`. `prepack` (`scripts/check-package.mjs`) refuses unless every target has its prebuild, `dist/` is newer than `ts/`, nothing but the expected files is packed (no sources, no `binding.gyp`, no tests), and `package.json` has no `install`, `preinstall` or `postinstall` script (the package never builds at install time). A refusal also prints how to list the contents. There is no switch to skip it; `npm pack --dry-run --ignore-scripts` shows a package's contents without the check. The tarball is the `package` artifact.
5. `install-check` and `electron-smoke`, on every target: the tarball is installed into a clean project with npm and with pnpm and loaded, and the installed package is loaded in the main process of Electron `ELECTRON_VERSION` (`scripts/ci/README.md` says what each proves).
6. `publish`, in the `npm-release` environment: `npm publish --provenance` by [trusted publishing](https://docs.npmjs.com/trusted-publishers), so no npm token exists. A version with a pre-release part goes to the `next` tag. A version that is already published is skipped, so a re-run is safe.
7. `github-release`, for a tag push only (a dry run creates no Release): the GitHub Release for the tag, titled with the tag. Its notes are the version's section of `CHANGELOG.md` (`scripts/ci/release-notes.mjs`, which fails when the section is missing or empty), it is marked a prerelease when the version has a pre-release part (the same `verify` output that picks the `next` npm tag), and its one asset is the tarball npm serves for that version, downloaded from the registry and checked against `npm view system-font-info@<version> dist.integrity` (retried for a short while after a publish). It runs after `publish` succeeded, including when `publish` skipped a version that was already on npm, so the hand-published first version gets its Release too. A tag that already has a Release is left as it is, so a re-run is safe. Only this job has `contents: write`.

To cut a release:

1. On a branch, run `npm version x.y.z --no-git-tag-version`, rename the `Unreleased` section of `CHANGELOG.md` to the version and date (`## x.y.z - YYYY-MM-DD`; the Release notes are that section), raise `ELECTRON_VERSION` in `release.yml` to the latest stable Electron of the major it names, and commit as `chore(release): x.y.z`.
2. Check the pipeline on the branch with `gh workflow run release.yml --ref <branch>` (a dry run by default), then land the branch on `main`.
3. Tag the landed commit, `git tag -a vx.y.z -m vx.y.z`, and push the tag. When the jobs before `publish` are green, `publish` waits for a required reviewer of the `npm-release` environment to approve it.

One-time setup on GitHub and npm:

- On GitHub, an `npm-release` environment with required reviewers **and** "Deployment branches and tags" restricted to the tag pattern `v*`. A trusted publisher binds the repository, the workflow file and the environment, not the git ref, so the environment is what keeps a run from any other branch or tag from publishing, and the reviewers add a human approval to every publish.
- On npm, a trusted publisher on the package (repository `0xfullex/system-font-info`, workflow `release.yml`, environment `npm-release`), after which the package can refuse token publishes. Its allowed actions must include `npm publish`, which the `publish` job runs over OIDC: a trusted publisher created after 2026-09-03 allows only `npm stage publish` by default. In the npm web UI tick "npm publish" under Allowed actions; with the CLI (npm 11 or newer) it is `npm trust github system-font-info --file release.yml --repo 0xfullex/system-font-info --env npm-release --allow-publish` (see [Trusted publishing](https://docs.npmjs.com/trusted-publishers) and [`npm trust`](https://docs.npmjs.com/cli/v12/commands/npm-trust)). A newly configured trusted publisher must complete its first successful publish through OIDC within 2 days or it expires and can no longer be used or edited (delete it and create it again); that first publish binds it to the repository for good. Configure it within the 2 days before the tag push that is meant to be its first publish, not earlier.

The first release, 0.1.0, is published by hand, because a trusted publisher is configured on a package that exists and its first OIDC publish is then the release after it (0.1.1 or 0.2.0). Do not configure the trusted publisher until that release is about to be tagged. It is published directly with `npm publish`, never staged: staging a package that does not exist yet makes npm publish a public `0.0.0-stage` placeholder version, which a direct publish does not. To publish 0.1.0:

1. Run the dry run on the commit to release and download the `package` artifact of that green run: `gh run download <run id> --name package`.
2. Publish that tarball from a machine logged in to npm, with 2FA (a passkey): `npm publish ./system-font-info-0.1.0.tgz`. That version has no provenance.
3. Only after the publish, push the `v0.1.0` tag. The `publish` job then finds 0.1.0 already on npm and does nothing, instead of trying to publish through OIDC, which is not configured yet. The `github-release` job creates the `v0.1.0` Release with the tarball that was published.
