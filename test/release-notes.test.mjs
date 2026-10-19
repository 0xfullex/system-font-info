// scripts/ci/release-notes.mjs, which takes one version's section out of CHANGELOG.md for its GitHub Release.
import assert from 'node:assert/strict'
import { test } from 'node:test'

import { releaseNotes } from '../scripts/ci/release-notes.mjs'

const CHANGELOG = `# Changelog

Intro.

## Unreleased

- Not yet.

## [1.1.0] - 2026-02-01

- Added b.

More about b.

## 1.0.0-rc.1 - 2026-01-15

- Candidate.

## 1.0.0 - 2026-01-01

- Added a.

## 0.9.0

`

test('takes the section of a version up to the next heading', () => {
  assert.equal(releaseNotes(CHANGELOG, '1.0.0'), '- Added a.')
})

test('reads a bracketed version heading and keeps the whole body', () => {
  assert.equal(releaseNotes(CHANGELOG, '1.1.0'), '- Added b.\n\nMore about b.')
})

test('does not take a pre-release section for its release version', () => {
  assert.equal(releaseNotes(CHANGELOG, '1.0.0-rc.1'), '- Candidate.')
})

test('fails when the version has no section', () => {
  assert.throws(() => releaseNotes(CHANGELOG, '2.0.0'), /no section for 2\.0\.0/)
  assert.throws(() => releaseNotes(CHANGELOG, '1.0'), /no section for 1\.0/)
})

test('fails when the section is empty', () => {
  assert.throws(() => releaseNotes(CHANGELOG, '0.9.0'), /section of 0\.9\.0 .* is empty/)
})
