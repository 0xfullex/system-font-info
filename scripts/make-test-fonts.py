#!/usr/bin/env python3
"""Generates the OpenType test fonts in test/fixtures/.

Run from the repository root, in a virtual environment that has fontTools:

    python3 -m venv .venv && .venv/bin/pip install fonttools
    .venv/bin/python scripts/make-test-fonts.py

The output is deterministic (fixed timestamps), so regenerating on any machine
with the same fontTools version produces byte-identical files. Family and
PostScript names all start with "SFITest" so they never collide with fonts
installed on a system.

Fonts written:
  SFITest-Variable.ttf   TrueType variable font: wght 100-900, localized names,
                         STAT, meta, GSUB with several language systems and
                         stylistic-set names, OS/2 version 5, cmap 4 and 12,
                         a format 1 name table with a language-tag record.
  SFITest-Collection.ttc Two static faces (TrueType and CFF) with different
                         name tables.
  SFITest-Symbol.ttf     A symbol font whose only cmap subtable is (3,0).
  SFITest-Legacy.ttf     Windows typographic names (16, 17) in en-US only, and
                         legacy names (1, 2) on the Macintosh platform only:
                         English, plus Turkish, Icelandic and Polish text in
                         the Mac Roman variants those languages use, and
                         description records (nameID 10) in the East Asian
                         Windows code pages, stored as 16-bit units.
"""

import struct
import sys
from contextlib import contextmanager
from datetime import datetime, timezone
from pathlib import Path

from fontTools.feaLib.builder import addOpenTypeFeaturesFromString
from fontTools.fontBuilder import FontBuilder, buildCmapSubTable
from fontTools.pens.t2CharStringPen import T2CharStringPen
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.ttLib import TTCollection, TTFont, newTable
from fontTools.ttLib.tables import _n_a_m_e
from fontTools.ttLib.tables._g_v_a_r import TupleVariation

OUT = Path(__file__).resolve().parent.parent / "test" / "fixtures"


def seconds_since_1904(*utc):
    return int((datetime(*utc, tzinfo=timezone.utc) - datetime(1904, 1, 1, tzinfo=timezone.utc)).total_seconds())


CREATED = seconds_since_1904(2020, 1, 2, 3, 4, 5)
MODIFIED = seconds_since_1904(2021, 6, 7, 8, 9, 10)
UNITS_PER_EM = 1000


def square(pen, x0, y0, x1, y1):
    pen.moveTo((x0, y0))
    pen.lineTo((x0, y1))
    pen.lineTo((x1, y1))
    pen.lineTo((x1, y0))
    pen.closePath()


# glyph name -> (advance width, rectangle)
GLYPH_BOXES = {
    ".notdef": (600, (50, 0, 550, 700)),
    "space": (300, None),
    "A": (500, (50, 0, 450, 700)),
    "B": (500, (50, 0, 450, 600)),
    "f": (400, (50, 0, 350, 700)),
    "i": (300, (50, 0, 250, 700)),
    "f_i": (650, (50, 0, 600, 700)),
    "uni1F600": (800, (100, 0, 700, 700)),
}
GLYPH_ORDER = list(GLYPH_BOXES)
CMAP = {0x20: "space", 0x41: "A", 0x42: "B", 0x66: "f", 0x69: "i", 0x1F600: "uni1F600"}


def truetype_glyphs():
    glyphs = {}
    for name, (_, box) in GLYPH_BOXES.items():
        pen = TTGlyphPen(None)
        if box:
            square(pen, *box)
        glyphs[name] = pen.glyph()
    return glyphs


def metrics():
    return {name: (adv, box[0] if box else 0) for name, (adv, box) in GLYPH_BOXES.items()}


def setup_common(fb, family, style, ps_name, *, extra_names=None):
    fb.setupGlyphOrder(GLYPH_ORDER)
    fb.setupCharacterMap(CMAP)
    fb.setupHorizontalMetrics(metrics())
    fb.setupHorizontalHeader(ascent=800, descent=-200)
    names = {
        "copyright": "SFITest fixture, public domain",
        "familyName": family,
        "styleName": style,
        "uniqueFontIdentifier": f"{ps_name};SFITest;1.000",
        "fullName": f"{family} {style}",
        "version": "Version 1.000",
        "psName": ps_name,
    }
    names.update(extra_names or {})
    fb.setupNameTable(names)
    fb.setupPost()
    fb.font["head"].created = CREATED
    fb.font["head"].modified = MODIFIED


def new_builder(is_ttf=True):
    fb = FontBuilder(UNITS_PER_EM, isTTF=is_ttf)
    fb.font.recalcTimestamp = False
    fb.setupHead(unitsPerEm=UNITS_PER_EM, created=CREATED, modified=MODIFIED)
    return fb


# ---- name table with a language-tag record (format 1) ------------------------

LANGUAGE_TAGS = ["gsw"]


def compile_name_format1(self, ttFont):
    """Replacement for table__n_a_m_e.compile that writes format 1 when the
    font declares LANGUAGE_TAGS; fontTools itself only writes format 0."""
    names = sorted(self.names)
    string_data = b""
    records = b""
    done = {}

    def store(data):
        nonlocal string_data
        if data not in done:
            done[data] = (len(string_data), len(data))
            string_data += data
        return done[data]

    for name in names:
        offset, length = store(name.toBytes())
        records += struct.pack(">HHHHHH", name.platformID, name.platEncID, name.langID, name.nameID, length, offset)
    tag_records = b""
    for tag in LANGUAGE_TAGS:
        offset, length = store(tag.encode("utf_16_be"))
        tag_records += struct.pack(">HH", length, offset)
    header_size = 6 + len(names) * 12 + 2 + len(tag_records)
    header = struct.pack(">HHH", 1, len(names), header_size)
    return header + records + struct.pack(">H", len(LANGUAGE_TAGS)) + tag_records + string_data


# ---- the variable font --------------------------------------------------------

FEATURES = """
languagesystem DFLT dflt;
languagesystem latn dflt;
languagesystem latn TRK;

feature liga {
    sub f i by f_i;
} liga;

feature ss01 {
    featureNames {
        name "Alternate B";
        name 3 1 0x804 "\\5907\\9009 B";
    };
    sub A by B;
} ss01;

feature locl {
    script latn;
    language TRK required;
    sub i by f;
} locl;
"""


@contextmanager
def name_table_format1():
    """Writes name tables as format 1 while the context is open (saving
    compiles the table, so the font must be saved inside it)."""
    original = _n_a_m_e.table__n_a_m_e.compile
    _n_a_m_e.table__n_a_m_e.compile = compile_name_format1
    try:
        yield
    finally:
        _n_a_m_e.table__n_a_m_e.compile = original


def build_variable():
    fb = new_builder()
    setup_common(
        fb,
        "SFITest Variable",
        "Regular",
        "SFITestVariable-Regular",
        extra_names={"typographicFamily": "SFITest Variable", "typographicSubfamily": "Regular"},
    )
    fb.setupGlyf(truetype_glyphs())
    fb.setupOS2(
        version=5,
        sTypoAscender=800,
        sTypoDescender=-200,
        sTypoLineGap=0,
        usWinAscent=800,
        usWinDescent=200,
        usWeightClass=400,
        achVendID="SFI ",
        sxHeight=500,
        sCapHeight=700,
        usLowerOpticalPointSize=8,
        usUpperOpticalPointSize=72,
    )
    fb.font["OS/2"].ulCodePageRange1 = 1
    fb.font["OS/2"].ulCodePageRange2 = 0

    fb.setupFvar(
        [("wght", 100, 400, 900, {"en": "Weight", "zh": "\u5b57\u91cd"})],
        [
            {"location": {"wght": 100}, "stylename": "Thin", "postscriptfontname": "SFITestVariable-Thin"},
            {"location": {"wght": 400}, "stylename": "Regular", "postscriptfontname": "SFITestVariable-Regular"},
            {
                "location": {"wght": 700},
                "stylename": {"en": "Bold", "zh": "\u7c97\u4f53"},
                "postscriptfontname": "SFITestVariable-Bold",
            },
        ],
    )
    # Glyph A: four outline points plus the four phantom points.
    fb.setupGvar(
        {
            "A": [TupleVariation({"wght": (0.0, 1.0, 1.0)}, [(0, 0), (0, 100), (60, 100), (60, 0)] + [(0, 0)] * 4)],
            "B": [TupleVariation({"wght": (-1.0, -1.0, 0.0)}, [(20, 0), (20, 0), (-20, 0), (-20, 0)] + [(0, 0)] * 4)],
        }
    )
    fb.setupStat(
        [
            {
                "tag": "wght",
                "name": "Weight",
                "ordering": 0,
                "values": [
                    {"value": 100, "name": "Thin"},
                    {"value": 400, "name": "Regular", "flags": 2},
                    {"value": 700, "name": "Bold"},
                ],
            }
        ]
    )

    name = fb.font["name"]
    # Windows: family (1) and full name (4) in several languages.
    localized = {
        0x0804: ("SFITest \u53d8\u91cf", "SFITest \u53d8\u91cf Regular"),  # zh-CN
        0x0411: ("SFITest \u30d0\u30ea\u30a2\u30d6\u30eb", "SFITest \u30d0\u30ea\u30a2\u30d6\u30eb Regular"),  # ja-JP
        0x0419: ("SFITest \u041f\u0435\u0440\u0435\u043c\u0435\u043d\u043d\u044b\u0439", "SFITest \u041f\u0435\u0440\u0435\u043c\u0435\u043d\u043d\u044b\u0439 Regular"),  # ru-RU
        0x0401: ("SFITest \u0645\u062a\u063a\u064a\u0631", "SFITest \u0645\u062a\u063a\u064a\u0631 Regular"),  # ar-SA
    }
    for lang_id, (family, full) in localized.items():
        name.setName(family, 1, 3, 1, lang_id)
        name.setName(full, 4, 3, 1, lang_id)
    # Macintosh: English comes from setupNameTable; add Chinese Simplified
    # (script 25, language 33), which needs a double-byte legacy encoding.
    name.setName("SFITest \u53d8\u91cf", 1, 1, 25, 33)
    name.setName("SFITest \u53d8\u91cf Regular", 4, 1, 25, 33)
    # Format 1: a Windows record whose language is the tag "gsw" (0x8000).
    name.setName("SFITest Alemannisch", 1, 3, 1, 0x8000)
    # A Windows record in a non-BMP-heavy string keeps surrogate pairs honest.
    name.setName("SFITest Variable \U0001F600", 10, 3, 10, 0x0409)

    addOpenTypeFeaturesFromString(fb.font, FEATURES)

    meta = newTable("meta")
    meta.data = {"dlng": "Latn", "slng": "Latn, Cyrl,Arab ,Hans"}
    fb.font["meta"] = meta

    return fb.font


# ---- the collection ------------------------------------------------------------


def build_static_face(family, style, ps_name, *, cff):
    fb = new_builder(is_ttf=not cff)
    setup_common(fb, family, style, ps_name)
    if cff:
        glyphs = {}
        for name, (adv, box) in GLYPH_BOXES.items():
            pen = T2CharStringPen(adv, None)
            if box:
                square(pen, *box)
            glyphs[name] = pen.getCharString()
        fb.setupCFF(ps_name, {"FullName": f"{family} {style}", "FamilyName": family}, glyphs, {})
    else:
        fb.setupGlyf(truetype_glyphs())
    fb.setupOS2(
        sTypoAscender=800,
        sTypoDescender=-200,
        usWinAscent=800,
        usWinDescent=200,
        usWeightClass=700 if style == "Bold" else 400,
        achVendID="SFI ",
    )
    # A name table that differs between the faces.
    fb.font["name"].setName(f"{family} only in this face", 10, 3, 1, 0x0409)
    return fb.font


def build_collection():
    regular = build_static_face("SFITest Collection", "Regular", "SFITestCollection-Regular", cff=False)
    bold = build_static_face("SFITest Collection", "Bold", "SFITestCollection-Bold", cff=True)
    collection = TTCollection()
    collection.fonts = [regular, bold]
    return collection


# ---- the symbol font -----------------------------------------------------------


def build_symbol():
    fb = new_builder()
    setup_common(fb, "SFITest Symbol", "Regular", "SFITestSymbol-Regular")
    cmap = newTable("cmap")
    cmap.tableVersion = 0
    cmap.tables = [buildCmapSubTable({0xF041: "A", 0xF042: "B"}, 4, 3, 0)]
    fb.font["cmap"] = cmap
    fb.setupGlyf(truetype_glyphs())
    fb.setupOS2(
        sTypoAscender=800,
        sTypoDescender=-200,
        usWinAscent=800,
        usWinDescent=200,
        achVendID="SFI ",
        ulCodePageRange1=1 << 31,  # bit 31: symbol character set
        usFirstCharIndex=0xF041,
        usLastCharIndex=0xF042,
    )
    return fb.font


# ---- the font with Macintosh-only legacy names -----------------------------------


def windows_code_page_units(text, codec):
    """`text` in a Windows multi-byte code page, one 16-bit unit per character."""
    units = b""
    for char in text:
        encoded = char.encode(codec)
        units += encoded if len(encoded) == 2 else b"\x00" + encoded
    return units


def build_legacy():
    fb = new_builder()
    setup_common(fb, "SFITest Legacy", "Book", "SFITestLegacy-Book")
    fb.setupGlyf(truetype_glyphs())
    fb.setupOS2(sTypoAscender=800, sTypoDescender=-200, usWinAscent=800, usWinDescent=200, achVendID="SFI ")

    name = fb.font["name"]
    name.names = []
    for name_id, text in {
        0: "SFITest fixture, public domain",
        3: "SFITestLegacy-Book;SFITest;1.000",
        4: "SFITest Legacy Book",
        5: "Version 1.000",
        6: "SFITestLegacy-Book",
    }.items():
        name.setName(text, name_id, 3, 1, 0x0409)
        name.setName(text, name_id, 1, 0, 0)
    # Windows: the typographic family and subfamily, English only.
    name.setName("SFITest Legacy", 16, 3, 1, 0x0409)
    name.setName("Book", 17, 3, 1, 0x0409)
    # Macintosh: the legacy family and subfamily, which differ from the
    # typographic ones the way "Avenir Book" differs from "Avenir".
    name.setName("SFITest Legacy Book", 1, 1, 0, 0)
    name.setName("Regular", 2, 1, 0, 0)
    # Turkish (language 17) and Icelandic (language 15) use their own Mac Roman
    # variants: the same bytes mean other characters than in plain Mac Roman.
    name.setName("Yaz\u0131 Tipi \u011e\u011f \u015e\u015f \u0130", 1, 1, 0, 17)
    name.setName("D\u00fcz", 2, 1, 0, 17)
    name.setName("Letur \u00de\u00fe \u00d0\u00f0", 1, 1, 0, 15)
    name.setName("Venjulegt", 2, 1, 0, 15)
    # Descriptions (nameID 10), which no family or style name reads. Polish
    # (language 25) uses the Central European Mac Roman variant.
    name.setName("Opis kr\u00f3j \u0105\u0119\u015b\u017c\u0142", 10, 1, 0, 25)
    # The East Asian Windows code pages (encodings 2-5). Each character is a
    # 16-bit unit: a double-byte character keeps its two bytes and a
    # single-byte one gets a zero high byte, so the strings hold zero bytes.
    for encoding_id, codec, language_id, text in [
        (2, "cp932", 0x0411, "\u65e5\u672c\u8a9e ABC"),
        (3, "cp936", 0x0804, "\u7b80\u4f53\u4e2d\u6587 ABC"),
        (4, "cp950", 0x0404, "\u7e41\u9ad4\u4e2d\u6587 ABC"),
        (5, "cp949", 0x0412, "\ud55c\uad6d\uc5b4 ABC"),
    ]:
        name.names.append(_n_a_m_e.makeName(windows_code_page_units(text, codec), 10, 3, encoding_id, language_id))
    return fb.font


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    with name_table_format1():
        build_variable().save(OUT / "SFITest-Variable.ttf")
    build_collection().save(OUT / "SFITest-Collection.ttc")
    build_symbol().save(OUT / "SFITest-Symbol.ttf")
    build_legacy().save(OUT / "SFITest-Legacy.ttf")
    print(f"wrote fixtures to {OUT}")


if __name__ == "__main__":
    sys.exit(main())
