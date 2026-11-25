# Multiplayer

Multiplayer games are played over a network with TCP/IP, on a home network
or over the internet. The original's IPX, modem, serial and e-mail games
are not made. The rules are ours; the screens are the original's
`mpsetup.wdf`, `mpconn.wdf` and `mpcnet.wdf`.

## Setting up

- **Host**: the host enters their name, the game's name and an optional
  password, and presses *Host*. Nagus listens on TCP port 47621. The host
  chooses the game's settings on the game set-up screen and then waits in
  the lobby. The lobby shows the address the others join at; over the
  internet, the host's router must forward port 47621 to the host.
- **Join**: the other players enter their name, the password and the host's
  address (with `:port` when it is not 47621), and press *Connect*.
- **Lobby**: each player chooses an empire with *Empire*. An empire that
  another player has cannot be chosen. When at least two players are in
  and every player has an empire, the host presses *Start*. The computer
  plays the empires nobody chose.

A player is refused for a wrong password (the original's "Invalid Password"),
or when five players are in or the game has started.

## Turns

Every player plays their turn on their own copy of the game. Pressing
*Turn* sends the player's orders to the host and waits for the others.
While waiting, the screen shows whose orders the turn waits for. When every
player's orders are in, the host does the following:

1. It takes each player's orders into its game. These are:
   - their colonies (workers, structures, building, buying);
   - where their fleets go and their orders;
   - their research and intelligence plans;
   - the credits they spent or gave;
   - the treaties and regard their diplomacy changed;
   - the proposals they made or answered.
2. It ends the turn once for each player, from the same game, so the galaxy
   comes out the same each time. Each player gets their own turn report.
3. It sends every player the game after the turn, and their report.

Battles are always resolved automatically in multiplayer games, so no
player waits for another's battle.

A proposal from one player to another waits for the other player's answer,
like a computer player's proposal does. It lapses after a turn.

## Chat

In a game, the players can talk in the original's chat window (F2, the
gamepad's Y button, or a click on a line shown at the top of the screen).
A line goes to the host, which sends it on to every player with the name and
empire of who said it; each player sees their own line when it comes back.
A line is at most 160 printable characters. *Ignore* under an empire's
picture hides what that player says from then on, on this computer only.
The last 200 lines are kept while the game runs.

## Leaving

When a player leaves, the computer plays their empire from then on, and the
others see a short message. When the host leaves, the game cannot go on,
and the other players are told so.

A multiplayer game is not saved, saved each turn or retired from.
