# Research

The technologies come from `techtree.tec` (see
[the table formats](../formats/tables.md)): seven fields of levels 0 to 10,
each level with the research points it needs. How research is spent is our
own approximation of the original's rules.

- Each turn an empire's colonies make research points (research facilities
  and structures, see [the economy](economy.md)).
- The player shares them between the six fields biotech, computers,
  construction, energy, propulsion and weapons, in percentages that add up
  to 100. A locked field keeps its share when another field's share
  changes; the other fields give or take in proportion to what they have.
- Each field gets its share times six, so that with equal shares each field
  gets as much as the whole. (Without this the first levels would take some
  fifty turns from a home system; the original's pace is not known.)
- A field that has gathered its next level's cost reaches that level; what
  is left over counts towards the level after.
- Sociology is not researched: it is always the average level of the six
  fields, rounded down. Research and intelligence facilities need it.
- Every field starts at the level of the empire's civilization setting.

Buildings need their field's level (see the build list in
[the economy](economy.md)).

## The databases

The research screen has two more pages, opened with its *Field Database*
and *Object Database* buttons (and *Management* to go back):

- The field database lists a field's levels with their descriptions from
  `techdesc.tec`, the points each needs and whether it is reached; *Next*
  and *Previous* change the field.
- The object database sorts the buildings into research, development,
  food, energy and defence (by their output and class) and lists the ship
  classes, with descriptions from `edifdesc.bst` and `shipdesc.sst` and a
  page of statistics (such as hull, shields, weapons, build cost and the
  research levels needed). Entries the player can already build are shown bright.
- Up and Down (or a click near the ends of a list) move through a list.
