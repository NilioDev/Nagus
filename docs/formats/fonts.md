# Fonts (`.fnt`)

Each font is a picture of all its glyphs plus a table of their widths.
There are five faces (one per empire) in four sizes (9, 15, 20 and 28).

| Offset | Size | What |
| --- | --- | --- |
| 0 | 1 | Cell height |
| 6 | 2 | Number of glyphs, 120 |
| 20 | 2 × count | Width of each glyph |
| 272 | 2 + 2 | Width and height of the glyph sheet |
| 276 | | The sheet: 16-bit pixels as in Targa pictures, bottom row first |

The sheet has five rows of glyphs, in this order:

1. `1234567890`
2. `A` to `Z`
3. `a` to `z`
4. `` `-=\[];',./~!@#$%^&*()_+|{}:"<>? ``
5. `äâàêèéïîöôüñÇçÄÖÜß®©™…–¿¡·` (Windows-1252)

Within a row, glyphs are separated by vertical lines of one colour (the
commonest colour on the top line of the sheet that is not black). Some lines
are two pixels wide, and the last glyph of a row may have no closing line,
so its width comes from the width table. Glyph pixels are shades of grey;
Nagus uses their brightness as coverage, so text can be drawn in any colour.
Characters the font lacks are skipped; a space is as wide as the digit `1`.
