// The target table and the loader, with the environment (platform, architecture, libc, require) injected.
import assert from 'node:assert/strict'
import { readFileSync } from 'node:fs'
import path from 'node:path'
import { test } from 'node:test'

import { loadBinding } from '../dist/native.js'
import { detectLibc, findTarget, TARGETS, targetName } from '../dist/targets.js'

const ROOT = '/pkg'
const prebuilt = (id) => path.join(ROOT, 'prebuilds', id, 'system-font-info.node')
const localBuild = path.join(ROOT, 'build', 'Release', 'system-font-info.node')

/** An environment whose requires succeed for `available` ids / files and fail with the given messages otherwise. */
function environment({ platform, arch, libc = 'glibc', available = [], failures = {} }) {
  const calls = { libc: 0, prebuilt: [], files: [] }
  const result = (key) => {
    if (available.includes(key)) return { loaded: key, getFamilies() {} }
    throw new Error(failures[key] ?? `Cannot find module '${key}'`)
  }
  return {
    calls,
    platform,
    arch,
    packageRoot: ROOT,
    libc: () => {
      calls.libc++
      return libc
    },
    requirePrebuilt: (id) => {
      calls.prebuilt.push(id)
      return result(id)
    },
    requireFile: (file) => {
      calls.files.push(file)
      return result(file)
    }
  }
}

test('every target id is its platform, architecture and libc, and no two targets share one', () => {
  for (const target of TARGETS) assert.equal(target.id, targetName(target.platform, target.arch, target.libc), target.id)
  assert.equal(new Set(TARGETS.map((target) => target.id)).size, TARGETS.length)
  assert.deepEqual(
    TARGETS.map((target) => target.id),
    ['darwin-arm64', 'darwin-x64', 'win32-x64', 'win32-arm64', 'linux-x64-glibc', 'linux-arm64-glibc']
  )
  // libc is a Linux property.
  for (const target of TARGETS) assert.equal(target.libc !== undefined, target.platform === 'linux', target.id)
})

test('findTarget matches platform, architecture and libc', () => {
  assert.equal(findTarget('darwin', 'arm64')?.id, 'darwin-arm64')
  assert.equal(findTarget('linux', 'arm64', 'glibc')?.id, 'linux-arm64-glibc')
  assert.equal(findTarget('linux', 'x64', 'musl'), undefined)
  assert.equal(findTarget('linux', 'x64'), undefined)
  assert.equal(findTarget('win32', 'ia32'), undefined)
  assert.equal(findTarget('freebsd', 'x64'), undefined)
})

test('detectLibc names the libc of this process once', (t) => {
  if (process.platform !== 'linux') return t.skip('Linux only')
  assert.equal(detectLibc(), typeof process.report.getReport().header.glibcVersionRuntime === 'string' ? 'glibc' : 'musl')
  assert.equal(detectLibc(), detectLibc())
})

test('the literal requires of native.ts are exactly the table targets, each with its own directory', () => {
  const source = readFileSync(new URL('../ts/native.ts', import.meta.url), 'utf8')
  const literals = [...source.matchAll(/case '([^']+)':\s*return require\('\.\.\/prebuilds\/\1\/system-font-info\.node'\)/g)].map(
    (m) => m[1]
  )
  assert.deepEqual(literals.sort(), TARGETS.map((target) => target.id).sort())
  // Every require of a prebuilt is one of those cases: none is built from a variable.
  assert.equal([...source.matchAll(/require\(/g)].length, literals.length + 1) // + the local build
})

for (const target of TARGETS) {
  test(`${target.id} loads prebuilds/${target.id}/system-font-info.node`, () => {
    const env = environment({ platform: target.platform, arch: target.arch, libc: target.libc, available: [target.id] })
    assert.equal(loadBinding(env).loaded, target.id)
    assert.deepEqual(env.calls.prebuilt, [target.id])
    // The local build is tried first and, absent, falls through.
    assert.deepEqual(env.calls.files, [path.join(ROOT, 'build', 'Release', 'system-font-info.node')])
    // The libc is asked on Linux only.
    assert.equal(env.calls.libc, target.platform === 'linux' ? 1 : 0)
  })
}

test('a local build is used before the prebuilt addon, so a stale prebuild never shadows it', () => {
  const env = environment({ platform: 'darwin', arch: 'arm64', available: [localBuild, 'darwin-arm64'] })
  assert.equal(loadBinding(env).loaded, localBuild)
  assert.deepEqual(env.calls.prebuilt, [])
})

test('the prebuilt addon is used when there is no local build', () => {
  const env = environment({ platform: 'darwin', arch: 'arm64', available: ['darwin-arm64'] })
  assert.equal(loadBinding(env).loaded, 'darwin-arm64')
  assert.deepEqual(env.calls.files, [localBuild])
})

test('a platform without a target can only use a local build, and the error names the supported targets', () => {
  const env = environment({ platform: 'freebsd', arch: 'x64', available: [localBuild] })
  assert.equal(loadBinding(env).loaded, localBuild)
  assert.deepEqual(env.calls.prebuilt, [])

  const error = thrown(() => loadBinding(environment({ platform: 'win32', arch: 'ia32' })))
  assert.match(error.message, /no prebuilt addon for win32-ia32 \(prebuilt targets: darwin-arm64, .*linux-arm64-glibc\)/)
  assert.ok(error.message.includes(localBuild), error.message)
})

test('a module that is not the addon is refused, naming the path and the bundler', () => {
  for (const replacement of [{}, '/pkg/prebuilds/win32-x64/system-font-info.node', null]) {
    const env = { ...environment({ platform: 'win32', arch: 'x64' }), requirePrebuilt: () => replacement }
    const error = thrown(() => loadBinding(env))
    assert.ok(error.message.includes(`${prebuilt('win32-x64')}: loaded, but it is not the addon`), error.message)
    assert.match(error.message, /a bundler may have replaced the \.node file/)
  }
})

test('musl is reported as not supported yet', () => {
  const env = environment({ platform: 'linux', arch: 'x64', libc: 'musl' })
  const error = thrown(() => loadBinding(env))
  assert.match(error.message, /^system-font-info: musl libc is not supported yet, so there is no prebuilt addon for linux-x64-musl/)
  assert.deepEqual(env.calls.prebuilt, [])
})

test('the error lists every path tried with its underlying failure', () => {
  const env = environment({ platform: 'win32', arch: 'x64', failures: { 'win32-x64': 'bad image', [localBuild]: 'no such file' } })
  assert.equal(
    thrown(() => loadBinding(env)).message,
    [
      'system-font-info: cannot load the native addon for win32-x64. Tried:',
      `  ${localBuild}: no such file`,
      `  ${prebuilt('win32-x64')}: bad image`
    ].join('\n')
  )
})

/** The error `fn` throws; fails when it does not throw. */
function thrown(fn) {
  try {
    fn()
  } catch (error) {
    return error
  }
  throw new assert.AssertionError({ message: 'expected the call to throw' })
}

test('a missing fontconfig or FreeType on Linux gets an install hint naming the package', () => {
  const missing = (soname) => `${soname}: cannot open shared object file: No such file or directory`
  const run = (platform, failures) => thrown(() => loadBinding(environment({ platform, arch: 'arm64', failures })))
  const id = 'linux-arm64-glibc'

  const fontconfig = run('linux', { [id]: missing('libfontconfig.so.1') })
  assert.match(
    fontconfig.message,
    /libfontconfig\.so\.1 is not installed\. Install it: apt install libfontconfig1 .*dnf install fontconfig/
  )
  assert.doesNotMatch(fontconfig.message, /libfreetype/)
  assert.ok(fontconfig.message.includes(prebuilt(id)), fontconfig.message) // the listing is still there

  const freetype = run('linux', { [id]: missing('libfreetype.so.6') })
  assert.match(freetype.message, /libfreetype\.so\.6 is not installed\. Install it: apt install libfreetype6 .*dnf install freetype/)

  // Another failure on Linux, and the same text on another platform, give no hint.
  assert.doesNotMatch(run('linux', { [id]: 'invalid ELF header' }).message, /Install it/)
  assert.doesNotMatch(run('darwin', { 'darwin-arm64': missing('libfontconfig.so.1') }).message, /Install it/)
})
