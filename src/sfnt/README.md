# src/sfnt

A platform-independent reader for the standard OpenType tables. Every platform
backend hands it table bytes (from CoreText, DirectWrite, or the font file
itself), so the font's own metadata comes out identically on all platforms.
The public API is `sfnt.h`; the other headers and `.cpp` files are private to
this directory.

The reader is our own code, not HarfBuzz or FreeType: it needs a few hundred
lines of fixed-layout parsing and nothing of shaping, rasterizing or hinting,
and both libraries are orders of magnitude larger than that.

## Files

| File | Owns |
| --- | --- |
| `sfnt.h` | The public API and the types it exchanges. |
| `reader.h` | `Reader`: bounds-checked big-endian access; every read past the table throws `ParseError`. |
| `text.cpp` | UTF-16BE and Latin-1 to UTF-8, hex. |
| `names.cpp` | `name` table parsing, `localizedName*`, LCID / Mac language lookup. |
| `fvar.cpp` | `fvar` parsing and its `opentype.fvar` value. |
| `layout.cpp` | GSUB / GPOS script, language system and feature lists, feature UI names. |
| `opentype_tables.cpp` | head, hhea/vhea, OS/2, post, maxp, cmap, meta. |
| `font_tables.cpp` | `readFontTables`: fetches the tables and assembles the `opentype` value. |
| `file.cpp` | `FontFile`: sfnt and collection directories, table bytes on demand. |
| `language_tables.{h,cpp}` | Generated LCID and Mac language code to BCP 47 tables. |

## Name decoding

Records stay in table order and nothing is merged here: `localizedName` and
the layout feature names list every decoded record, exact duplicates included,
because merging them is the JS layer's job. For each record:

- Language: platform 3 maps the LCID, platform 1 the Mac language code (binary
  search over the generated tables); a format 1 table's `languageId >= 0x8000`
  is the language-tag record it indexes. Platforms 0 and 2 have no language
  (""). An unknown id also gives "" and `languageId` keeps the raw value.
- Text: platform 0 (any encoding), platform 3 encodings 0, 1 and 10, and
  platform 2 encoding 1 are UTF-16BE; surrogate pairs are combined, a lone
  surrogate or a trailing odd byte becomes U+FFFD. Platform 2 encoding 2 is
  ISO 8859-1; encoding 0 is ASCII, and a byte above 0x7F leaves the record
  undecoded. A zero-length string is "" in every encoding.
- Everything else (platform 1, platform 3 encodings 2-6, unknown pairs) goes to
  the `LegacyDecoder` with the raw `languageId`, because a Mac code page can
  depend on the language (MacIcelandic, MacTurkish...). When it returns nothing
  (or an empty string), `value` is absent and `bytesHex` holds the raw bytes.
- A record whose string lies outside the table is a `ParseError` for the whole
  table, except a zero-length one, whose offset is ignored.

## Legacy fallback

`localizedNameWithFallback(16, 1)` (and `17, 2`) lists the typographic records,
then the legacy records of every language the typographic ones do not cover.
"Same language" compares a language key: the primary subtag, so a Mac "en"
`ID 1` record does not appear next to a Windows "en-US" `ID 16` one. Chinese is
the exception, because its two written languages are named separately: the key
carries the script (explicit `Hans` / `Hant`, else CN and SG mean `Hans`, TW, HK
and MO mean `Hant`, else plain "zh"), so a CJK font that has English typographic
names keeps its Simplified or Traditional Chinese legacy names.

## Values

`readFontTables` builds the `opentype` value described by `OpenTypeInfo` in
`ts/types.ts`. Numbers are as stored; the interpretations are:

- Versions (`head`, `hhea`, `vhea`, `post`, `maxp`) are Version16Dot16: major
  plus the top hex digit of the minor word as tenths (`0x00011000` is 1.1).
  A minor word that is not of that form is read as a fraction of 65536.
- `head.created` / `modified`: LONGDATETIME (signed seconds since
  1904-01-01 UTC) as `YYYY-MM-DDTHH:MM:SSZ`. The value is converted as stored,
  even when the font evidently wrote a Unix timestamp; there is no guessing.
  A date whose year falls outside 0001-9999 is left out of `head`.
- `os2`: a field group of a newer version is read only when the table is long
  enough to hold all of it. Optical sizes are TWIPs / 20, in points.
  `vendorId` is the four bytes as stored, each byte as the same code point.
- `fvar`: Fixed 16.16 as doubles. The instance PostScript name ID exists only
  when `instanceSize == axisCount * 4 + 6`; 0xFFFF ("none") counts as absent.
  `postscriptName` is the first decoded record of that nameID, any language.
- `meta`: `dlng` / `slng` are split on commas, trimmed, empty items dropped.
- `layout`: language systems list the required feature first, then the others
  in order, each tag once; the default language system is `dflt`. Tags are as
  stored, so a three-letter tag keeps its trailing space (`"TRK "`).
  `featureNames` covers `ssXX` (FeatureParams UINameID) and `cvXX`
  (FeatUILabelNameID); a nameID of 0 means none, and features sharing a tag
  contribute all of their name records, in feature order.
- STAT is not read.

## Invariants

- `readFontTables` takes the tag list in any order and with repeats; it sorts
  and de-duplicates it, so `opentype.tables` is ascending whatever the caller
  passes.
- Malformed data throws `ParseError` from the parsing functions; nothing reads
  out of bounds. `readFontTables` never throws for table content: a table that
  is present but cannot be fetched or parsed is left out and its tag is listed
  in `opentype.errors` (ascending). A malformed `name` table gives empty
  `names` and an error entry, and `fvar` / layout names are then empty too.
- `FontFile::open` returns nullopt for anything that is not an sfnt (`0x00010000`,
  `OTTO`, `true`, `typ1`) or a `ttcf` collection of any version. A face of a
  collection whose directory cannot be read keeps its index with no tables.
  Table bytes are read from disk on demand; only directories are kept in memory.
  Paths are UTF-8 and go through `std::filesystem`.

## Regenerating

- `language_tables.cpp` is generated, never edited by hand: see the header of
  `scripts/gen-language-tables.c` (macOS only).
- The test fonts in `test/fixtures/` come from `scripts/make-test-fonts.py`
  (fontTools; the script's header says how to run it). Output is deterministic,
  so regenerating produces no diff unless the script or fontTools changes.
