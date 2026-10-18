// Compiles the fontconfig orthography files (third_party/fontconfig/fc-lang/*.orth) into
// src/orthography/orthography_data.cpp, the table of orthographies built into the addon.
//
//   node scripts/gen-orthographies.mjs           write the generated file
//   node scripts/gen-orthographies.mjs --check   regenerate in memory and compare with the file on disk;
//                                                list the stale files and exit 1, write nothing
//
// The .orth format is read the way fontconfig's own fc-lang/fc-lang.py reads it:
//   - `include other.orth` (at the start of a line) splices that file's lines in place;
//   - `#` starts a comment, and a tab ends the code point text (the rest is the character's name);
//   - a line is `XXXX`, `XXXX-YYYY` or `XXXX..YYYY`, hexadecimal with an optional `0x` prefix;
//   - a line that starts with `-` deletes its code points (mni.orth removes one included from bn.orth).
// Lines apply in order, so a deletion only removes what the lines before it added.
//
// An orthography is named like fc-lang.py names it: the file name without `.orth`, `_` replaced by `-`,
// lower case (`zh_cn.orth` is `zh-cn`). fontconfig's own table is not reused because it holds
// fontconfig's internal character set leaves, not code points.
import { readdirSync, readFileSync, writeFileSync } from 'node:fs'
import path from 'node:path'
import { fileURLToPath } from 'node:url'

const root = path.join(path.dirname(fileURLToPath(import.meta.url)), '..')
const orthDirectory = path.join(root, 'third_party/fontconfig/fc-lang')
const fontconfigReadme = path.join(root, 'third_party/fontconfig/README.md')
const dataFile = 'src/orthography/orthography_data.cpp'

const MAX_CODE_POINT = 0x10ffff
const ROWS_PER_LINE = 6
// Counts the language-coverage contract states (docs/api.md, `LanguageCoverage`); a parser regression
// that changes them fails generation instead of silently shipping wrong `required` values.
const KNOWN_COUNTS = { 'zh-cn': 6765, 'zh-tw': 13063, ja: 2314 }
const NAME_PATTERN = /^[a-z0-9]+(-[a-z0-9]+)*$/
const LINE_PATTERN = /^(-)?\s*(?:0x)?([0-9a-f]+)(?:(?:-|\.\.)(?:0x)?([0-9a-f]+))?$/i

function fail(message) {
  console.error(`gen-orthographies: ${message}`)
  process.exit(2)
}

function fontconfigCommit() {
  const match = /commit `([0-9a-f]{40})`/.exec(readFileSync(fontconfigReadme, 'utf8'))
  if (!match) fail(`no 40-digit commit in ${path.relative(root, fontconfigReadme)}`)
  return match[1]
}

/** The operations of one .orth file, includes expanded in place: { remove, first, last } in file order. */
function readOperations(fileName, including = []) {
  if (including.includes(fileName)) fail(`${fileName}: include cycle (${[...including, fileName].join(' -> ')})`)
  const operations = []
  const lines = readFileSync(path.join(orthDirectory, fileName), 'utf8').split(/\r?\n/)
  lines.forEach((rawLine, index) => {
    if (rawLine.startsWith('include ')) {
      operations.push(...readOperations(rawLine.slice('include '.length).trim(), [...including, fileName]))
      return
    }
    const text = rawLine.split('#')[0].split('\t')[0].trim()
    if (text === '') return
    const match = LINE_PATTERN.exec(text)
    if (!match) fail(`${fileName}:${index + 1}: cannot parse "${rawLine}"`)
    const first = Number.parseInt(match[2], 16)
    const last = match[3] === undefined ? first : Number.parseInt(match[3], 16)
    if (first > last || last > MAX_CODE_POINT) fail(`${fileName}:${index + 1}: invalid range "${rawLine}"`)
    operations.push({ remove: match[1] !== undefined, first, last })
  })
  return operations
}

/** Sorted, merged, inclusive [first, last] ranges and the number of code points. */
function compile(fileName) {
  const codePoints = new Set()
  for (const { remove, first, last } of readOperations(fileName)) {
    for (let codePoint = first; codePoint <= last; codePoint++) {
      if (remove) codePoints.delete(codePoint)
      else codePoints.add(codePoint)
    }
  }
  const sorted = [...codePoints].sort((a, b) => a - b)
  const ranges = []
  for (const codePoint of sorted) {
    const previous = ranges.at(-1)
    if (previous && previous[1] + 1 === codePoint) previous[1] = codePoint
    else ranges.push([codePoint, codePoint])
  }
  return { ranges, codePointCount: sorted.length }
}

function compileAll() {
  const orthographies = readdirSync(orthDirectory)
    .filter((fileName) => fileName.endsWith('.orth'))
    .map((fileName) => ({
      name: fileName.slice(0, -'.orth'.length).replaceAll('_', '-').toLowerCase(),
      ...compile(fileName)
    }))
    .sort((a, b) => (a.name < b.name ? -1 : a.name > b.name ? 1 : 0))
  for (let index = 0; index < orthographies.length; index++) {
    const { name } = orthographies[index]
    if (!NAME_PATTERN.test(name)) fail(`orthography name "${name}" is not lower-case ASCII words joined by "-"`)
    if (index > 0 && orthographies[index - 1].name === name) fail(`two files give the orthography name "${name}"`)
  }
  for (const [name, expected] of Object.entries(KNOWN_COUNTS)) {
    const actual = orthographies.find((orthography) => orthography.name === name)?.codePointCount
    if (actual !== expected) fail(`${name} has ${actual} code points, expected ${expected}`)
  }
  return orthographies
}

const hex = (codePoint) => `0x${codePoint.toString(16).toUpperCase().padStart(4, '0')}`

function render(orthographies, commit) {
  const rows = []
  const table = []
  for (const { name, ranges, codePointCount } of orthographies) {
    table.push(`    {"${name}", {kRanges + ${rows.length}, ${ranges.length}}, ${codePointCount}},`)
    for (const [first, last] of ranges) rows.push(`{${hex(first)}, ${hex(last)}}`)
  }
  const rangeLines = []
  for (let index = 0; index < rows.length; index += ROWS_PER_LINE) {
    rangeLines.push(`    ${rows.slice(index, index + ROWS_PER_LINE).join(', ')},`)
  }
  return [
    '// Generated by scripts/gen-orthographies.mjs. Do not edit by hand.',
    `// Source: fontconfig fc-lang/*.orth at commit ${commit}, see third_party/fontconfig/README.md.`,
    "// The data is derived from fontconfig's files; its license notice is third_party/fontconfig/COPYING.",
    '#include "orthography.h"',
    '',
    'namespace sfi::orthography::detail {',
    '',
    '// clang-format off',
    "// Every orthography's code points as ascending, non-adjacent, inclusive ranges, one orthography after another.",
    'constexpr Range kRanges[] = {',
    ...rangeLines,
    '};',
    '',
    '// Ascending by name; `ranges` points into kRanges.',
    'const Orthography kOrthographies[] = {',
    ...table,
    '};',
    '// clang-format on',
    '',
    `const size_t kOrthographyCount = ${orthographies.length};`,
    '',
    '}  // namespace sfi::orthography::detail',
    ''
  ].join('\n')
}

const mode = process.argv[2]
if (process.argv.length > 3 || (mode !== undefined && mode !== '--check')) {
  console.error('usage: node scripts/gen-orthographies.mjs [--check]')
  process.exit(2)
}

const outputs = new Map([[dataFile, render(compileAll(), fontconfigCommit())]])

if (mode === '--check') {
  const stale = [...outputs].filter(([file, content]) => {
    try {
      return readFileSync(path.join(root, file), 'utf8') !== content
    } catch {
      return true
    }
  })
  if (stale.length > 0) {
    console.error(`stale generated files (run \`npm run generate:orthographies\`):\n${stale.map(([file]) => `  ${file}`).join('\n')}`)
    process.exit(1)
  }
} else {
  for (const [file, content] of outputs) writeFileSync(path.join(root, file), content)
}
