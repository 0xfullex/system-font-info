// Checks or rewrites the formatting of the C, C++ and Objective-C++ sources with
// the clang-format found on PATH.
//
//   node scripts/clang-format.mjs --check   fail when a file is not formatted
//   node scripts/clang-format.mjs --fix     rewrite the files in place
//
// The major version is pinned because clang-format changes its output between
// majors, so two developers with different versions would reformat each other's
// files. Plain Node without dependencies, so it runs on every platform.
import { execFileSync, spawnSync } from 'node:child_process'
import path from 'node:path'
import { fileURLToPath } from 'node:url'

const REQUIRED_MAJOR = 20
const EXTENSIONS = ['c', 'cc', 'cpp', 'h', 'mm']
// Windows limits a command line to 32767 characters; stay far below it.
const MAX_COMMAND_LENGTH = 24000

const mode = process.argv[2]
if (process.argv.length !== 3 || (mode !== '--check' && mode !== '--fix')) {
  console.error('usage: node scripts/clang-format.mjs --check | --fix')
  process.exit(2)
}

const root = path.join(path.dirname(fileURLToPath(import.meta.url)), '..')

function fail(message) {
  console.error(`clang-format: ${message}`)
  process.exit(1)
}

function checkVersion() {
  const result = spawnSync('clang-format', ['--version'], { encoding: 'utf8' })
  if (result.error || result.status !== 0) {
    fail(
      `could not run clang-format (${result.error?.message ?? `exit code ${result.status}`}). Install clang-format ${REQUIRED_MAJOR}, see CONTRIBUTING.md#development.`
    )
  }
  const version = result.stdout.trim()
  const major = /version (\d+)\./.exec(version)?.[1]
  if (Number(major) !== REQUIRED_MAJOR) {
    fail(`found "${version}" but major version ${REQUIRED_MAJOR} is required. Install clang-format ${REQUIRED_MAJOR}, see CONTRIBUTING.md#development.`)
  }
}

function trackedSources() {
  const patterns = EXTENSIONS.map((extension) => `*.${extension}`)
  const output = execFileSync('git', ['ls-files', '-z', '--', ...patterns], { cwd: root, encoding: 'utf8' })
  return output.split('\0').filter(Boolean)
}

function batches(files, fixedArguments) {
  const result = []
  let current = []
  let length = fixedArguments.join(' ').length
  for (const file of files) {
    if (current.length > 0 && length + file.length + 1 > MAX_COMMAND_LENGTH) {
      result.push(current)
      current = []
      length = fixedArguments.join(' ').length
    }
    current.push(file)
    length += file.length + 1
  }
  if (current.length > 0) result.push(current)
  return result
}

checkVersion()
const files = trackedSources()
if (files.length === 0) fail('git ls-files found no C, C++ or Objective-C++ sources')

const fixedArguments = mode === '--check' ? ['--dry-run', '-Werror'] : ['-i']
let failed = false
for (const batch of batches(files, fixedArguments)) {
  const result = spawnSync('clang-format', [...fixedArguments, ...batch], { cwd: root, stdio: 'inherit' })
  if (result.error) fail(result.error.message)
  if (result.status !== 0) failed = true
}
if (failed) {
  console.error(mode === '--check' ? 'clang-format: files above are not formatted; run "npm run format"' : 'clang-format: failed')
  process.exit(1)
}
