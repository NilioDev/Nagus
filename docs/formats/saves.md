# Saved games

Nagus saves games in its own format, not the original's. A saved game holds
only what changes while playing; the tables (buildings, ships, techs) come
from the player's game files when the game is loaded, so a saved game holds
nothing from the original files except names it showed (systems, planets,
races).

The twelve places in the list are the files `slot01.sav` to `slot12.sav` in
the `saves` folder beside the settings file. A save is written to
`slotNN.sav.part` first and then renamed, so a failed save leaves the old
game.

## Layout

Little-endian. `i32`/`u32` are 4 bytes, `u8` one byte; `text` is a `u16`
length and that many bytes; `sector` is two `i32` (x, y). A list is a `u32`
count and then its entries.

| What | Type |
| --- | --- |
| Magic `NAGUSSAV` | 8 bytes |
| Version (1) | u32 |
| Name shown in the list | text |
| Description shown in the list | text |
| Turn | i32 |
| Galaxy shape, size, minor races, difficulty, victory, empire | 6 × i32 |
| Starting level of each empire | 5 × i32 |
| Random events, automatic combat | 2 × u8 |
| The player's empire | i32 |
| Star kind codes | list of text |
| Race names | list of text |
| Map width, height | 2 × i32 |
| Home system of each empire (or -1) | 5 × i32 |
| Star systems | list, see below |
| Colonies | list, see below |
| Each empire's state | 5 × see below |
| Next fleet number | i32 |
| Fleets | list, see below |

A star system: sector, kind (i32), name (text), race (i32, -1 for none),
has a star (u8), then a list of planets: name (text), type, atmosphere,
size, room, picture variant (6 × i32).

A colony: system, owner, population, room, food in store, industry put into
the first item of the queue (6 × i32); home (u8); for each of the five
outputs the facility building, how many and how many are worked (3 × i32);
a list of structures (building i32, switched on u8); the build queue as a
list of kind (u8: 0 facility, 1 upgrade, 2 structure, 3 ship) and building
or ship class (i32).

An empire: credits (i32), level in each of the seven fields (7 × i32),
research towards the next level (7 × i32), share of research for the six
fields (6 × i32), locked shares (6 × u8), research and intelligence points
(2 × i32).

A fleet: number, owner (2 × i32), sector, under way (u8), destination
(sector), order (u8: 0 none, 1 colonize), and a list of ships: class, hull
(2 × i32).

## Reading

The reader checks every count, number and index against limits: building
and ship numbers must be in the player's tables, sectors on the map,
systems in the list, and nothing may follow the last fleet. A file that
fails any check is shown as unreadable and cannot be loaded. Files over
64 MB are not read.
