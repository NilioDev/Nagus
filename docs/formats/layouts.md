# Screen layouts (`.wdf`)

A layout gives a screen or window its background picture and places its
elements. All numbers are little-endian; names are 13 bytes, padded with
spaces or zeros.

## Header (105 bytes)

| Offset | Size | What |
| --- | --- | --- |
| 0 | 4 | Version, 2 |
| 4 | 4 | Always 1 |
| 8 | 4 | The layout's number |
| 12 | 16 | x, y, width, height (32-bit each) |
| 28 | 13 | Background picture name, or `none` |

## Elements

Elements follow one after another until the end of the file. Every element
starts with the same 24 bytes: type, number (the code knows each element by
it), x, y, width and height, all 32-bit. What follows depends on the type:

| Type | Total size | What |
| --- | --- | --- |
| 2 | 76 | Button |
| 7 | 119 | Slider |
| 8 | 73 | Scroll bar |
| 13 | 41 | Picture: its name at 24 |
| 14 | 101 | Text field |

**Button.** A picture name at 24, a 32-bit number at 37, the style at 41
(2: a toggle button with on and off pictures), 1 at 42 when holding the
button repeats it (the arrows of a number field), the picture when up at 50
and when pressed at 63.

**Text field.** The font is named at 88. The colour's red, green and blue
bytes are at 53, flags at 56, and the 32-bit number at 84 is 1 when the
layout sets the colour itself; otherwise the code chooses it.

Slider and scroll bar pictures are names at fixed places in their records;
Nagus does not use them yet.
