# Films

The films are loose AVI files next to the resource archive:

| File | What | Sound |
| --- | --- | --- |
| `open.avi` | the opening film, 800×300, 1804 frames | `intro.wav` (separate) |
| `c.avi`, `h.avi`, `f.avi`, `k.avi`, `r.avi` | each empire's film, shown when a new game starts | in the file |

## AVI

A RIFF file of type `AVI `. The reader walks its chunks and lists (at
most four deep), taking:

- `avih`: the time of a frame in microseconds (first u32);
- `strh`: each stream's type (`vids` or `auds`);
- `strf`: for the picture, width and height (i32) and the compression,
  which must be `cvid`; for the sound, format (1, PCM), channels, rate and
  bits;
- in `movi` (also inside `rec ` lists): chunks named by stream number and
  kind (`00dc`, `00id`, `01wb`, ...). Picture chunks are remembered by
  place; sound is read.

Sizes are checked against the file and the list they are in; a cut-off
file plays up to where it ends.

## Cinepak

Each frame (big-endian numbers): flags (u8), size (u24), width, height,
number of strips (3 × u16), then the strips. A strip: kind (0x1000 key,
0x1100 between), size, top, left, bottom, right (6 × u16); when top is 0
the strip lies under the one before and bottom is its height. Then
chunks of kind and size (2 × u16):

| Kind | What |
| --- | --- |
| 0x2000 / 0x2200 | v4 / v1 code book: up to 256 entries of Y0..Y3, U, V (6 bytes) |
| 0x2100 / 0x2300 | the same, only the entries whose bit is set (32-bit flag words) |
| 0x2400–0x2700 | the same in grey (4 bytes an entry) |
| 0x3000 | the 4×4 blocks: a bit for each, v4 (four indices, one for each 2×2 part) or v1 (one index, each Y for a 2×2 part) |
| 0x3100 | the same, with a bit before each: whether the block changes |
| 0x3200 | v1 indices only |

A strip starts with the code books of the strip above unless bit 0 of
the frame's flags is set; code books carry over from frame to frame.
Colour: red = Y + 2V, green = Y − U/2 − V, blue = Y + 2U.
