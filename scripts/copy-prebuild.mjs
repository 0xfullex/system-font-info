// Copies the freshly built addon to prebuilds/<target id>/, where the package loads it from (the ids are the
// target table's, ts/targets.ts: `npm run build:ts` compiles it to dist/targets.js). The target is this
// machine's platform and, on Linux, libc. `--arch <arch>` names the target architecture of a cross build; the
// default is the architecture of this process. `--strip` removes the symbols a user's process does not need from the
// copy, as a release build does (the package ships six of these files); a local build keeps them for debugging.
import { execFileSync } from 'node:child_process'
import { copyFileSync, existsSync, mkdirSync } from 'node:fs'
import { dirname, join } from 'node:path'
import { fileURLToPath } from 'node:url'
import { parseArgs } from 'node:util'
import { detectLibc, findTarget, targetName } from '../dist/targets.js'

const { values } = parseArgs({
  options: { arch: { type: 'string', default: process.arch }, strip: { type: 'boolean', default: false } }
})

const root = join(dirname(fileURLToPath(import.meta.url)), '..')
const source = join(root, 'build', 'Release', 'system-font-info.node')
if (!existsSync(source)) {
  console.error(`copy-prebuild: ${source} does not exist; build the addon first (npm run build:native)`)
  process.exit(1)
}

const libc = process.platform === 'linux' ? detectLibc() : undefined
const target = findTarget(process.platform, values.arch, libc)
if (!target) {
  console.error(`copy-prebuild: ${targetName(process.platform, values.arch, libc)} is not a target of the package (ts/targets.ts)`)
  process.exit(1)
}

const targetDir = join(root, 'prebuilds', target.id)
mkdirSync(targetDir, { recursive: true })
const destination = join(targetDir, 'system-font-info.node')
copyFileSync(source, destination)
if (values.strip) {
  // The system's own `strip` of the machine that built the file: no cross builds. macOS re-signs the file it changes.
  // Windows has nothing to strip: the debug symbols are a separate .pdb file that is not copied.
  if (process.platform === 'darwin') execFileSync('strip', ['-x', destination], { stdio: 'inherit' })
  if (process.platform === 'linux') execFileSync('strip', ['--strip-unneeded', destination], { stdio: 'inherit' })
}
console.log(`copy-prebuild: ${destination}`)
