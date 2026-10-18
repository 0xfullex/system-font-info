// Loads an installed copy of the package in the main process of a real Electron and lists the fonts.
//
//   electron scripts/ci/electron-smoke.mjs <project directory> [--no-sandbox]
//
// <project directory> is a directory in which `system-font-info` is installed (the release workflow installs the packed
// tarball into an empty one), so what runs is what a user's app gets. The script exits 1 on any failure.
//
// What no unit test can see: that the addon loads in Electron's Node (a different build of V8 and Node-API than the
// Node.js the tests run on), that its calls resolve there, and, on Linux, which fontconfig the process holds. Chromium
// carries its own copy of fontconfig, but exports none of its symbols, so the addon must bind the system library that
// GTK and Pango use; the check reads /proc/self/maps and fails if the process maps two libfontconfig files, or one that
// is not the system's.
import assert from 'node:assert/strict'
import { readFileSync, realpathSync } from 'node:fs'
import { createRequire } from 'node:module'
import path from 'node:path'
import { app } from 'electron'

const projectDirectory = process.argv[2]
if (!projectDirectory || projectDirectory.startsWith('--')) {
  console.error('usage: electron scripts/ci/electron-smoke.mjs <project directory> [--no-sandbox]')
  app.exit(2)
}

/** The libfontconfig files this process has mapped, by real path. */
function mappedFontconfigs() {
  const maps = readFileSync('/proc/self/maps', 'utf8')
  return [...new Set([...maps.matchAll(/(\/\S*libfontconfig\.so\S*)/g)].map((match) => realpathSync(match[1])))]
}

function checkLinuxFontconfig() {
  const libraries = mappedFontconfigs()
  console.log('mapped libfontconfig:', libraries)
  assert.equal(libraries.length, 1, `expected exactly one libfontconfig mapped, found ${libraries.length}`)
  const [library] = libraries
  assert.match(path.basename(library), /^libfontconfig\.so\.1(\.|$)/, 'not a libfontconfig.so.1')
  assert.match(library, /^\/(usr\/)?lib/, `${library} is not under the system library directories`)
  for (const own of [path.dirname(process.execPath), path.resolve(projectDirectory)]) {
    assert.ok(!library.startsWith(`${own}${path.sep}`), `${library} is bundled with ${own}, not the system's`)
  }
}

async function smoke() {
  console.log(`electron ${process.versions.electron}, node ${process.versions.node}, node-api ${process.versions.napi}`)
  console.log(`${process.platform} ${process.arch}`)
  const require = createRequire(path.join(path.resolve(projectDirectory), 'package.json'))
  const { getFamilies } = require('system-font-info')
  console.log('package:', require.resolve('system-font-info'))

  const plain = await getFamilies({ fields: [] })
  assert.ok(plain.length > 0, 'getFamilies({ fields: [] }) returned no families')
  assert.equal(typeof plain[0].family, 'string')
  const names = await getFamilies({ preset: 'names' })
  assert.ok(names.length > 0, "getFamilies({ preset: 'names' }) returned no families")
  assert.ok(Array.isArray(names[0].familyNames) && names[0].familyNames.length > 0, 'the first family has no familyNames')
  console.log(`${plain.length} families, ${names.length} with names`)

  if (process.platform === 'linux') checkLinuxFontconfig()
}

smoke().then(
  () => {
    console.log('electron smoke: ok')
    app.exit(0)
  },
  (error) => {
    console.error('electron smoke: failed\n', error)
    app.exit(1)
  }
)
