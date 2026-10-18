// The generated files must match what their generators produce from the current sources. A generator run with
// --check lists the files that differ and exits 1; the fix is `npm run generate`. The field generator reads dist/,
// which `npm run build:catalogue` (and `pretest`) builds.
import assert from 'node:assert/strict'
import { spawnSync } from 'node:child_process'
import { test } from 'node:test'
import { fileURLToPath } from 'node:url'

const root = fileURLToPath(new URL('..', import.meta.url))

const checkGenerator = (script) => spawnSync(process.execPath, [script, '--check'], { cwd: root, encoding: 'utf8' })

test('the files generated from the field catalogue are up to date', () => {
  const result = checkGenerator('scripts/gen-fields.mjs')
  assert.equal(result.status, 0, `${result.stderr}${result.stdout}`)
})

test('src/orthography/orthography_data.cpp is up to date', () => {
  const result = checkGenerator('scripts/gen-orthographies.mjs')
  assert.equal(result.status, 0, result.stderr)
})
