// Refuses to pack the package unless its contents are the release's: `npm pack` and `npm publish` run it as `prepack`.
//
//   node scripts/check-package.mjs
//
// The package is the one every user installs, so an incomplete one (a machine that built only its own prebuilt addon) or
// one that carries the sources must never be produced by accident. It checks, and reports every failure at once:
//   - the compiled JavaScript exists (`main` and `types` of package.json) and is newer than every source in ts/;
//   - every target of the table (ts/targets.ts) has its `prebuilds/<id>/system-font-info.node`;
//   - nothing else is packed: no sources, no `binding.gyp`, no tests, no fontconfig orthography sources.
//
// There is no switch to skip it. To look at what a package would hold, run `npm pack --dry-run --ignore-scripts`.
//
// The packed files are asked of npm itself (`npm pack --dry-run`), so the check sees the result of `files`, `.npmignore`
// and npm's own always-included files, not a guess of them.
import { execFileSync } from 'node:child_process'
import { existsSync, readdirSync, readFileSync, statSync } from 'node:fs'
import path from 'node:path'
import { fileURLToPath, pathToFileURL } from 'node:url'

export const ADDON_FILE = 'system-font-info.node'

/** The files at the package root that must ship. */
const ROOT_FILES = ['package.json', 'README.md', 'LICENSE', 'CHANGELOG.md']

/** The license notice of the fontconfig data the addon is generated from, and the note that explains it. */
const THIRD_PARTY_FILES = ['third_party/fontconfig/COPYING', 'third_party/fontconfig/README.md']

/** Everything a package may hold besides the prebuilt addons: these directories (any file below them) and these files. */
const ALLOWED_DIRECTORIES = ['dist/', 'docs/']
const ALLOWED_FILES = [...ROOT_FILES, ...THIRD_PARTY_FILES]

/** The modification time of the newest file below `directory`. */
function newestModification(directory) {
  let newest = 0
  for (const entry of readdirSync(directory, { recursive: true, withFileTypes: true })) {
    if (entry.isFile()) newest = Math.max(newest, statSync(path.join(entry.parentPath, entry.name)).mtimeMs)
  }
  return newest
}

/**
 * The reasons this tree must not be packed, empty when it may.
 *
 * @param {object} input
 * @param {string} input.root the package root: package.json, ts/ and dist/ are read from it
 * @param {readonly string[]} input.targetIds the ids of the target table
 * @param {readonly string[]} input.packedFiles the files the package would hold, relative to `root`, with `/` separators
 * @returns {string[]}
 */
export function checkPackage({ root, targetIds, packedFiles }) {
  const problems = []
  const packed = new Set(packedFiles)

  const manifest = JSON.parse(readFileSync(path.join(root, 'package.json'), 'utf8'))
  const compiled = [manifest.main, manifest.types]
  const missingCompiled = compiled.filter((file) => !existsSync(path.join(root, file)))
  for (const file of missingCompiled) problems.push(`${file} does not exist; run \`npm run build:ts\``)
  if (missingCompiled.length === 0) {
    const sources = newestModification(path.join(root, 'ts'))
    if (compiled.some((file) => statSync(path.join(root, file)).mtimeMs < sources)) {
      problems.push('dist/ is older than a file in ts/; run `npm run build:ts`')
    }
  }

  for (const id of targetIds) {
    if (!packed.has(`prebuilds/${id}/${ADDON_FILE}`)) {
      problems.push(`prebuilds/${id}/${ADDON_FILE} is not in the package; build it on that target and run \`npm run copy-prebuild\``)
    }
  }
  for (const file of [...ROOT_FILES, THIRD_PARTY_FILES[0]]) {
    if (!packed.has(file)) problems.push(`${file} is not in the package`)
  }

  const prebuilds = new Set(targetIds.map((id) => `prebuilds/${id}/${ADDON_FILE}`))
  for (const file of [...packed].sort()) {
    const allowed =
      prebuilds.has(file) || ALLOWED_FILES.includes(file) || ALLOWED_DIRECTORIES.some((directory) => file.startsWith(directory))
    if (!allowed) problems.push(`${file} must not be in the package`)
  }
  return problems
}

/** The files `npm pack` would put in the package at `root`. Scripts are not run: the nested pack would run this check again. */
function listPackedFiles(root) {
  const output = execFileSync('npm', ['pack', '--dry-run', '--json', '--ignore-scripts'], {
    cwd: root,
    encoding: 'utf8',
    shell: process.platform === 'win32',
    stdio: ['ignore', 'pipe', 'inherit']
  })
  return JSON.parse(output)[0].files.map((file) => file.path)
}

async function main() {
  const root = path.join(path.dirname(fileURLToPath(import.meta.url)), '..')
  const targetsFile = path.join(root, 'dist', 'targets.js')
  if (!existsSync(targetsFile)) {
    console.error('check-package: dist/ does not exist; run `npm run build:ts`')
    process.exit(1)
  }
  const { TARGETS } = await import(pathToFileURL(targetsFile).href)
  const problems = checkPackage({ root, targetIds: TARGETS.map((target) => target.id), packedFiles: listPackedFiles(root) })
  if (problems.length > 0) {
    console.error(`check-package: refusing to pack:\n${problems.map((problem) => `  - ${problem}`).join('\n')}`)
    process.exit(1)
  }
  console.log(`check-package: ${TARGETS.length} prebuilds, dist/ up to date, nothing else in the package`)
}

if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) await main()
