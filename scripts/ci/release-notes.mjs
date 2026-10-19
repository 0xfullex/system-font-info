// Prints the section of CHANGELOG.md that belongs to one version: the notes of its GitHub Release.
//
//   node scripts/ci/release-notes.mjs <version> [changelog path]
//
// A section starts at a `## ` heading whose first word is the version, bare or in brackets (`## 1.2.0 - 2026-03-01`,
// `## [1.2.0] - 2026-03-01`), and ends at the next `## ` heading. It fails, with a message, when the version has no
// section or the section is empty, so a Release is never created with the wrong or no notes.
import { readFileSync } from 'node:fs'
import { fileURLToPath, pathToFileURL } from 'node:url'

/**
 * The notes of `version` in `changelog`, trimmed.
 *
 * @param {string} changelog the text of CHANGELOG.md
 * @param {string} version
 * @returns {string}
 * @throws {Error} when the version has no section, or its section is empty
 */
export function releaseNotes(changelog, version) {
  const lines = changelog.split(/\r?\n/)
  const isHeading = (line) => line.startsWith('## ')
  const headingVersion = (line) =>
    line
      .slice(3)
      .trim()
      .split(/\s+/)[0]
      .replace(/^\[(.*)\]$/, '$1')

  const start = lines.findIndex((line) => isHeading(line) && headingVersion(line) === version)
  if (start === -1) throw new Error(`CHANGELOG.md has no section for ${version}`)

  const rest = lines.slice(start + 1)
  const end = rest.findIndex(isHeading)
  const notes = (end === -1 ? rest : rest.slice(0, end)).join('\n').trim()
  if (notes === '') throw new Error(`The section of ${version} in CHANGELOG.md is empty`)
  return notes
}

if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) {
  const [version, changelogPath = fileURLToPath(new URL('../../CHANGELOG.md', import.meta.url))] = process.argv.slice(2)
  if (!version) {
    console.error('usage: node scripts/ci/release-notes.mjs <version> [changelog path]')
    process.exit(2)
  }
  try {
    console.log(releaseNotes(readFileSync(changelogPath, 'utf8'), version))
  } catch (error) {
    console.error(`::error::${error.message}`)
    process.exit(1)
  }
}
