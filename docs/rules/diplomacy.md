# Diplomacy

Our own rules. The race table gives each race how it first regards each
empire and how warlike it is (see [tables](../formats/tables.md)); the rest
is our design.

## Meeting

Races start unmet. An empire meets a race once it has explored a sector
with one of the race's colonies or its home system, or sees one of its
fleets. They are then *neutral*.

## Treaties

War, neutral, non-aggression, friendship, affiliation and alliance between
empires; friendship, affiliation and membership with minor races. Only
races at war fight (see [combat](combat.md)).

A race agrees to a treaty when its regard for the empire is high enough.
For empires the regard needed is 70 for non-aggression, 90 for friendship,
115 for affiliation and 140 for alliance (which needs friendship first),
times the race's aggression (0.8 for the Ferengi to 1.2 for the
Klingons). From war, non-aggression is peace and needs only 30, and after
20 turns of war each turn adds one more, up to 40.

A minor race needs 75 for friendship, then 100 for affiliation, then 130
and ten turns of affiliation for membership. A minor race that joins an
empire becomes one of its colonies, with the race's people and the best
facilities the empire can build. Once it has joined an empire, no other
can win it over.

## Regard

Regard starts at the race table's value and stays between 0 and 200.

- Friendship or better: one more every two turns.
- War: one less every two turns.
- Otherwise it goes back, one every four turns, towards the race's first
  regard divided by its aggression.
- Empires with colonies within three sectors of each other: one less
  every four turns.
- A battle between two races: five less each way; an attack on a system:
  fifteen less for the defender.
- A declaration of war: thirty less for the race it is declared on.
- A gift: one more for every 25 credits, at most ten.
- After peace, both think of each other at least 50.
- A proposal the player rejects or leaves: five less.

## Computer players

Each turn a computer player, for each empire it has met:

- at war: proposes peace once its regard plus its weariness (see above)
  reaches 25;
- neutral (or under a non-aggression treaty it has kept for 30 turns, with
  regard under 50): declares war when its regard is under 60 times its
  aggression, it is at war with nobody, it is turn 20 or later, and it has
  as many idle warships as it needs to attack (see
  [computer players](computer-players.md));
- otherwise proposes the best treaty it thinks the other would sign.

It proposes treaties to minor races as soon as they would agree, and every
five turns, with more than 600 credits, gives 200 to the minor race that
thinks best of it.

Proposals to the player wait in the diplomacy screen's *Active* page
until the end of the turn; then they lapse.
