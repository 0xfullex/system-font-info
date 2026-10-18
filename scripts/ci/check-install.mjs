// Checks a clean install of the packed package: it was not built on install, and it loads and lists fonts.
//
//   node scripts/ci/check-install.mjs <project directory>
//
// <project directory> is an empty directory in which the tarball was installed with npm or pnpm. What a unit test
// cannot see is the package as an installer leaves it: that `files` shipped what the loader needs, that no install
// step compiled anything, and that the prebuilt addon of this machine loads from there.
import assert from 'node:assert/strict'
import { existsSync } from 'node:fs'
import { createRequire } from 'node:module'
import path from 'node:path'

const projectDirectory = process.argv[2]
if (!projectDirectory) {
  console.error('usage: node scripts/ci/check-install.mjs <project directory>')
  process.exit(2)
}

const require = createRequire(path.join(path.resolve(projectDirectory), 'package.json'))
const packageDirectory = path.dirname(require.resolve('system-font-info/package.json'))
console.log('package:', packageDirectory)
for (const file of ['build', 'binding.gyp', 'src']) {
  assert.ok(!existsSync(path.join(packageDirectory, file)), `${file} exists in the installed package`)
}

const { getFamilies } = require('system-font-info')
const families = await getFamilies({ fields: [] })
assert.ok(families.length > 0, 'getFamilies returned no families')
console.log(`${process.platform} ${process.arch}, node ${process.version}: ${families.length} families`)
