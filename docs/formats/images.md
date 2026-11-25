# Pictures and pointers

## Pictures (`.tga`)

Uncompressed Targa pictures (image type 2) with 16 bits per pixel, five bits
each for red, green and blue (bit 15 unused). The 18-byte header holds the
width and height as 16-bit numbers at offsets 12 and 14, the bits per pixel
at 16 and the descriptor at 17. Rows run from the bottom up unless bit 5 of
the descriptor is set. The colour `0x7c1f` (full red and blue, magenta) is
see-through.

## Pointers (`.cur`)

A 16-byte header of 16-bit numbers: hot spot x and y, number of frames,
milliseconds per frame, width, height; then a 32-bit size of the picture
that follows. Then one Targa picture with all frames stacked from the top,
each `height / frames` rows high.

## Animations (`.ani`)

A 16-byte header of 16-bit numbers: frame width, frame height, number of
frames, a second number (often the same), sheet width and sheet height; then
the 32-bit size of the Targa picture that follows. The picture holds the
frames in rows, left to right.

The planets' animations are named by four letters: the type (`a`rctic,
`b`arren, `d`esert, `g`as giant, `j`ungle, `o`ceanic, `t`erran,
`v`olcanic), the atmosphere (`m`ethane, `n`one, oxygen `r`ich, `s`ulfuric,
`t`hin oxygen), a variant letter and the size (`s`, `m`, `l`). The planets
of the Sol system have their own (`eart.ani`, `jupi.ani`, ...); stars and
other map objects use their map code (`s-y-.ani`).
