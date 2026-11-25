# Texts

## The lexicon (`lexicon.dic`)

Most short texts of the game (button labels, settings, names) are in one
list, numbered from 0. The game's code asks for texts by number, and the
numbers stay the same with the 1.0.2 patch, which replaces the file.

| Offset | Size | What |
| --- | --- | --- |
| 0 | 4 | Offset of the table |
| table | 4 | Unknown (952) |
| table + 4 | 2 | Number of texts (1460) |
| table + 6 | 8 × count | Per text: 32-bit offset and 32-bit length, counting its closing zero |

Texts are Windows-1252.

## Empire descriptions (`racedesc.rst`)

Texts separated by zero bytes. The first five are the descriptions of the
major empires, in the order Cardassian, Federation, Ferengi, Klingon,
Romulan, shown on the empire choice screen.

## Tooltips (`teachtip.tdb`)

The help text shown when the pointer rests on a button, found by the
button's number in its layout (the same numbers as in the `.wdf` files).

| Offset | Size | What |
| --- | --- | --- |
| 0 | 4 | Number of tooltips (394) |
| 4 | 12 | Unknown; not needed |
| 16 | 16 × count | Per tooltip: button number, length counting the closing zero, an address in the original's memory (not used), and the offset of the text from the end of this table |

Each text starts with a space and has its own line breaks (LF, sometimes
CR LF), which Nagus keeps; the original draws the lines as they are.
