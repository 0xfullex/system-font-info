# Contributing

This file is for people who build, change or release `system-font-info`. Using the package is covered by the [README](README.md), [docs/api.md](docs/api.md) and [docs/platforms.md](docs/platforms.md). Each source directory has a `README.md` with its conventions and invariants (start at [src/README.md](src/README.md), [ts/README.md](ts/README.md) and [scripts/ci/README.md](scripts/ci/README.md)); read the one of a directory, and those of its parents, before changing code in it.

- [Building from source](#building-from-source)
- [Development](#development)
- [Continuous integration](#continuous-integration)
- [Releasing](#releasing)

## Building from source

Building from source is also how to use the package on a platform that has no prebuilt binary, such as Linux with musl (Alpine): the package loads the build before any prebuilt binary.

Requires Node.js, a C++20 compiler and Python (for node-gyp). On macOS, the Xcode command line tools. On Linux, pkg-config, fontconfig 2.13 or newer and FreeType 2.9 or newer, with their development packages (`pkg-config libfontconfig-dev libfreetype-dev` on Debian / Ubuntu).

```sh
npm install
npm run build          # node-gyp rebuild, then tsc
npm run copy-prebuild  # optional: build/Release/*.node -> prebuilds/<target>/
npm test
```

The package loads `build/Release/system-font-info.node` before any `prebuilds/` binary, so `npm run build` is enough to use your own build; the installed package has no `build/`, so it loads the prebuilt binary of its platform. `copy-prebuild` is for assembling the directories a package ships. `copy-prebuild` names the directory from the target table (`ts/targets.ts`, compiled by `npm run build:ts`); `npm run copy-prebuild -- --arch x64` names the target architecture of a cross build. `npm run copy-prebuild -- --strip` strips the copy, as a release build does.

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

## Continuous integration

`.github/workflows/ci.yml` runs on every push to every branch (and on demand). One job runs the platform-independent checks: typecheck, lint, format check and the generators' `--check`. The test matrix is made from the target table (`node scripts/targets.mjs --matrix`): each target builds the addon and runs `npm test` on its own runner (or only loads it, when the target's `tests` flag is off; `.github/actions/build-addon` makes that decision for every platform alike), never cross-compiled. Linux targets build and test inside a `manylinux_2_28` container; `scripts/ci/linux-container.sh` is the one script that does it, so a Linux failure can be reproduced anywhere Docker runs:

```sh
scripts/ci/linux-container.sh quay.io/pypa/manylinux_2_28_x86_64 24 \
  fontconfig-devel freetype-devel fontconfig dejavu-sans-fonts google-noto-sans-cjk-ttc-fonts
```

The arguments are the image, the Node.js major version and the packages to install: the headers to build against and the fonts the tests need, since a container has none. The workflow's `LINUX_PACKAGES` is the list CI uses. `--verify tests|load|none` before the image says what runs after the build: the test suite (the default), only a load of the package and a listing of the families (for a target whose `tests` flag in the target table is off), or nothing. `--prebuild-out <directory>` also writes the stripped addon to `<directory>/<target id>/`, owned by the invoking user; a release build is `--prebuild-out <directory> --verify none`.

The steps the CI and release workflows share live in two local actions, `.github/actions/target-matrix` (the matrix from the target table) and `.github/actions/build-addon` (build one target natively); `scripts/ci/README.md` lists the scripts.

## Releasing

`.github/workflows/release.yml` builds, verifies and publishes the package. It runs when a `v*` tag is pushed, and on demand as a dry run: everything except the publish. The jobs, in order:

1. `verify`: the tag is `v` plus the version in `package.json`, the tagged commit is on `main`, and its `CHANGELOG.md` has a non-empty section for the version (checked before anything is published, since a published version's tagged commit can no longer be given its notes).
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

The first release, 0.9.0, was published by hand, because a trusted publisher can only be configured on a package that exists; its first OIDC publish is the release after 0.9.0. Do not configure the trusted publisher until that release is about to be tagged. Creating the package also made npm publish a public `0.0.0-stage` placeholder version, although 0.9.0 was published directly with `npm publish` and not staged; `latest` moved to 0.9.0 once npm had processed it, about a minute later.

A release published by hand, as 0.9.0 was (or whenever the OIDC publish is unavailable), goes:

1. Run the dry run on the commit to release and download the `package` artifact of that green run: `gh run download <run id> --name package`.
2. Publish that tarball from a machine logged in to npm, with 2FA (a passkey), in an interactive terminal (npm waits there for the browser confirmation; without a terminal it fails with `EOTP`): `npm publish ./system-font-info-<version>.tgz`. A version published this way has no provenance. npm answers that the package is being processed; check that `npm view system-font-info@<version> dist.integrity` matches the tarball before going on.
3. Only after the publish, push the `v<version>` tag. The `publish` job then finds the version already on npm and does nothing. The `github-release` job creates the Release with the tarball that was published.
