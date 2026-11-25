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

`STBOF.RES` and `ALT.RES` are ordinary zip archives (stored and deflated
entries). Names are matched without regard to case. Files in
`library/base/patch` take the place of the archive's files with the same name.
