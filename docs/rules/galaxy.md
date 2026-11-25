# The galaxy

How the original lays out its galaxy is not known in detail yet; what Nagus
does now is its own approximation, to be replaced as the original's
behaviour is pinned down.

## Size

| Size | Sectors (our guess) |
| --- | --- |
| Small | 20 × 15 |
| Medium | 30 × 22 |
| Large | 40 × 30 |

Each sector holds at most one object: a star (red, orange, yellow, white,
green or blue), a nebula, a black hole, a neutron star, an X-ray or radio
pulsar, or a wormhole. About 18% of the sectors inside the galaxy's shape
hold one, each kind with the share that `objstruc.smt` gives it.

## Shape

- **Elliptical:** objects thin out from the middle to the edge.
- **Ring:** a band around an empty middle.
- **Spiral:** a core with two arms.
- **Irregular:** six to nine clumps at random places.

Objects next to one just placed are less likely, so the map does not clump
too tightly.

## Homes

The five empires' home systems are picked as far from each other as the map
allows, not right at the edge, and named from the lexicon (Cardassia, Sol,
Ferenginar, Qo'nos, Romulus). Minor races get homes on other stars, at
least three sectors from any empire, as many as the set-up asks for:

| Minor races | Small | Medium | Large |
| --- | --- | --- | --- |
| Few | 5 | 8 | 12 |
| Some | 10 | 15 | 20 |
| Many | 15 | 22 | 29 |

The other stars get names from `starname.bin` at random.

## The same galaxy everywhere

A galaxy is made from a seed with a random number generator that gives the
same numbers on every system (`game/random.hpp`), so a seed always makes
the same galaxy.

## The map

The main screen (`<x>maingal.wdf`, one per empire) shows the map in element
`0x3f9`, 25 pixels per sector up close; the Map button shows the whole
galaxy at once with the small pictures. The panel below the map
(`<x>_starmp.wdf`, placed in element `0x3f6`) names the chosen sector and
what is in it. Sectors are named "column.row", counted from 1 (our guess).

## Map modes and layers

The buttons below the map choose its mode: *Economic*, *Military* or
*Events*. Each mode keeps its own choice of layers from the *Overlay* menu:
system names, the edges of the player's ship ranges (short green, medium
yellow, long red), the paths of the player's moving fleets, colony
defences (firepower, and "+" with shields), territory, trade routes, what
the player's colonies build, each colony's population, and the systems'
bonuses (top left of the sector: D for dilithium, F for a food and E for an
energy bonus). Economic starts with names, territory, trade routes, build
orders and population; Military with names, territory, ranges, paths and
defences; Events with names, territory and paths. In Events mode the panel
below the map lists last turn's events. Below the map, a dot before a
planet's name marks its bonus (green food, yellow energy).

## Bonuses

The original's galaxies have dilithium and planets with food and energy
bonuses; how it places them (`planboni.bin` may hold the odds) is not
known, so Nagus places them its own way, from a random stream of their own
so that the rest of a galaxy is the same as without them:

- Dilithium in each empire's home system and in a fifth of the other star
  systems.
- A food bonus on 15% of jungle, oceanic and terran planets; an energy
  bonus on 12% of barren, desert, gas giant and volcanic planets.

What they do is in [the economy](economy.md#bonuses).
