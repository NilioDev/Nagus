# Game tables

Little-endian numbers; names are fixed-size fields ended by a zero byte.
Bytes not listed are not understood yet (many hold leftovers from the memory
of the program that wrote the files).

## Races (`race.rst`)

A 16-bit count (35), then 108-byte records: the five empires first
(Cardassians, Federation, Ferengi, Klingons, Romulans: this is the order of
the empires everywhere in the tables), then the minor races.

| Offset | Size | What |
| --- | --- | --- |
| 0 | 40 | Name |
| 40 | 16 | Portrait picture |

## Map objects (`objstruc.smt`)

A 16-bit count (12), then 24-byte records:

| Offset | Size | What |
| --- | --- | --- |
| 0 | 2 | Index |
| 2 | 2 | Always 64 |
| 4 | 4 | Share of the map objects of this kind (a 32-bit float); the shares add up to 1 |
| 8 | 6 | Code |

The codes are `blkh` (black hole), `xp-` (X-ray pulsar), `neb-` (nebula),
`ns` (neutron star), `s-r-`, `s-o-`, `s-y-`, `s-w-`, `s-g-`, `s-b-` (red,
orange, yellow, white, green and blue stars), `worm` (wormhole) and `rpls`
(radio pulsar). The galaxy map draws `<code>map.tga`, and `<code>maps.tga`
when zoomed out; the system view animates `<code>.ani`.

## Star names (`starname.bin`)

A 32-bit count (297), then 24-byte records: the name (20 bytes) and its
length plus one (32-bit).

## Home systems (`meplanet.bin`)

88-byte records, one per planet of the five empires' home systems, in the
order of the empires and of the planets from their star:

| Offset | Size | What |
| --- | --- | --- |
| 0 | 40 | Planet name |
| 40 | 40 | Race name |
| 80 | 4 | Not understood (0, 1 or 10) |
| 84 | 4 | Not understood; the same for all planets of a system |

## Technologies (`techtree.tec`)

72-byte records, field by field, level 0 to 10 in each:

| Offset | Size | What |
| --- | --- | --- |
| 0 | 20 | Name |
| 55 | 1 | Field: biotech, computers, construction, energy, propulsion, weapons, sociology |
| 56 | 1 | Level |
| 60 | 4 | Research points needed |
| 68 | 4 | Offset of its description in `techdesc.tec` (zero-ended texts) |

Sociology is not shown to the player.
