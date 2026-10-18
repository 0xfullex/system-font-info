# scripts/ci

Scripts the GitHub Actions workflows (`../../.github/workflows/`) run. Each is also runnable by hand, so a CI failure can be reproduced.

| File | Role |
| --- | --- |
| `linux-container.sh` | Builds the addon in a manylinux container and, by default, runs the tests; with `--prebuild-out <directory>` it builds a release instead (stripped addon written to `<directory>/<target id>/`, no tests). Needs Docker. |

## Invariants

- **The workflows share their steps through local actions, never by copying.** `../../.github/actions/target-matrix` turns the target table (`../../ts/targets.ts`) into a job matrix, so no workflow lists the targets; `../../.github/actions/build-addon` builds one target natively (this script on Linux, the runner's toolchain elsewhere). `ci.yml` and `release.yml` both call them.
- **Linux builds run through `docker run`, never a job-level `container:`**, because the runner injects its own Node.js into such a container. The script installs Node.js itself, checked against `SHASUMS256.txt`.
- **The container mounts the repository read-only** and builds from a copy, so a checkout's `node_modules` and build output (possibly of another platform) never leak in.
