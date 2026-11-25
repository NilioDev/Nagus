# Intelligence

Our own rules; the game's screens show the plan, the reports and how the
empires compare.

## Points

Colonies make intelligence points with their intelligence facilities, as
they make research. Each turn an empire shares its points out:

- **Security** takes its share (0 to 100 percent) and guards the empire.
  If the empire plans no operations, all points go to security.
- The rest goes to **operations**: for each other empire, a share for
  espionage and a share for sabotage. All operation shares add up to at most
  100 percent, and the points are split between them in proportion.

Points for an operation gather from turn to turn. Once 150 have gathered
against an empire, an operation happens (one espionage and one sabotage
each turn at most) and costs 150 points. Only empires that have met can act
against each other.

## Success

The chance of success is 100 × gathered / (gathered + 2 × the target's
security points), between 10 and 90 percent. A caught operation costs 10
of the target's regard for the empire that sent it (see
[diplomacy](diplomacy.md)).

## What operations do

Each operation follows the focus set for its target.

| Focus | Espionage | Sabotage |
| --- | --- | --- |
| General | learns where the target's systems are | unrest kills 10% of a colony's people |
| Science | gains half the cost of the next level (at most 200 points) in the field where the target is furthest ahead | wipes the target's research in the field where it has gathered most |
| Economy | takes 10% of the target's credits (at most 500) | wipes the industry put into a colony's building |
| Military | finds the target's fleets | halves a ship's hull |

## Reports

The player hears of their own operations, of operations caught at home and
of sabotage felt at home. The last 60 operations are kept.

## Computer players

They keep 40 percent for security. Against an empire they are at war with
they plan 10 percent espionage and 20 sabotage, on the military or the
economy; against others 10 percent espionage on science.
