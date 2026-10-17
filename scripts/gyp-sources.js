// Prints the C/C++/Objective-C++ sources of the given directories for
// binding.gyp, so a source file added to a directory is compiled without
// editing the build file. Paths are relative to the package root, '/'-separated.
// biome-ignore lint/suspicious/noRedundantUseStrict: a CommonJS script, run by binding.gyp, where the directive is not redundant
'use strict'
const fs = require('node:fs')
const path = require('node:path')

const root = path.join(__dirname, '..')
const sources = []
for (const dir of process.argv.slice(2)) {
  for (const name of fs.readdirSync(path.join(root, dir)).sort()) {
    if (/\.(c|cc|cpp|mm)$/.test(name)) sources.push(`${dir}/${name}`)
  }
}
process.stdout.write(sources.join(' '))
