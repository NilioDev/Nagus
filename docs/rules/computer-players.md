# Computer players

Our own design: the original's computer players are not described anywhere
we may use, so ours follow simple rules that give the same kind of game.
Every empire that the player does not play gives its orders at the start of
each turn, before the colonies work. The same game always gives the same
orders.

## Building

A colony with an empty build queue chooses one thing, the first of:

1. a farm, when it makes less food than its people eat;
2. a power plant, when its structures need more energy than it makes;
3. a colony ship, when the empire has fewer than two (in fleets or queues)
   and there are more free systems within reach than it has colony ships;
4. the best warship it can build, while the empire has fewer than one, plus
   one for every 40, 30, 20, 15 or 10 turns (simple to impossible);
5. a troop transport, while its fleets attack a system and it has fewer
   than two;
6. an upgrade to better facilities;
7. the cheapest structure there is spare energy for;
8. another facility: industry, research, energy, food, intelligence.

Credits over a reserve of 150 buy what is being built, colony ships first.

## Fleets

- A colony ship founds a colony where it is if it can; otherwise it goes to
  the free system within its range that is nearest and has the most room.
  Two colony ships do not go to the same system.
- Scouts go to a sector up to four steps away, within range, a different
  one each turn they are idle.
- Warships wait where they are built until the empire has 6, 5, 4, 3 or 3
  idle warships (simple to impossible); then they go to the nearest enemy
  colony it knows of within range, and attack it (see [combat](combat.md)).
- Troop transports go to a system the empire's warships are attacking, and
  join the attack.

Research keeps the starting shares. What they do in diplomacy is in
[diplomacy](diplomacy.md).
