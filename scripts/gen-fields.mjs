// Generates everything that is derived from the field catalogue (ts/field-catalogue.ts):
//
//   src/core/field_catalogue.h   the native planner's field table
//   docs/api.md                  the Presets table and the Result typing code block
//   docs/platforms.md            the group of every field on each platform, and the groups each preset reaches
//
//   node scripts/gen-fields.mjs            write the files
//   node scripts/gen-fields.mjs --check    regenerate in memory, list the files that differ from disk, exit 1 if any
//
// It reads the compiled catalogue and closure, dist/field-catalogue.js and dist/field-closure.js (`npm run generate`
// builds them first). The documents are
// edited only between their `<!-- BEGIN GENERATED: <name> (scripts/gen-fields.mjs) -->` and
// `<!-- END GENERATED: <name> -->` lines; everything else in them is hand-written.
import { readFileSync, writeFileSync } from 'node:fs'
import path from 'node:path'
import { fileURLToPath } from 'node:url'
import { FIELD_CATALOGUE } from '../dist/field-catalogue.js'
import { closureOf, groupsOf, PRESETS, presetFields } from '../dist/field-closure.js'

const root = path.join(path.dirname(fileURLToPath(import.meta.url)), '..')
// In the order of the generated `Platform` enumerators and of the columns of the matrix.
const PLATFORMS = ['darwin', 'win32', 'linux']
const GROUPS = ['A', 'B', 'C', 'D', 'E']

const arguments_ = process.argv.slice(2)
const check = arguments_.includes('--check')
if (arguments_.length > 1 || (arguments_.length === 1 && !check)) {
  console.error('usage: node scripts/gen-fields.mjs [--check]')
  process.exit(2)
}

// ---- catalogue sanity ---------------------------------------------------------------------------------------------

/** A source with groups of OS work, as opposed to null, a constant, an undocumented source or a derivation. */
const isNativeWork = (source) => typeof source === 'object' && source !== null && 'groups' in source
const isDerived = (source) => typeof source === 'object' && source !== null && 'derivedFrom' in source

function validateCatalogue() {
  const names = new Set()
  for (const spec of FIELD_CATALOGUE) {
    if (names.has(spec.name)) throw new Error(`catalogue: ${spec.name} is listed twice`)
    names.add(spec.name)
  }
  for (const spec of FIELD_CATALOGUE) {
    if ((spec.level === 'opentype') !== (spec.tables !== undefined)) {
      throw new Error(`catalogue: ${spec.name}: only (and every) opentype entry lists its tables`)
    }
    for (const platform of PLATFORMS) {
      const source = spec.sources[platform]
      for (const input of source?.derivedFrom ?? []) {
        if (!names.has(input)) throw new Error(`catalogue: ${spec.name} (${platform}) is derived from ${input}, which is not a field`)
        if (FIELD_CATALOGUE.find((other) => other.name === input).sources[platform] === null) {
          throw new Error(`catalogue: ${spec.name} (${platform}) is derived from ${input}, which the platform does not provide`)
        }
      }
      const groups = isNativeWork(source) ? [...source.groups, ...(source.when ?? []).map((w) => w.group)] : []
      for (const group of groups) {
        if (!GROUPS.includes(group)) throw new Error(`catalogue: ${spec.name} (${platform}): unknown group ${group}`)
      }
    }
  }
}

// ---- src/core/field_catalogue.h -----------------------------------------------------------------------------------

const LEVEL_NAMES = { family: 'Family', face: 'Face', opentype: 'OpenType', platform: 'Platform' }

/** `faces.opentype.os2` -> `FacesOpentypeOs2`. */
function enumerator(name) {
  return name
    .split('.')
    .map((part) => part[0].toUpperCase() + part.slice(1))
    .join('')
}

/** The tag as sfnt::makeTag encodes it: four ASCII characters, the first in the most significant byte. */
function tagValue(tag) {
  if (!/^[\x20-\x7e]{4}$/.test(tag)) throw new Error(`catalogue: ${JSON.stringify(tag)} is not a four-character table tag`)
  let value = 0
  for (const character of tag) value = value * 256 + character.charCodeAt(0)
  return value
}

function maskExpression(groups) {
  return groups.length === 0 ? '0' : groups.map((group) => `kGroup${group}`).join(' | ')
}

function platformSourceExpression(source) {
  if (source === null) return '{false, false, 0, 0}'
  if (!isNativeWork(source)) return `{true, ${isDerived(source)}, 0, 0}`
  const conditional = (source.when ?? []).map((w) => w.group)
  return `{true, false, ${maskExpression(source.groups)}, ${maskExpression(conditional)}}`
}

function generateHeader() {
  const maxTables = Math.max(1, ...FIELD_CATALOGUE.map((spec) => spec.tables?.length ?? 0))
  const rows = FIELD_CATALOGUE.map((spec) => {
    const sources = PLATFORMS.map((platform) => platformSourceExpression(spec.sources[platform])).join(', ')
    const tags = (spec.tables ?? []).map((tag) => `0x${tagValue(tag).toString(16).toUpperCase().padStart(8, '0')} /* ${tag} */`)
    return `    {Field::${enumerator(spec.name)}, "${spec.name}", Level::${LEVEL_NAMES[spec.level]}, {{${sources}}}, {{${tags.join(', ')}}}},`
  })
  const enumerators = FIELD_CATALOGUE.map((spec) => `  ${enumerator(spec.name)},`)

  return `// Generated by scripts/gen-fields.mjs from ts/field-catalogue.ts. Do not edit by hand.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

// Every field a caller can select, and what the native side needs to know about it: its level in the result, and on
// each platform whether it is provided, whether the JS layer derives it there and which groups of OS work it needs
// (the groups of docs/platforms.md, "Source groups"). Plain standard C++20: this header is shared by all platforms.

namespace sfi {

// The fields, in catalogue order; \`kFields[size_t(field)]\` describes \`field\`.
enum class Field : uint16_t {
${enumerators.join('\n')}
};
inline constexpr size_t kFieldCount = ${FIELD_CATALOGUE.length};

// Where a field sits in the result.
enum class Level : uint8_t { Family, Face, OpenType, Platform };

enum class Platform : uint8_t { Darwin, Win32, Linux };
inline constexpr size_t kPlatformCount = 3;

// The groups of OS work, as a bit set.
enum class Group : uint8_t { A, B, C, D, E };
using GroupMask = uint8_t;
inline constexpr GroupMask kGroupA = 1 << size_t(Group::A);
inline constexpr GroupMask kGroupB = 1 << size_t(Group::B);
inline constexpr GroupMask kGroupC = 1 << size_t(Group::C);
inline constexpr GroupMask kGroupD = 1 << size_t(Group::D);
inline constexpr GroupMask kGroupE = 1 << size_t(Group::E);

// One field on one platform.
struct PlatformSource {
  // False: the platform does not provide the field.
  bool available;
  // The JS layer computes the field on this platform from other fields; the native side is never asked for it. A
  // platform that gets the same field from the OS itself has groups instead.
  bool derived;
  // The groups the field always needs, and those it needs only in some cases (for example macOS \`faces.format\`
  // needs C only to tell CFF2 from CFF). Both are empty for a field with no OS work (a constant, or one the package
  // derives) and for one whose source has not been established.
  GroupMask always;
  GroupMask conditional;
};

// The most tables one \`opentype\` entry is made of.
inline constexpr size_t kMaxEntryTables = ${maxTables};

struct FieldInfo {
  Field field;
  // The field name as callers write it, for example "faces.opentype.os2".
  std::string_view name;
  Level level;
  std::array<PlatformSource, kPlatformCount> platforms;
  // The tables of an \`opentype\` entry, as sfnt::makeTag encodes their tags; unused slots are 0. The entry
  // \`tables\` has none: it is the table directory itself.
  std::array<uint32_t, kMaxEntryTables> tables;
};

// clang-format off
inline constexpr std::array<FieldInfo, kFieldCount> kFields = {{
${rows.join('\n')}
}};
// clang-format on

constexpr bool fieldsAreInEnumOrder() {
  for (size_t i = 0; i < kFieldCount; ++i) {
    if (size_t(kFields[i].field) != i) return false;
  }
  return true;
}
static_assert(fieldsAreInEnumOrder(), "kFields must list the fields in the order of the Field enumerators");

}  // namespace sfi
`
}

// ---- docs ---------------------------------------------------------------------------------------------------------

function replaceRegion(text, name, body, file) {
  const begin = `<!-- BEGIN GENERATED: ${name} (scripts/gen-fields.mjs) -->`
  const end = `<!-- END GENERATED: ${name} -->`
  const from = text.indexOf(begin)
  const to = text.indexOf(end)
  if (from < 0 || to < from) throw new Error(`${file}: the generated region "${name}" is missing its begin/end markers`)
  return `${text.slice(0, from + begin.length)}\n${body}\n${text.slice(to)}`
}

/** The Presets table of api.md: each preset lists what it adds to the one before. */
function presetsTable() {
  const lines = ['| Preset | Contains |', '| --- | --- |']
  PRESETS.forEach((preset, index) => {
    const specs = FIELD_CATALOGUE.filter((spec) => spec.preset === preset)
    // A level that the preset takes whole (every platform field, every OpenType entry) is written as a pattern.
    const wholeLevels = ['opentype', 'platform'].filter((level) => {
      const inLevel = FIELD_CATALOGUE.filter((spec) => spec.level === level)
      return specs.filter((spec) => spec.level === level).length === inLevel.length
    })
    const named = specs.filter((spec) => !wholeLevels.includes(spec.level)).map((spec) => `\`${spec.name}\``)
    const patterns = wholeLevels.map((level) => `every \`faces.${level}.<${level === 'opentype' ? 'entry' : 'field'}>\``)
    const parts = [...(named.length > 0 ? [named.join(', ')] : []), ...patterns]
    const lead = index === 0 ? '' : `\`${PRESETS[index - 1]}\`, and `
    lines.push(`| \`${preset}\` | ${lead}${parts.join(', and ')} |`)
  })
  return lines.join('\n')
}

function typesRegion() {
  const text = readFileSync(path.join(root, 'ts/types.ts'), 'utf8')
  const match = /^\/\/ #region selection-types\n([\s\S]*?)^\/\/ #endregion$/m.exec(text)
  if (!match) throw new Error('ts/types.ts: the selection-types region is missing')
  return `\`\`\`ts\n${match[1].trimEnd()}\n\`\`\``
}

function renderSource(source) {
  if (source === null) return '—'
  if (source === 'constant') return source
  if (source === 'undocumented') return 'not yet documented'
  if (isDerived(source)) return 'derived'
  const conditions = (source.when ?? []).map((w) => `${w.group} for ${w.for}`)
  if (source.groups.length === 0) return conditions.map((c) => c.replace(' for ', ', for ')).join('; ')
  return [source.groups.join(', '), ...conditions].join('; ')
}

function groupMatrix() {
  const lines = ['| Field | macOS | Windows | Linux |', '| --- | --- | --- | --- |']
  for (const spec of FIELD_CATALOGUE) {
    lines.push(`| \`${spec.name}\` | ${PLATFORMS.map((platform) => renderSource(spec.sources[platform])).join(' | ')} |`)
  }
  return lines.join('\n')
}

/** The "groups each preset reaches" table: the groups the fields of each preset cost, per platform. */
function presetGroupsTable() {
  const render = ({ always, conditional }) =>
    [always.join(', '), ...conditional.map(({ group, for: condition }) => `${group} for ${condition}`)].join('; ')
  const rows = [['`fields: []` (`family` only)', []]]
  for (const preset of PRESETS) rows.push([`\`${preset}\``, presetFields(preset)])
  const lines = ['| Selection | macOS | Windows | Linux |', '| --- | --- | --- | --- |']
  for (const [label, fields] of rows) {
    const cells = PLATFORMS.map((platform) => render(groupsOf(closureOf(fields, platform), platform)))
    lines.push(`| ${label} | ${cells.join(' | ')} |`)
  }
  return lines.join('\n')
}

// ---- run ----------------------------------------------------------------------------------------------------------

validateCatalogue()

const outputs = new Map()
outputs.set('src/core/field_catalogue.h', generateHeader())

function readIfExists(file) {
  try {
    return readFileSync(path.join(root, file), 'utf8')
  } catch (error) {
    if (error.code === 'ENOENT') return null
    throw error
  }
}

function editDocument(file, regions) {
  let text = readFileSync(path.join(root, file), 'utf8')
  for (const [name, body] of regions) text = replaceRegion(text, name, body, file)
  outputs.set(file, text)
}
editDocument('docs/api.md', [
  ['presets', presetsTable()],
  ['result-typing', typesRegion()]
])
editDocument('docs/platforms.md', [
  ['field-groups', groupMatrix()],
  ['preset-groups', presetGroupsTable()]
])

if (check) {
  const stale = []
  for (const [file, content] of outputs) {
    if (readIfExists(file) !== content) stale.push(file)
  }
  if (stale.length > 0) {
    console.error(`generated content is out of date; run \`npm run generate\`:\n${stale.map((file) => `  ${file}`).join('\n')}`)
    process.exit(1)
  }
} else {
  for (const [file, content] of outputs) writeFileSync(path.join(root, file), content)
}
