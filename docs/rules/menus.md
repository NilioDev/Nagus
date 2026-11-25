# Menus

## Main menu (`opening.wdf`)

Nine buttons. Before a game is running only *New Single Player Game* and
*Quit Game* do something in Nagus; the others are dimmed until they work.

## Game set-up (`gmesetup.wdf`)

Each setting has a button that steps to the next value, and a text field
that shows the value (texts from the lexicon). Values and the order they
step through:

| Setting | Values | Nagus's default |
| --- | --- | --- |
| Galaxy shape | Irregular, Elliptical, Ring, Spiral | Irregular |
| Galaxy size | Small, Medium, Large | Small |
| Minor races | None, Few, Some, Many | Some |
| Difficulty | Simple, Easy, Normal, Hard, Impossible | Normal |
| Each empire's starting level | Beginning, Early, Developed, Expanded, Advanced | Beginning |
| Random events | On, Off | On |
| Tactical combat | Manual, Automatic | Manual |
| Victory | Domination, Team play, Vendetta | Domination |

The galaxy's picture beside the shape and size changes with them. The turn
timer is for multiplayer games and stays dimmed. The defaults are our best
understanding of the original, not yet checked against it; Nagus remembers
the player's last choices in its settings file.

## Empire choice (`emppick.wdf`)

Five empire buttons in the order Cardassian, Federation, Ferengi, Klingon,
Romulan. The chosen empire's button shows its lit icon, its portrait fills
the large frame and its description (from `racedesc.rst`) the text field.
The team buttons appear only for team play. *Accept* starts the game,
*Cancel* goes back to the set-up.
