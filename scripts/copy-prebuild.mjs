// Copies the freshly built addon to prebuilds/<platform>-<arch>/, where the
// package loads it from. `--arch <arch>` names the target architecture of a
// cross build; the default is the architecture of this process.
import { copyFileSync, existsSync, mkdirSync } from 'node:fs'
import { dirname, join } from 'node:path'
import { fileURLToPath } from 'node:url'
import { parseArgs } from 'node:util'

const { values } = parseArgs({ options: { arch: { type: 'string', default: process.arch } } })

const root = join(dirname(fileURLToPath(import.meta.url)), '..')
const source = join(root, 'build', 'Release', 'system-font-info.node')
if (!existsSync(source)) {
  console.error(`copy-prebuild: ${source} does not exist; build the addon first (npm run build:native)`)
  process.exit(1)
}

const targetDir = join(root, 'prebuilds', `${process.platform}-${values.arch}`)
mkdirSync(targetDir, { recursive: true })
const target = join(targetDir, 'system-font-info.node')
copyFileSync(source, target)
console.log(`copy-prebuild: ${target}`)
