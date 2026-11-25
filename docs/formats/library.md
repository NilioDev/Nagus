# The game files and Nagus's library

## What the player has

The game came on one CD. Images of it are usually a `.bin`/`.cue` pair (raw
2352-byte sectors, mode 1 or mode 2) or an `.iso` (2048-byte sectors), often
in a zip. The CD holds an ISO 9660 file system. The files Nagus needs are:

| File | What |
| --- | --- |
| `ENGLISH/STBOF.RES` | The main resource archive: pictures, layouts, fonts, texts, tables. |
| `ALT.RES` | More resources. |
| `MUSIC.SND`, `SFX.SND`, `ENGLISH/ENGLISH.SND` | Music, sound effects and speech. |
| `ENGLISH/*.AVI`, `*.AVI` | Films. |

An installed game has the same files in its folder.

The 1.0.2 patch (`botf102e.zip`) holds a list file `patch.lst` and the files
it replaces inside `STBOF.RES` (a handful of text and rule tables).

## Import

Nagus reads the CD image as a stream: it looks up the directory entries it
needs and copies only the wanted files (`.res`, `.snd`, `.avi`, `.wav`), in
the order they lie on the disc, so a CD image in a zip is never unpacked to
disk first. Folders are searched up to four levels deep; names are matched
without regard to case.

The library, in the app's data folder:

```
library/
  base/            the game's files, names in lower case
    manifest.txt   "nagus-base 1" and "game"
    patch/         the patch's files, names in lower case
      manifest.txt "nagus-patch 1" and "patch 1.0.2"
```

Each part is written into a staging folder first and renamed into place
when complete, so an interrupted import leaves nothing half-done. The
manifest is written last.

## Resource archives

`STBOF.RES` is an ordinary zip archive (stored and deflated entries).
Names are matched without regard to case. Files in `library/base/patch`
take the place of the archive's files with the same name.

`ALT.RES` holds the end-of-game pictures, for files `STBOF.RES` does not
have. It is our own reading:

| Field | Type |
| --- | --- |
| Name ("alt.res") | 16 bytes |
| Where the list is | u32 |
| Number of files | u32 |
| The packed files | bytes |
| The list: for each file name (16 bytes), offset, number, size, packed size, method (0 stored, 1 LZSS), a check value we do not use | 16 bytes + 6 × u32 |

LZSS: a flag byte comes before each eight items, lowest bit first. A set
bit is a byte as it is; a clear bit is two bytes `a b`: copy
`(a >> 4) + 2` bytes from place `(a & 15) << 8 | b` of a 4096-byte window.
Every byte unpacked also goes into the window, starting at place 1; the
window starts as zeros. The reader checks that every file lies inside the
archive and stops when the packed data runs out.
