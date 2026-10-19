// scripts/check-package.mjs, which `npm pack` runs as `prepack`, on fake package trees.
import assert from 'node:assert/strict'
import { mkdirSync, mkdtempSync, rmSync, utimesSync, writeFileSync } from 'node:fs'
import { tmpdir } from 'node:os'
import path from 'node:path'
import { test } from 'node:test'

import { ADDON_FILE, checkPackage } from '../scripts/check-package.mjs'

const TARGET_IDS = ['darwin-arm64', 'linux-x64-glibc']

/** The files of a package that may ship, as paths relative to its root. */
const completePackage = () => [
  'package.json',
  'README.md',
  'README.zh-CN.md',
  'LICENSE',
  'CHANGELOG.md',
  'dist/index.js',
  'dist/index.d.ts',
  'docs/api.md',
  'third_party/fontconfig/COPYING',
  'third_party/fontconfig/README.md',
  ...TARGET_IDS.map((id) => `prebuilds/${id}/${ADDON_FILE}`)
]

/** Writes `files` under a new directory, with a ts/ source older than the compiled JavaScript, and returns the root. */
function makeTree(files, scripts) {
  const root = mkdtempSync(path.join(tmpdir(), 'check-package-'))
  const write = (file, content = '') => {
    mkdirSync(path.dirname(path.join(root, file)), { recursive: true })
    writeFileSync(
      path.join(root, file),
      file === 'package.json' ? JSON.stringify({ main: 'dist/index.js', types: 'dist/index.d.ts', scripts }) : content
    )
  }
  write('ts/index.ts')
  for (const file of files) write(file)
  utimesSync(path.join(root, 'ts/index.ts'), 1000, 1000)
  return root
}

/** Runs the check on a tree made of `files`; the packed files are all of them but the sources. */
function check(files, { packed = files, scripts } = {}) {
  const root = makeTree(files, scripts)
  try {
    return checkPackage({ root, targetIds: TARGET_IDS, packedFiles: packed })
  } finally {
    rmSync(root, { recursive: true })
  }
}

test('a complete package passes', () => {
  assert.deepEqual(check(completePackage()), [])
})

test('every missing prebuild is listed, with the target it belongs to', () => {
  const files = completePackage().filter((file) => !file.startsWith('prebuilds/'))
  const problems = check(files)
  assert.equal(problems.length, TARGET_IDS.length)
  for (const id of TARGET_IDS)
    assert.ok(
      problems.some((problem) => problem.includes(`prebuilds/${id}/${ADDON_FILE}`)),
      id
    )
})

test('a prebuild that exists but is not packed is missing', () => {
  const files = completePackage()
  const packed = files.filter((file) => file !== `prebuilds/darwin-arm64/${ADDON_FILE}`)
  assert.deepEqual(
    check(files, { packed }).map((problem) => problem.split(' ')[0]),
    [`prebuilds/darwin-arm64/${ADDON_FILE}`]
  )
})

test('sources, build files and anything unlisted are refused', () => {
  const unwanted = [
    'binding.gyp',
    'src/core/addon.cpp',
    'test/native.test.mjs',
    'ts/index.ts',
    'scripts/check-package.mjs',
    'build/Release/system-font-info.node',
    'third_party/fontconfig/fc-lang/en.orth',
    'prebuilds/freebsd-x64/system-font-info.node',
    'prebuilds/darwin-arm64/other.node'
  ]
  const problems = check([...completePackage(), ...unwanted])
  assert.deepEqual(problems, unwanted.map((file) => `${file} must not be in the package`).sort())
})

test('required root files and the fontconfig notice must be packed', () => {
  const required = ['README.md', 'README.zh-CN.md', 'LICENSE', 'CHANGELOG.md', 'third_party/fontconfig/COPYING']
  const files = completePackage()
  const problems = check(files, { packed: files.filter((file) => !required.includes(file)) })
  assert.deepEqual(
    problems,
    required.map((file) => `${file} is not in the package`)
  )
})

test('compiled JavaScript that is missing or older than a source is refused', () => {
  assert.deepEqual(
    check(completePackage().filter((file) => !file.startsWith('dist/'))).map((problem) => problem.split(' ')[0]),
    ['dist/index.js', 'dist/index.d.ts']
  )

  const root = makeTree(completePackage())
  try {
    const future = new Date(Date.now() + 60_000)
    utimesSync(path.join(root, 'ts/index.ts'), future, future)
    const problems = checkPackage({ root, targetIds: TARGET_IDS, packedFiles: completePackage() })
    assert.equal(problems.length, 1)
    assert.match(problems[0], /^dist\/ is older than a file in ts\//)
  } finally {
    rmSync(root, { recursive: true })
  }
})

test('a manifest with an install-time script is refused, whatever else it holds', () => {
  const scripts = { build: 'node-gyp rebuild', test: 'node --test', preinstall: 'a', install: 'node-gyp rebuild', postinstall: 'c' }
  assert.deepEqual(
    check(completePackage(), { scripts }).map((problem) => problem.split('`')[1]),
    ['preinstall', 'install', 'postinstall']
  )
  assert.deepEqual(check(completePackage(), { scripts: { prepack: 'x', build: 'y' } }), [])
})
