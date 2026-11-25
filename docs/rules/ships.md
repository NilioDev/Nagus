# Ships and fleets

The ship classes come from `shiplist.sst` and the technologies they need
from `shiptech.sst` (see [the table formats](../formats/tables.md)). How
ships move and found colonies is our own approximation of the original's
rules.

## Building

A colony with a shipyard can build the ship classes of its empire whose
technologies the empire has. Of a line of classes (a class and the one that
improves on it), only the newest it can build is offered. Outposts,
starbases and special ships are not built yet. A finished ship starts a new
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

## Seeing

An empire sees what is within two sectors of its colonies and within one
sector of its fleets, plus their ships' scan.

What an empire has once seen stays explored: the map shows stars, names
and the empires' space only in explored sectors, and leaves the rest dark.
Fleets of other empires show only while they are seen.
