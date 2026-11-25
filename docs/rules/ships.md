# Ships and fleets

The ship classes come from `shiplist.sst` and the technologies they need
from `shiptech.sst` (see [the table formats](../formats/tables.md)). How
ships move and found colonies is our own approximation of the original's
rules.

## Building

A colony with a shipyard can build the ship classes of its empire whose
technologies the empire has. Of a line of classes (a class and the one that
improves on it), only the newest it can build is offered. Outposts and
starbases are built by troop transports (see [Stations](#stations));
special ships are not built. A finished ship starts a new
fleet in the colony's sector.

Every empire starts with its cheapest colony ship and its cheapest scout at
home.

## Moving

A fleet's speed, in sectors a turn, is that of its slowest ship by role:
scouts 3; destroyers, cruisers and strike cruisers 2; command ships, colony
ships and transports 1; plus one for every four levels of propulsion.

A fleet may go as far from the empire's nearest colony as its shortest
ranged ship allows: short 3 sectors, medium 6, long 10, plus one for every
three levels of propulsion. Fleets go in a straight line, one sector a step.

## Colonies

A fleet with a colony ship in a sector with a star that has room for its
people, and no colony or minor race, can be told to colonize. On the next
turn the colony ship becomes a colony of ten million people for each point
of its capacity, with a farm and a factory of the best kind the empire can
build.

## Orders

The fleet panel below the map gives a fleet its orders. These rules are our
own reading of the original's help texts:

- *Redeploy*: while it is lit, a click on a ship in the panel takes it out
  into a fleet of its own, and a click on another of the player's fleets in
  the list on the left joins that fleet's ships to the chosen one. A fleet
  keeps at least one ship.
- *Avoid* and *Engage*: a fleet that avoids battles never starts one; it
  fights only when an enemy that engages (or a system's defences) finds it.
  Fleets engage unless told otherwise.
- *Raid*: at an enemy empire's colony, with at least one warship. At the end
  of the turn, after the battles, a fleet still there takes 25 credits per
  warship from the colony's owner, up to what they have.
- *Scrap*: the fleet is dismantled at the end of the turn, before it moves
  or fights. Nothing is paid back.
- *Cloak* (beside *Redeploy*): a fleet whose ships all have a cloaking
  device (bit 4 at offset 176 of `shiplist.sst`: the Romulans' ships and
  some of the Klingons') can cloak and decloak. Other empires do not see a
  cloaked fleet, and it takes no part in battles and does not defend a
  colony. Ordering it to attack, raid or intercept decloaks it, and a ship
  without a cloak that joins it shows the fleet.
- *Intercept*: with at least one warship. The fleet stays where it is, and
  after all fleets have moved it may catch one enemy fleet it can see that
  stopped in a neighbouring sector: it moves there and the battle is
  fought. The chance, shown in the fleet panel, is 40% plus 15% for each
  sector of speed, up to 95%. The order stays until the fleet is moved.
- *Enter Wormhole*: in a wormhole's sector. At the end of the turn, before
  fleets move, the fleet comes out at the wormhole's other end. Our choice:
  the wormholes are paired in the order of the systems; an odd one out has
  no endpoint, and a fleet that enters it is lost.
- *Train Crew*: at one of the empire's colonies with a working structure
  that gives ship experience (a military or naval academy). Each turn the
  crews gain 10 points of experience, up to 60.

- *Build Outpost*, *Build Starbase* and *Upgrade*: see
  [Stations](#stations).

Moving a fleet cancels its raid, scrap, intercept, training or station
order.

## Stations

Outposts and starbases are stations: they stay in their sector and have the
hull, shields and weapons of their class in the ship table (roles outpost
and starbase). As the original's help texts say, troop transports build
them:

- *Build Outpost*: in a sector with no station, when the empire knows the
  technologies of an outpost class.
- *Build Starbase*: at one of the empire's outposts, when it knows those of
  a starbase class. The starbase takes the outpost's place.
- *Upgrade*: at one of the empire's stations, when a better class of the
  same kind (the one that costs most) can be built.

Our choice for how long it takes: each troop transport in the fleet puts
150 industry a turn into the work, until it reaches the class's cost (for
an upgrade, the difference in cost). Then the oldest troop transport is
used up to man the station, and the order ends. The work is kept while the
fleet stays.

A station counts like a colony for range (fleets may go as far from it as
from a colony) and for seeing (two sectors). It fights in battles in its
sector like a ship, but never starts one by moving; one whose hull is gone
is destroyed. A starbase mends its empire's ships in its sector fully each
turn, as a colony does; stations mend a tenth of their own hull a turn. The
map shows each station's small picture in its sector.

Computer players' troop transports with no attack to join build an outpost
at the colony where they are, then turn it into a starbase and upgrade it.

## Crew experience

Each ship's crew has experience from 0 to 100, in five levels of 20 points:
green, regular, veteran, elite and legendary (shown under each ship in the
fleet panel). New ships start green. A crew gains 5 points from every
battle its ship lives through, and up to 60 by training. Each level above
green adds a twentieth to the ship's firepower in battle. These numbers are
our own.

## Seeing

An empire sees what is within two sectors of its colonies and within one
sector of its fleets, plus their ships' scan.

What an empire has once seen stays explored: the map shows stars, names
and the empires' space only in explored sectors, and leaves the rest dark.
Fleets of other empires show only while they are seen.
