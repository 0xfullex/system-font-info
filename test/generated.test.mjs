// Generated files must match what their generators produce from the current sources.
import assert from 'node:assert/strict'
import { spawnSync } from 'node:child_process'
import { test } from 'node:test'
import { fileURLToPath } from 'node:url'

const root = fileURLToPath(new URL('..', import.meta.url))

const checkGenerator = (script) => spawnSync(process.execPath, [script, '--check'], { cwd: root, encoding: 'utf8' })

test('src/orthography/orthography_data.cpp is up to date', () => {
  const result = checkGenerator('scripts/gen-orthographies.mjs')
  assert.equal(result.status, 0, result.stderr)
})
