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
| 80 | 4 | 10 for a gas giant; 0 or 1 otherwise (not understood) |
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

## Planet types and atmospheres (`environ.est`)

A 16-bit count (13), eight 32-bit floats with the share of planets of each
type (they add up to one), then from offset 0x82 thirteen 50-byte names: the
eight planet types (arctic, barren, desert, gas giant, jungle, oceanic,
terran, volcanic) and the five atmospheres (methane, none, oxygen rich,
sulfuric, thin oxygen). Elsewhere atmospheres are numbered on from the types,
8 to 12.

## Habitats (`habit.bin`)

Fourteen 8-byte records: planet type, atmosphere (8-12) and six numbers not
yet understood. They list which atmospheres each planet type can have.

## Room for people (`planspac.bin`)

Twenty-one 40-byte records, one per planet type and size (gas giants have
none): type (8 bits), size (8 bits, small, medium, large), 16 bits not
understood, then nine 32-bit numbers of people (in millions). What picks
one of the nine is not known yet; Nagus picks one at random.

## Ship classes (`shiplist.sst`, `shiptech.sst`)

`shiplist.sst` holds 288-byte records, one per ship class:

| Offset | Size | What |
| --- | --- | --- |
| 0 | 40 | Name |
| 40 | 20 | Class name |
| 84 | 4 | Offset of its description in `shipdesc.sst` |
| 88 | 8 | Picture name: `i_<name>30.tga`, `60`, `120`, `170` |
| 102 | 2 | Index |
| 104 | 2 | Role: scout, destroyer, cruiser, strike cruiser, command, colony ship, outpost, starbase, special, transport |
| 108 | 4 | Beam strength (our reading) |
| 112 | 4 | Beam count (our reading) |
| 128 | 4 | Torpedo strength (our reading) |
| 132 | 4 | Torpedo count (our reading) |
| 148 | 4 | Hull |
| 152 | 4 | Shields |
| 160 | 4 | Build cost |
| 260 | 8 | Range as a double: 0 short, 1 medium, 2 long |
| 276 | 1 | Scan |
| 278 | 2 | Capacity: colonists or troops |
| 280 | 1 | Race (0-4 the empires, then minor races; 36 monsters) |

`shiptech.sst` holds a 16-byte record for each: the race (32 bits), the
index and the index of the class it improves on (16 bits each, `0xffff` for
none), and the levels of biotech, computers, construction, energy,
propulsion and weapons it needs (a byte each).
