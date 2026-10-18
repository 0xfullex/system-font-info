#!/usr/bin/env bash
# Builds the addon and runs the test suite inside a Linux container, the way CI does for the Linux targets, so a
# failure there can be reproduced on a developer machine with the same command.
#
#   scripts/ci/linux-container.sh [--prebuild-out <directory>] [--verify tests|load|none] <image> <node-major> <package>...
#
#   <image>       the container image, e.g. quay.io/pypa/manylinux_2_28_x86_64 (the image decides the glibc the
#                 addon is built against and the architecture it is built for).
#   <node-major>  the Node.js major version to install, e.g. 24: the newest release of that line from nodejs.org.
#   <package>     the packages dnf installs first: the headers to build against and the fonts the tests need.
#
# The script builds the addon, then verifies it as --verify says: `tests` (the default) runs the test suite, `load` only
# loads the package and lists the families (scripts/ci/load-smoke.mjs), `none` does nothing. With --prebuild-out it
# also strips the addon and writes it to <directory>/<target id>/system-font-info.node (the layout of `prebuilds/`);
# a release build passes `--verify none`, because the release workflow tests the assembled prebuilds on their own and
# needs no fonts then.
#
# The repository is mounted read-only and its sources are copied inside the container, so the checkout is never
# written to and a host's node_modules, build output (possibly for another platform) and worktrees never leak in. The
# container runs as root; what it writes to --prebuild-out is chowned to the user who ran the script.
set -euo pipefail

# Inside the container this script is run again with --inside; see the end of the file.
run_inside() {
  local node_major=$1
  shift

  echo "::group::Install packages"
  # EPEL is enabled in the manylinux images but holds none of these packages, and a failing EPEL mirror would fail the install.
  dnf install -y --disablerepo=epel --setopt=install_weak_deps=False "$@"
  echo "::endgroup::"

  echo "::group::Install Node.js ${node_major}"
  install_node "${node_major}"
  echo "::endgroup::"

  echo "::group::Environment"
  echo "image: $(. /etc/os-release && echo "${PRETTY_NAME}"), glibc $(ldd --version | head -n 1 | awk '{ print $NF }'), $(uname -m)"
  echo "node $(node --version), npm $(npm --version), $(g++ --version | head -n 1)"
  rpm -q fontconfig fontconfig-devel freetype freetype-devel
  echo "pkg-config: fontconfig $(pkg-config --modversion fontconfig), freetype2 $(pkg-config --modversion freetype2) (libtool version)"
  echo "fonts: $(fc-list | wc -l) faces in $(fc-list : family | sort -u | wc -l) families"
  fc-list : family | sort -u
  echo "::endgroup::"

  mkdir /work
  tar -C /source --exclude=./.git --exclude=./.worktrees --exclude=./node_modules --exclude=./build --exclude=./dist --exclude=./prebuilds -cf - . |
    tar -C /work -xf -
  cd /work

  npm ci
  npm run build
  if [ -n "${PREBUILD_OUT:-}" ]; then
    npm run copy-prebuild -- --strip
    cp -R prebuilds/. "${PREBUILD_OUT}/"
    # The container runs as root; the files belong to the user who started the script, not to root.
    chown -R "${HOST_UID}:${HOST_GID}" "${PREBUILD_OUT}"
  fi
  case "${VERIFY}" in
    tests) npm test ;;
    load) node scripts/ci/load-smoke.mjs ;;
    none) ;;
  esac
}

# Installs the newest Node.js of a major line from the official tarball, after checking it against SHASUMS256.txt.
install_node() {
  local major=$1 arch
  case "$(uname -m)" in
    x86_64) arch=x64 ;;
    aarch64) arch=arm64 ;;
    *) echo "unsupported architecture $(uname -m)" >&2; return 1 ;;
  esac

  local base="https://nodejs.org/dist/latest-v${major}.x"
  local directory
  directory=$(mktemp -d)
  curl -fsSL --retry 3 -o "${directory}/SHASUMS256.txt" "${base}/SHASUMS256.txt"
  local tarball
  tarball=$(awk -v arch="${arch}" '$2 ~ "^node-v[0-9.]+-linux-" arch "\\.tar\\.xz$" { print $2 }' "${directory}/SHASUMS256.txt")
  if [ "$(echo "${tarball}" | wc -l)" -ne 1 ] || [ -z "${tarball}" ]; then
    echo "expected one linux-${arch} tarball in ${base}/SHASUMS256.txt, found: ${tarball:-none}" >&2
    return 1
  fi
  curl -fsSL --retry 3 -o "${directory}/${tarball}" "${base}/${tarball}"
  (cd "${directory}" && grep -F " ${tarball}" SHASUMS256.txt | sha256sum --check --strict -)
  tar -xJf "${directory}/${tarball}" -C /usr/local --strip-components=1 --no-same-owner
  rm -rf "${directory}"
}

if [ "${1:-}" = "--inside" ]; then
  shift
  run_inside "$@"
  exit
fi

usage="usage: $0 [--prebuild-out <directory>] [--verify tests|load|none] <image> <node-major> <package>..."
# The --prebuild-out directory is mounted into the container, which writes the prebuild there as root and hands it back to this user.
prebuild_out=""
verify=tests
while [ "$#" -gt 0 ]; do
  case "$1" in
    --prebuild-out)
      [ "$#" -ge 2 ] || { echo "--prebuild-out needs a directory" >&2; exit 2; }
      mkdir -p "$2"
      prebuild_out=$(cd "$2" && pwd)
      shift 2
      ;;
    --verify)
      case "${2:-}" in
        tests | load | none) verify=$2 ;;
        *) echo "--verify needs tests, load or none" >&2; exit 2 ;;
      esac
      shift 2
      ;;
    *) break ;;
  esac
done
if [ "$#" -lt 3 ]; then
  echo "${usage}" >&2
  exit 2
fi
image=$1
shift
repository=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
# The container runs the very file the host started, from the read-only mount.
docker run --rm \
  --volume "${repository}:/source:ro" \
  --env "VERIFY=${verify}" \
  ${prebuild_out:+--volume "${prebuild_out}:/prebuild-out" --env PREBUILD_OUT=/prebuild-out --env "HOST_UID=$(id -u)" --env "HOST_GID=$(id -g)"} \
  "${image}" \
  bash /source/scripts/ci/linux-container.sh --inside "$@"
