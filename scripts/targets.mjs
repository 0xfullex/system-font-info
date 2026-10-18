// Prints the target table (ts/targets.ts) for scripts and CI, so no other file lists the targets.
//
//   node scripts/targets.mjs --matrix    the table as a GitHub Actions matrix: {"include":[{"id":…,"runner":…,"tests":…}, …]}
//
// It reads the compiled table, dist/targets.js (`npm run build:ts` builds it).
import { TARGETS } from '../dist/targets.js'

if (process.argv.length !== 3 || process.argv[2] !== '--matrix') {
  console.error('usage: node scripts/targets.mjs --matrix')
  process.exit(2)
}
console.log(JSON.stringify({ include: TARGETS }))
