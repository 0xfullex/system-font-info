# scripts/ci

Scripts the GitHub Actions workflows (`../../.github/workflows/`) run. Each is also runnable by hand, so a CI failure can be reproduced.

| File | Role |
| --- | --- |
| `linux-container.sh` | Builds the addon in a manylinux container and verifies it as `--verify tests\|load\|none` says (the test suite by default). With `--prebuild-out <directory>` it also writes the stripped addon to `<directory>/<target id>/`, owned by the invoking user; a release build is that plus `--verify none`. Needs Docker. |
| `load-smoke.mjs` | Loads the built package and lists the families: what `--verify load` and the action's `verify: load` run for a target whose `tests` flag in the target table is off. |
| `check-install.mjs` | Run against a project the packed tarball was installed into (with npm or pnpm): the installed package holds no `build/`, `binding.gyp` or `src/`, the addon of the machine loads from it, and `getFamilies` lists fonts. What a unit test cannot see: the package as an installer leaves it. |
| `electron-smoke.mjs` | Run by Electron, against a project with the tarball installed: loads the package in the main process once the app is ready, calls `getFamilies` with `fields: []` and with the `names` preset, and exits 1 on any failure. On Linux it also reads `/proc/self/maps` and fails unless exactly one `libfontconfig.so.1` is mapped and it is the system's. What a unit test cannot see: the addon in Electron's own Node, and which fontconfig an Electron process holds (Chromium bundles one but exports no symbols from it, so the addon binds the one GTK and Pango use). Run it by hand with `npx electron scripts/ci/electron-smoke.mjs <project> --no-sandbox` (`xvfb-run` on a headless Linux). |

## Invariants

- **The workflows share their steps through local actions, never by copying.** `../../.github/actions/target-matrix` turns the target table (`../../ts/targets.ts`) into a job matrix, so no workflow lists the targets; `../../.github/actions/build-addon` builds one target natively (this script on Linux, the runner's toolchain elsewhere) and verifies it (`tests`, `load` or `none`) on every platform alike, so the target table's `tests` flag has one reader. `ci.yml` and `release.yml` both call them.
- **Linux builds run through `docker run`, never a job-level `container:`**, because the runner injects its own Node.js into such a container. The script installs Node.js itself, checked against the `SHASUMS256.txt` that nodejs.org publishes beside the tarball. Both come from the same origin, so the check catches a corrupted or truncated download but not a compromised origin.
- **The container mounts the repository read-only** and builds from a copy, so a checkout's `node_modules`, build output (possibly of another platform) and `.worktrees` never leak in. Files written to `--prebuild-out` are chowned to the invoking host user (the container runs as root).
