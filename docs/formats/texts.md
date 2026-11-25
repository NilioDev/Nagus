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
