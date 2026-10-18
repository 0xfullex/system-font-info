# scripts/ci

Scripts the GitHub Actions workflows (`../../.github/workflows/`) run. Each is also runnable by hand, so a CI failure can be reproduced.

| File | Role |
| --- | --- |
| `linux-container.sh` | Builds the addon in a manylinux container and, by default, runs the tests; with `--prebuild-out <directory>` it builds a release instead (stripped addon written to `<directory>/<target id>/`, no tests). Needs Docker. |
| `check-install.mjs` | Run against a project the packed tarball was installed into (with npm or pnpm): the installed package holds no `build/`, `binding.gyp` or `src/`, the addon of the machine loads from it, and `getFamilies` lists fonts. What a unit test cannot see: the package as an installer leaves it. |
| `electron-smoke.mjs` | Run by Electron, against a project with the tarball installed: loads the package in the main process once the app is ready, calls `getFamilies` with `fields: []` and with the `names` preset, and exits 1 on any failure. On Linux it also reads `/proc/self/maps` and fails unless exactly one `libfontconfig.so.1` is mapped and it is the system's. What a unit test cannot see: the addon in Electron's own Node, and which fontconfig an Electron process holds (Chromium bundles one but exports no symbols from it, so the addon binds the one GTK and Pango use). Run it by hand with `npx electron scripts/ci/electron-smoke.mjs <project> --no-sandbox` (`xvfb-run` on a headless Linux). |

## Invariants

- **The workflows share their steps through local actions, never by copying.** `../../.github/actions/target-matrix` turns the target table (`../../ts/targets.ts`) into a job matrix, so no workflow lists the targets; `../../.github/actions/build-addon` builds one target natively (this script on Linux, the runner's toolchain elsewhere). `ci.yml` and `release.yml` both call them.
- **Linux builds run through `docker run`, never a job-level `container:`**, because the runner injects its own Node.js into such a container. The script installs Node.js itself, checked against `SHASUMS256.txt`.
- **The container mounts the repository read-only** and builds from a copy, so a checkout's `node_modules` and build output (possibly of another platform) never leak in.
