# Combat

Our own rules, in the spirit of the original's combat. Battles between
computer players, and all battles when tactical combat is set to
*Automatic* in the game set-up, are resolved automatically as below. With
*Manual*, the player fights their own battles on the combat screen (see
[Tactical combat](#tactical-combat)).

## Who fights

Races fight when they are at war (see [diplomacy](diplomacy.md)). After
fleets move, every sector where ships or
a colony's defences of enemies meet, and one of them can shoot, has a
battle. Unarmed ships (colony ships, transports) can be shot but start no
battle. Cloaked fleets take no part (see [ships](ships.md#orders)).

## A battle

A battle lasts three rounds. In each round every armed ship, and a colony
with orbital batteries, fires at an enemy chosen at random, all at the same
time:

- A ship's firepower is beams × beam damage plus torpedoes × torpedo damage
  from the ship table, divided by five, raised by a tenth for each level of
  weapons technology, and by a twentieth for each level of its crew's
  experience above green (see [ships](ships.md#crew-experience)). Between
  half and four fifths of it hits.
- A ship's shields absorb three times their rating, raised by a tenth for
  each level of energy technology, for the whole battle; then the hull takes
  the damage. A ship with no hull left is destroyed.
- Orbital batteries fire with their bonus (80 each) raised by weapons
  technology; a shield generator gives the colony shields of its bonus times
  the energy technology level. Colonies are not destroyed in battle.

Damage stays: ships mend fully at their own colonies and by a tenth of their
hull each turn elsewhere.

## Attacking a system

A fleet in a sector with an enemy colony can be ordered to *Attack System*.
Each turn, as long as no armed ships of the colony's owner are in the
sector, all of one empire's attacking fleets there act together:

- Warships bombard: one million people die for each 100 points of firepower
  that the colony's shields do not stop.
- Troop transports land, and are used up. Their troops (the ship's capacity,
  raised by ground combat structures) fight the people: ten for each million,
  raised by ground defence structures. If the troops are more, the colony
  changes hands and loses a part of its people; if not, the troops kill a
  twentieth of their number in millions.

A colony with no people left is gone, and its system is free to colonize
again. The player is told of each battle they fought in and each attack on
or by them in the list on the left after the turn.

## Tactical combat

With tactical combat set to *Manual*, ending the turn stops after the
fleets have moved: every battle the player takes part in opens the combat
screen (the original's `<x>tactscn.wdf` layout), one after the other. The
rest of the turn (the other battles, attacks on systems, research and so
on) follows once they are fought. The battle shows in 3D with the game's
own models (see [models](../formats/models.md)): the player's ships on the
left facing right, the others' on the right, the camera swinging slowly
above the player's side, with the fire of the last round as beams and a
fireball for each ship destroyed. A ship's size follows its hull (the
models' own sizes differ too much to use), stations are larger. *Playback*
and *Tactical* (or the shoulder buttons) switch between this view and a
flat one, the sides in rows seen from above.

In the 3D view the camera panel on the left (`<x>butil.wdf`) turns the
camera left and right, raises and lowers it, zooms in and out, puts it back
(*Reset*), opens the list of views (`<x>b_views.wdf`), hides or shows the
names and hull bars (*Status*), and hands the rest of the battle to the
computer (*Auto*). Dragging across the view turns and tilts the camera too.
The views: *Top* (high above), *Chase* (behind one of the player's ships),
*Reverse* (before the player's ships, looking back at them), *From Target*
(behind the enemy) and *Towards Target* (on one of the player's ships,
looking at the enemy).

Each round plays as a tape once it is fought: the shots one after the
other (one every 220 ms, each beam fading over 700 ms), the damage
showing as each ship takes its last hit, and a destroyed ship's fireball
after it. The buttons under the camera panel (`<x>bvcr.wdf`) play the
round again: to the start, rewind (three times as fast), stop or play
(they share a place, only one shows), fast forward (three times) and to
the end. A bar under them shows how far the tape has run.

First the player chooses (`<x>fiskdlg.wdf`, which says where the battle
is and how many ships each side has) to *Fight* (give orders), to let the
battle be fought automatically (*Auto*: as below, the player's ships
choosing as the computer would) or to *Hail*: the player's ships open
hailing frequencies in the first round, and if that does not end the
battle, the computer fights the rest of it for them. Each round, the player gives all
their ships one maneuver and presses *Turn*. The maneuvers and what they are
good against are the original's (their names and help come from the
lexicon); the numbers are ours:

| Maneuver | What it does |
| --- | --- |
| Assault, Charge | Straight at the enemy |
| Circle, Harry | Around the enemy |
| Flyby, Strafe | Past the enemy |
| Evade | Gives a quarter of the damage, takes half |
| Ram | Rams an enemy ship: both take half the rammer's full hull, past shields |
| Retreat | Fires nothing, takes three quarters of the damage, then leaves |
| Open hailing frequencies | Fires nothing; if every side hails, the battle is avoided |

Straight beats past, around beats straight, past beats around: fire from a
maneuver that works well against the target's does half as much again, and
fire against one it works poorly against does half. Charge, Harry and Strafe
are at close range: a quarter more damage, given and taken. A round works
like a round of the automatic battle otherwise (same firepower, shields and
chance). Colonies' defences always fire straight.

The computer's sides retreat when they have less than a third of their
enemies' strength (firepower, hull and shields), hail in the first round
when weaker, and otherwise attack, half the time with the maneuver that
works well against what the strongest enemy did last round.

The battle ends when no one left can shoot at an enemy, or after eight
rounds. Ships that retreated move to the neighbouring sector nearest their
empire's colonies, with their orders cleared.
