// Loads the built package and lists the families once: proof that the addon of this machine loads and answers, for a
// target whose test suite is switched off (the `tests` flag of the target table).
//
//   node scripts/ci/load-smoke.mjs
//
// It reads `dist/`, so `npm run build` comes first.
import { createRequire } from 'node:module'

const { getFamilies } = createRequire(import.meta.url)('../../dist/index.js')
const families = await getFamilies({ fields: [] })
console.log(families.length, 'families')
