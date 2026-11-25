# Colonies and their economy

The numbers here come from the game's tables (see
[the table formats](../formats/tables.md)); how they work together is our own
approximation of the original's rules, to be refined as more is understood.

## Population

A colony holds a whole star system. The room for people is the sum over its
planets of the planet's room (from `planspac.bin`) times the race's liking
for the planet (`habit.bin`, a number from 0 to 10 for each empire and one
for minor races) divided by ten. Only terraformed planets count: terran
planets and all planets of the races' home systems are terraformed from the
start, the others once colony ships have terraformed them (see
[ships](ships.md#terraforming)).

While people are fed and there is room, population grows each turn by the
rate of its bracket in `ppgrowth.bin` (in ten-millionths), at least by one
million. Each four million people eat one food a turn. A surplus goes into
store, up to five turns of need; when the store runs out people starve and
the population shrinks.

## Facilities

Each colony has one kind of facility for each of food, industry, energy,
research and intelligence, of the best level its empire can build. Every
facility needs ten million workers (the building's `workers`) and makes its
`amount`. The player chooses how many work; people without work go where
they are needed (farms while food is short, power plants while energy is
short, then industry, research and intelligence). A colony can have one
facility for each ten million of its room.

Building an upgrade replaces all facilities of that kind at once and costs
the building's cost for each facility.

## Structures

Structures add to what the colony makes, by their bonus (`edifbnft.bst`):
an amount of food, energy, credits, research, intelligence or industry, or a
percentage more of it; shipyards let the colony build ships. Structures that
need energy work only while switched on and while the colony makes energy
for them, in the order they were built.

## Industry and credits

Industry pays for the first item of the build queue; what is left goes to
the next item. With nothing to build, industry is sold as trade goods: one
credit for each point. People pay one credit for each five million.
Finishing the first item at once costs two credits for each missing point of
industry.

## The start

Each empire starts with its home system at seven tenths of its room, the
technologies of every field at the level of its civilization setting
(beginning 1 to advanced 5), 1000 credits and a shipyard of its own kind.
Its people work enough farms and power plants, and the others split five to
three to two between industry, research and intelligence.

## The build list

The build list offers one more facility of each kind (while the colony has
room for more), the upgrade to a better facility once the empire's
technology allows it, and structures the empire can build that the colony
does not have yet: one shipyard to a system, structures for the home system
only there, and those that need a planet type only where there is one. Not
offered yet: structures of minor races, those for special planets (such as
dilithium) and those for conquered systems. The queue holds five items.

## Bonuses

Each terraformed planet with a food or energy bonus (see
[the galaxy](galaxy.md#bonuses)) adds 25% to its colony's food or energy,
along with the structures' percentages; the energy bonus also counts
towards powering structures. Dilithium in a system adds 25% to the
industry put into a ship there. These numbers are ours. The system screen
lists each planet's bonus and says when dilithium is present.

## Trade routes

Our own numbers, after the original's help texts: an empire may trade with
a race it has a friendship, affiliation or alliance with. Each of its
colonies may have one trade route for each full 100 million people (at
least one), plus what its structures add (bonus *Trade routes*). A route
goes from the colony to a colony of another empire or to a minor race's
home. Each turn it pays (the people at both ends) / 20 credits, raised by
the colony's *% income on trade route* structures, to the empire that set
it up, and the same to the partner when that is an empire. A minor race's
people count as half the room of its home.

A route ends when the treaty falls below friendship (war, for one) or one
end changes hands; when a colony has too many routes for its people, the
newest go. Computer players fill their free routes with the partners that
pay most.

In *Economic* mode, choosing one of the player's colonies shows its routes
under the fleets in the list on the left: a click on *Trade Routes* and then
on a system on the map sets one up, and a click on a route ends it. The
*Trade routes* layer draws the routes the player knows: their own in green,
the others' in yellow.

