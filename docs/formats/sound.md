# Sound

The game's sounds are loose files next to the resource archive. All
numbers are little-endian. Every reader checks counts, offsets and sizes
against the file before reading.

## IMA ADPCM

Music and speech are kept as IMA ADPCM, the common 4-bit encoding, in
blocks of 512 bytes for each channel. A block starts, for each channel,
with the first sample (i16), the step index (u8, 0 to 88) and a spare
byte. Then follow groups of four bytes for each channel in turn, each
byte two samples (low half first). Sounds are 22050 samples a second.

## sfx.snd and english.snd

| Field | Type |
| --- | --- |
| Where the table is | u32 |
| The sounds | bytes |
| Table, to the end of the file: for each sound offset, size, length, flags | 4 × u32 |

In `sfx.snd` (163 sounds) each sound is a whole WAV file (16-bit, one
channel). In `english.snd` (1364 speeches) each is raw IMA ADPCM, one
channel; the length is the size in bytes once decoded. Flag 2 means two
channels.

## music.snd

| Field | Type |
| --- | --- |
| For each track: number of pieces, where its table is | 2 × u32 |
| The pieces' sound | bytes |
| For each track, its table: for each piece offset, size, length, flags | 4 × u32 |

The track list runs up to the first piece's sound (16 tracks). Pieces are
raw IMA ADPCM, two channels (flags 3); the length is the size in bytes once
decoded. A track is played piece after piece, then again from the first.

`music.tbl` in the resource archive seems to say how pieces may follow each
other; we do not use it yet.

## Which sound is which

Our reading, from how the files are grouped and how long they are:

- Track 0 is the main theme, played in the menus. Tracks 1 to 5 are the
  empires' themes (Cardassian, Federation, Ferengi, Klingon, Romulan),
  played during the game. Tracks 6 to 15 are shorter pieces not used yet.
- Sounds 76 to 80 are the button clicks of the five empires.

## WAV files

The end-of-game speeches (`*eog*.wav`), the film sound (`intro.wav`) and
`test.wav` are WAV files: 16-bit PCM or IMA ADPCM (format 0x11).
